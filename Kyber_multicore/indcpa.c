#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "params.h"
#include "indcpa.h"
#include "polyvec.h"
#include "poly.h"
#include "ntt.h"
#include "symmetric.h"
#include "randombytes.h"
#include "pico/multicore.h"
#include <stdio.h>
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

typedef struct
{
  uint8_t *buf;
  polyvec *a;
} core1_hash_data_t;

typedef struct
{
  polyvec *a;
  polyvec *pkpv;
  polyvec *skpv;
  unsigned int start;
  unsigned int end;
} core1_mul_data_t;

typedef struct
{
  uint8_t *pk;
  polyvec *pkpv;
  const uint8_t *publicseed;
} core1_pack_data_t;

typedef struct
{
  polyvec *at; // input array
  polyvec *b;  // output array
  polyvec *sp; // secret vector
  unsigned int start;
  unsigned int end;
} core1_mul_data_enc_t;

typedef struct
{
  poly *k;
  const uint8_t *m;
} core1_frommsg_data_t;

/*************************************************
 * Name:        pack_pk
 *
 * Description: Serialize the public key as concatenation of the
 *              serialized vector of polynomials pk
 *              and the public seed used to generate the matrix A.
 *
 * Arguments:   uint8_t *r: pointer to the output serialized public key
 *              polyvec *pk: pointer to the input public-key polyvec
 *              const uint8_t *seed: pointer to the input public seed
 **************************************************/
static void pack_pk(uint8_t r[KYBER_INDCPA_PUBLICKEYBYTES],
                    polyvec *pk,
                    const uint8_t seed[KYBER_SYMBYTES])
{
  polyvec_tobytes(r, pk);
  memcpy(r + KYBER_POLYVECBYTES, seed, KYBER_SYMBYTES);
}

/*************************************************
 * Name:        unpack_pk
 *
 * Description: De-serialize public key from a byte array;
 *              approximate inverse of pack_pk
 *
 * Arguments:   - polyvec *pk: pointer to output public-key polynomial vector
 *              - uint8_t *seed: pointer to output seed to generate matrix A
 *              - const uint8_t *packedpk: pointer to input serialized public key
 **************************************************/
static void unpack_pk(polyvec *pk,
                      uint8_t seed[KYBER_SYMBYTES],
                      const uint8_t packedpk[KYBER_INDCPA_PUBLICKEYBYTES])
{
  polyvec_frombytes(pk, packedpk);
  memcpy(seed, packedpk + KYBER_POLYVECBYTES, KYBER_SYMBYTES);
}

/*************************************************
 * Name:        pack_sk
 *
 * Description: Serialize the secret key
 *
 * Arguments:   - uint8_t *r: pointer to output serialized secret key
 *              - polyvec *sk: pointer to input vector of polynomials (secret key)
 **************************************************/
static void pack_sk(uint8_t r[KYBER_INDCPA_SECRETKEYBYTES], polyvec *sk)
{
  polyvec_tobytes(r, sk);
}

/*************************************************
 * Name:        unpack_sk
 *
 * Description: De-serialize the secret key; inverse of pack_sk
 *
 * Arguments:   - polyvec *sk: pointer to output vector of polynomials (secret key)
 *              - const uint8_t *packedsk: pointer to input serialized secret key
 **************************************************/
static void unpack_sk(polyvec *sk, const uint8_t packedsk[KYBER_INDCPA_SECRETKEYBYTES])
{
  polyvec_frombytes(sk, packedsk);
}

/*************************************************
 * Name:        pack_ciphertext
 *
 * Description: Serialize the ciphertext as concatenation of the
 *              compressed and serialized vector of polynomials b
 *              and the compressed and serialized polynomial v
 *
 * Arguments:   uint8_t *r: pointer to the output serialized ciphertext
 *              poly *pk: pointer to the input vector of polynomials b
 *              poly *v: pointer to the input polynomial v
 **************************************************/
static void pack_ciphertext(uint8_t r[KYBER_INDCPA_BYTES], polyvec *b, poly *v)
{
  polyvec_compress(r, b);
  poly_compress(r + KYBER_POLYVECCOMPRESSEDBYTES, v);
}

/*************************************************
 * Name:        unpack_ciphertext
 *
 * Description: De-serialize and decompress ciphertext from a byte array;
 *              approximate inverse of pack_ciphertext
 *
 * Arguments:   - polyvec *b: pointer to the output vector of polynomials b
 *              - poly *v: pointer to the output polynomial v
 *              - const uint8_t *c: pointer to the input serialized ciphertext
 **************************************************/
static void unpack_ciphertext(polyvec *b, poly *v, const uint8_t c[KYBER_INDCPA_BYTES])
{
  polyvec_decompress(b, c);
  poly_decompress(v, c + KYBER_POLYVECCOMPRESSEDBYTES);
}

/*************************************************
 * Name:        rej_uniform
 *
 * Description: Run rejection sampling on uniform random bytes to generate
 *              uniform random integers mod q
 *
 * Arguments:   - int16_t *r: pointer to output buffer
 *              - unsigned int len: requested number of 16-bit integers (uniform mod q)
 *              - const uint8_t *buf: pointer to input buffer (assumed to be uniformly random bytes)
 *              - unsigned int buflen: length of input buffer in bytes
 *
 * Returns number of sampled 16-bit integers (at most len)
 **************************************************/
static unsigned int rej_uniform(int16_t *r,
                                unsigned int len,
                                const uint8_t *buf,
                                unsigned int buflen)
{
  unsigned int ctr, pos;
  uint16_t val0, val1;

  ctr = pos = 0;
  while (ctr < len && pos + 3 <= buflen)
  {
    val0 = ((buf[pos + 0] >> 0) | ((uint16_t)buf[pos + 1] << 8)) & 0xFFF;
    val1 = ((buf[pos + 1] >> 4) | ((uint16_t)buf[pos + 2] << 4)) & 0xFFF;
    pos += 3;

    if (val0 < KYBER_Q)
      r[ctr++] = val0;
    if (ctr < len && val1 < KYBER_Q)
      r[ctr++] = val1;
  }

  return ctr;
}

#define gen_a(A, B) gen_matrix(A, B, 0)
#define gen_at(A, B) gen_matrix(A, B, 1)

/*************************************************
 * Name:        gen_matrix
 *
 * Description: Deterministically generate matrix A (or the transpose of A)
 *              from a seed. Entries of the matrix are polynomials that look
 *              uniformly random. Performs rejection sampling on output of
 *              a XOF
 *
 * Arguments:   - polyvec *a: pointer to ouptput matrix A
 *              - const uint8_t *seed: pointer to input seed
 *              - int transposed: boolean deciding whether A or A^T is generated
 **************************************************/
#if (XOF_BLOCKBYTES % 3)
#error "Implementation of gen_matrix assumes that XOF_BLOCKBYTES is a multiple of 3"
#endif

#define GEN_MATRIX_NBLOCKS ((12 * KYBER_N / 8 * (1 << 12) / KYBER_Q + XOF_BLOCKBYTES) / XOF_BLOCKBYTES)
// Not static for benchmarking
void gen_matrix(polyvec *a, const uint8_t seed[KYBER_SYMBYTES], int transposed)
{
  unsigned int ctr, i, j;
  unsigned int buflen;
  uint8_t buf[GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES];
  xof_state state;

  for (i = 0; i < KYBER_K; i++)
  {
    for (j = 0; j < KYBER_K; j++)
    {
      if (transposed)
        xof_absorb(&state, seed, i, j);
      else
        xof_absorb(&state, seed, j, i);

      xof_squeezeblocks(buf, GEN_MATRIX_NBLOCKS, &state);
      buflen = GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES;
      ctr = rej_uniform(a[i].vec[j].coeffs, KYBER_N, buf, buflen);

      while (ctr < KYBER_N)
      {
        xof_squeezeblocks(buf, 1, &state);
        buflen = XOF_BLOCKBYTES;
        ctr += rej_uniform(a[i].vec[j].coeffs + ctr, KYBER_N - ctr, buf, buflen);
      }
    }
  }
}

/*
  - Below are the functions which are to be sent to core1 for the keypair derand function
*/

void core1_hash_worker()
{
  // Wait for data from core 0
  core1_hash_data_t *data = (core1_hash_data_t *)multicore_fifo_pop_blocking();

  // Core 1 does hashing and gen_a
  const uint8_t *publicseed = data->buf;
  hash_g(data->buf, data->buf, KYBER_SYMBYTES + 1);
  gen_a(data->a, publicseed);

  // Signal completion to core 0
  multicore_fifo_push_blocking(1);
}

void core1_mul_worker()
{
  core1_mul_data_t *data = (core1_mul_data_t *)multicore_fifo_pop_blocking();

  unsigned int start = data->start;
  unsigned int end = data->end;

  for (unsigned int i = start; i < end; i++)
  {
    polyvec_basemul_acc_montgomery(&data->pkpv->vec[i], &data->a[i], data->skpv);
    poly_tomont(&data->pkpv->vec[i]);
  }

  // Signal completion to core0
  multicore_fifo_push_blocking(1);
}

void core1_pack_worker()
{
  core1_pack_data_t *data = (core1_pack_data_t *)multicore_fifo_pop_blocking();

  // Core1 does pack_pk
  pack_pk(data->pk, data->pkpv, data->publicseed);

  // Signal completion to core0
  multicore_fifo_push_blocking(1);
}

/*************************************************
 * Name:        indcpa_keypair_derand
 *
 * Description: Generates public and private key for the CPA-secure
 *              public-key encryption scheme underlying Kyber
 *
 * Arguments:   - uint8_t *pk: pointer to output public key
 *                             (of length KYBER_INDCPA_PUBLICKEYBYTES bytes)
 *              - uint8_t *sk: pointer to output private key
 *                             (of length KYBER_INDCPA_SECRETKEYBYTES bytes)
 *              - const uint8_t *coins: pointer to input randomness
 *                             (of length KYBER_SYMBYTES bytes)
 **************************************************/

void indcpa_keypair_derand(uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                           uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES],
                           const uint8_t coins[KYBER_SYMBYTES])
{
  unsigned int i;
  uint8_t buf[2 * KYBER_SYMBYTES];
  const uint8_t *publicseed = buf;
  const uint8_t *noiseseed = buf + KYBER_SYMBYTES;
  static polyvec a[KYBER_K];
  static polyvec e, pkpv, skpv;

  memcpy(buf, coins, KYBER_SYMBYTES);
  buf[KYBER_SYMBYTES] = KYBER_K;

  // Launch core 1 worker for hashing
  multicore_launch_core1(core1_hash_worker);

  // Prepare and send data for core 1
  static volatile core1_hash_data_t core1_data;
  core1_data.buf = buf;
  core1_data.a = a;
  multicore_fifo_push_blocking((uintptr_t)&core1_data);

  // Meanwhile, core 0 can generate noise in parallel
  uint8_t nonce = 0;
  for (i = 0; i < KYBER_K; i++)
    poly_getnoise_eta1(&skpv.vec[i], noiseseed, nonce++);

  for (i = 0; i < KYBER_K; i++)
    poly_getnoise_eta1(&e.vec[i], noiseseed, nonce++);

  polyvec_ntt(&skpv);
  polyvec_ntt(&e);

  // Wait for core 1 to finish hash & gen_a
  multicore_fifo_pop_blocking();
  multicore_reset_core1();

  // Parallelisation across k vector lanes
  unsigned int half = KYBER_K / 2; // floor division
  unsigned int core1_start = half;
  unsigned int core1_end = KYBER_K;
  unsigned int core0_start = 0;
  unsigned int core0_end = half;

  // Prepare work packet for core1
  static volatile core1_mul_data_t mul_data;
  mul_data.a = a;
  mul_data.pkpv = &pkpv;
  mul_data.skpv = &skpv;
  mul_data.start = core1_start;
  mul_data.end = core1_end;

  // Launch worker on core1 for multiplication
  multicore_launch_core1(core1_mul_worker);
  multicore_fifo_push_blocking((uintptr_t)&mul_data);

  // Core 0 processes the first half
  for (i = core0_start; i < core0_end; i++)
  {
    polyvec_basemul_acc_montgomery(&pkpv.vec[i], &a[i], &skpv);
    poly_tomont(&pkpv.vec[i]);
  }

  // Wait for core1 to finish before proceeding
  multicore_fifo_pop_blocking();
  multicore_reset_core1();

  // Securely zeroise 'a' after use
  memset(a, 0, sizeof(a));

  polyvec_add(&pkpv, &pkpv, &e);
  polyvec_reduce(&pkpv);

  // Securely zeroise 'e' after use
  memset(e.vec, 0, sizeof(e.vec));

  // Launch core1 worker for packing
  static volatile core1_pack_data_t pack_data;
  pack_data.pk = pk;
  pack_data.pkpv = &pkpv;
  pack_data.publicseed = publicseed;

  multicore_launch_core1(core1_pack_worker);
  multicore_fifo_push_blocking((uintptr_t)&pack_data);

  // Core0 packs secret key in parallel
  pack_sk(sk, &skpv);

  // Securely zeroise 'pkpv' after use
  memset(pkpv.vec, 0, sizeof(pkpv.vec));

  // Wait for core1
  multicore_fifo_pop_blocking();
  multicore_reset_core1();

  // Finally, zeroise the secret key
  memset(skpv.vec, 0, sizeof(skpv.vec));
}


/*
  - Below are the functions which are to be sent to core1 for the enc function
*/

void core1_mul_worker_enc()
{
  core1_mul_data_enc_t *data = (core1_mul_data_enc_t *)multicore_fifo_pop_blocking();
  unsigned int start = data->start;
  unsigned int end = data->end;

  for (unsigned int i = start; i < end; i++)
  {
    polyvec_basemul_acc_montgomery(&data->b->vec[i], &data->at[i], data->sp);
  }

  // Signal completion
  multicore_fifo_push_blocking(1);
}

void core1_frommsg_worker()
{
  core1_frommsg_data_t *data = (core1_frommsg_data_t *)multicore_fifo_pop_blocking();
  poly_frommsg(data->k, data->m);
  multicore_fifo_push_blocking(1); // signal completion
}

/*************************************************
 * Name:        indcpa_enc
 *
 * Description: Encryption function of the CPA-secure
 *              public-key encryption scheme underlying Kyber.
 *
 * Arguments:   - uint8_t *c: pointer to output ciphertext
 *                            (of length KYBER_INDCPA_BYTES bytes)
 *              - const uint8_t *m: pointer to input message
 *                                  (of length KYBER_INDCPA_MSGBYTES bytes)
 *              - const uint8_t *pk: pointer to input public key
 *                                   (of length KYBER_INDCPA_PUBLICKEYBYTES)
 *              - const uint8_t *coins: pointer to input random coins used as seed
 *                                      (of length KYBER_SYMBYTES) to deterministically
 *                                      generate all randomness
 **************************************************/

void indcpa_enc(uint8_t c[KYBER_INDCPA_BYTES],
                const uint8_t m[KYBER_INDCPA_MSGBYTES],
                const uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                const uint8_t coins[KYBER_SYMBYTES])
{
  unsigned int i;
  uint8_t seed[KYBER_SYMBYTES];
  uint8_t nonce = 0;
  static polyvec sp, pkpv, ep, at[KYBER_K], b;
  static poly v, k, epp;

  // Launch core1 for poly_frommsg
  static volatile core1_frommsg_data_t frommsg_data;
  frommsg_data.k = &k;
  frommsg_data.m = m;

  multicore_launch_core1(core1_frommsg_worker);
  multicore_fifo_push_blocking((uintptr_t)&frommsg_data);

  // Meanwhile, Core0 does unpack_pk
  unpack_pk(&pkpv, seed, pk);

  // Wait for core1
  multicore_fifo_pop_blocking();
  multicore_reset_core1();

  gen_at(at, seed);

  for (i = 0; i < KYBER_K; i++)
    poly_getnoise_eta1(sp.vec + i, coins, nonce++);
  for (i = 0; i < KYBER_K; i++)
    poly_getnoise_eta2(ep.vec + i, coins, nonce++);
  poly_getnoise_eta2(&epp, coins, nonce++);
  
  polyvec_ntt(&sp);


  // Compute ranges for splitting
  unsigned int half = KYBER_K / 2;
  unsigned int core1_start = half;
  unsigned int core1_end = KYBER_K;
  unsigned int core0_start = 0;
  unsigned int core0_end = half;

  // Setup data packet for core1
  static volatile core1_mul_data_enc_t mul_data2;
  mul_data2.at = at;
  mul_data2.b = &b;
  mul_data2.sp = &sp;
  mul_data2.start = core1_start;
  mul_data2.end = core1_end;

  // Launch multiplication worker on core1
  multicore_launch_core1(core1_mul_worker_enc);
  multicore_fifo_push_blocking((uintptr_t)&mul_data2);

  // Core0 executes its portion
  for (i = core0_start; i < core0_end; i++)
  {
    polyvec_basemul_acc_montgomery(&b.vec[i], &at[i], &sp);
  }

  polyvec_basemul_acc_montgomery(&v, &pkpv, &sp);

  // Wait for core1 and cleanup
  multicore_fifo_pop_blocking();
  multicore_reset_core1();

  polyvec_invntt_tomont(&b);
  poly_invntt_tomont(&v);

  polyvec_add(&b, &b, &ep);
  poly_add(&v, &v, &epp);
  poly_add(&v, &v, &k);
  polyvec_reduce(&b);
  poly_reduce(&v);

  // Securely zeroise all used buffers before packing
  memset(at, 0, sizeof(at));  // Zeroise gen_at-related buffer
  memset(&sp, 0, sizeof(sp));  // Zeroise sp
  memset(&ep, 0, sizeof(ep)); // Zeroise ep
  memset(&epp, 0, sizeof(epp));
  memset(&pkpv, 0, sizeof(pkpv));  // Zeroise pkpv
  memset(&k, 0, sizeof(k));  // Zeroise k
  
  pack_ciphertext(c, &b, &v);
  
  // Finally, zeroise the remaining sensitive data
  memset(&b, 0, sizeof(b));  // Zeroise b
  memset(&v, 0, sizeof(v));  // Zeroise v
}


/*************************************************
 * Name:        indcpa_dec
 *
 * Description: Decryption function of the CPA-secure
 *              public-key encryption scheme underlying Kyber.
 *
 * Arguments:   - uint8_t *m: pointer to output decrypted message
 *                            (of length KYBER_INDCPA_MSGBYTES)
 *              - const uint8_t *c: pointer to input ciphertext
 *                                  (of length KYBER_INDCPA_BYTES)
 *              - const uint8_t *sk: pointer to input secret key
 *                                   (of length KYBER_INDCPA_SECRETKEYBYTES)
 **************************************************/

void indcpa_dec(uint8_t m[KYBER_INDCPA_MSGBYTES],
                const uint8_t c[KYBER_INDCPA_BYTES],
                const uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES])
{
  static polyvec b, skpv;
  static poly v, mp;

  unpack_ciphertext(&b, &v, c);
  unpack_sk(&skpv, sk);

  polyvec_ntt(&b);
  polyvec_basemul_acc_montgomery(&mp, &skpv, &b);
  poly_invntt_tomont(&mp);
  poly_sub(&mp, &v, &mp);
  poly_reduce(&mp);
  poly_tomsg(m, &mp);

  //zeroise sensitive data
  memset(&skpv, 0, sizeof(skpv));
  memset(&b, 0, sizeof(b));  
  memset(&v, 0, sizeof(v));  
  memset(&mp, 0, sizeof(mp));  
}
