#include <math.h>

#include <stddef.h>
#include <string.h>
#include "kem.h"
#include "randombytes.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include <inttypes.h>
#include <stdio.h>
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "profile.h"

#define NTESTS 1000

static int test_keys_timed(uint64_t *d_keygen, uint64_t *d_enc, uint64_t *d_dec) {
  static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  static uint8_t sk[CRYPTO_SECRETKEYBYTES];
  static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  static uint8_t key_a[CRYPTO_BYTES];
  static uint8_t key_b[CRYPTO_BYTES];

  uint64_t t0, t1;

  // keygen
  t0 = time_us_64();
  crypto_kem_keypair(pk, sk);
  t1 = time_us_64();
  *d_keygen = t1 - t0;

  // encaps
  t0 = time_us_64();
  crypto_kem_enc(ct, key_b, pk);
  t1 = time_us_64();
  *d_enc = t1 - t0;

  // decaps
  t0 = time_us_64();
  crypto_kem_dec(key_a, ct, sk);
  t1 = time_us_64();
  *d_dec = t1 - t0;

  if (memcmp(key_a, key_b, CRYPTO_BYTES)) {
    printf("ERROR keys\n");
    return 1;
  }
  return 0;
}

static int test_invalid_sk_a(void)
{
    static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    static uint8_t sk[CRYPTO_SECRETKEYBYTES];
    static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    static uint8_t key_a[CRYPTO_BYTES];
    static uint8_t key_b[CRYPTO_BYTES];

    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, key_b, pk);
    randombytes(sk, CRYPTO_SECRETKEYBYTES);
    crypto_kem_dec(key_a, ct, sk);

    if (!memcmp(key_a, key_b, CRYPTO_BYTES))
    {
        printf("ERROR invalid sk\n");
        return 1;
    }
    return 0;
}

static int test_invalid_ciphertext(void)
{
    static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    static uint8_t sk[CRYPTO_SECRETKEYBYTES];
    static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    static uint8_t key_a[CRYPTO_BYTES];
    static uint8_t key_b[CRYPTO_BYTES];
    static uint8_t b;
    size_t pos;

    do
    {
        randombytes(&b, sizeof(uint8_t));
    } while (!b);
    randombytes((uint8_t *)&pos, sizeof(size_t));

    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, key_b, pk);

    ct[pos % CRYPTO_CIPHERTEXTBYTES] ^= b;

    crypto_kem_dec(key_a, ct, sk);

    if (!memcmp(key_a, key_b, CRYPTO_BYTES))
    {
        printf("ERROR invalid ciphertext\n");
        return 1;
    }
    return 0;
}

void print_profile_results(int prof_runs,
                           double avg_keygen_total,
                           double avg_enc_total,
                           double avg_dec_total)
{
    printf("\n=== MULTICORE FINE-GRAIN PROFILING (CSV) ===\n");

    printf("phase,component,avg_us,percent_of_total\n");

    /* ================= KEYGEN ================= */

    double kg_total = avg_keygen_total;

    double kg_phase_hash = kg_prof.phase_hash_gena / (double)prof_runs;
    double kg_phase_mul  = kg_prof.phase_matmul / (double)prof_runs;
    double kg_phase_pack = kg_prof.phase_pack / (double)prof_runs;

    double kg_noise  = kg_prof.noise / (double)prof_runs;
    double kg_ntt    = kg_prof.ntt / (double)prof_runs;
    double kg_addred = kg_prof.add_reduce / (double)prof_runs;

    double kg_c1_hash = kg_prof.core1_hash_gena / (double)prof_runs;
    double kg_c1_mul  = kg_prof.core1_matmul / (double)prof_runs;
    double kg_c1_pack = kg_prof.core1_pack / (double)prof_runs;

    printf("\n# KEYGEN\n");

    printf("keygen,phase_hash_genA,%.2f,%.2f\n", kg_phase_hash, 100.0 * kg_phase_hash / kg_total);
    printf("keygen,phase_matmul,%.2f,%.2f\n",   kg_phase_mul,  100.0 * kg_phase_mul  / kg_total);
    printf("keygen,phase_pack,%.2f,%.2f\n",     kg_phase_pack, 100.0 * kg_phase_pack / kg_total);

    printf("keygen,core0_noise,%.2f,%.2f\n",    kg_noise,  100.0 * kg_noise  / kg_total);
    printf("keygen,core0_ntt,%.2f,%.2f\n",      kg_ntt,    100.0 * kg_ntt    / kg_total);
    printf("keygen,core0_add_reduce,%.2f,%.2f\n",kg_addred,100.0 * kg_addred/ kg_total);

    printf("keygen,core1_hash_genA,%.2f,%.2f\n", kg_c1_hash,100.0 * kg_c1_hash / kg_total);
    printf("keygen,core1_matmul,%.2f,%.2f\n",    kg_c1_mul, 100.0 * kg_c1_mul  / kg_total);
    printf("keygen,core1_pack,%.2f,%.2f\n",      kg_c1_pack,100.0 * kg_c1_pack / kg_total);

    /* Efficiency estimates */
    double kg_idle_core0 = kg_phase_hash - (kg_noise + kg_ntt);
    if (kg_idle_core0 < 0) kg_idle_core0 = 0;

    double kg_idle_core1 = kg_phase_hash - kg_c1_hash;
    if (kg_idle_core1 < 0) kg_idle_core1 = 0;

    printf("keygen,core0_idle_est,%.2f,%.2f\n", kg_idle_core0, 100.0 * kg_idle_core0 / kg_total);
    printf("keygen,core1_idle_est,%.2f,%.2f\n", kg_idle_core1, 100.0 * kg_idle_core1 / kg_total);


    /* ================= ENC ================= */

    double enc_total = avg_enc_total;

    double enc_phase_frommsg = enc_prof.phase_frommsg / (double)prof_runs;
    double enc_phase_mul     = enc_prof.phase_matmul / (double)prof_runs;

    double enc_unpack = enc_prof.unpack / (double)prof_runs;
    double enc_genat  = enc_prof.gen_at / (double)prof_runs;
    double enc_noise  = enc_prof.noise / (double)prof_runs;
    double enc_ntt    = enc_prof.ntt / (double)prof_runs;
    double enc_invntt = enc_prof.invntt / (double)prof_runs;
    double enc_addred = enc_prof.add_reduce / (double)prof_runs;
    double enc_pack   = enc_prof.pack / (double)prof_runs;

    double enc_c1_frommsg = enc_prof.core1_frommsg / (double)prof_runs;
    double enc_c1_mul     = enc_prof.core1_matmul / (double)prof_runs;

    printf("\n# ENC\n");

    printf("enc,phase_frommsg,%.2f,%.2f\n", enc_phase_frommsg, 100.0 * enc_phase_frommsg / enc_total);
    printf("enc,phase_matmul,%.2f,%.2f\n",  enc_phase_mul,     100.0 * enc_phase_mul / enc_total);

    printf("enc,core0_unpack,%.2f,%.2f\n", enc_unpack, 100.0 * enc_unpack / enc_total);
    printf("enc,core0_gen_at,%.2f,%.2f\n", enc_genat,  100.0 * enc_genat  / enc_total);
    printf("enc,core0_noise,%.2f,%.2f\n", enc_noise, 100.0 * enc_noise / enc_total);
    printf("enc,core0_ntt,%.2f,%.2f\n", enc_ntt, 100.0 * enc_ntt / enc_total);
    printf("enc,core0_invntt,%.2f,%.2f\n", enc_invntt, 100.0 * enc_invntt / enc_total);
    printf("enc,core0_add_reduce,%.2f,%.2f\n", enc_addred, 100.0 * enc_addred / enc_total);
    printf("enc,core0_pack,%.2f,%.2f\n", enc_pack, 100.0 * enc_pack / enc_total);

    printf("enc,core1_frommsg,%.2f,%.2f\n", enc_c1_frommsg, 100.0 * enc_c1_frommsg / enc_total);
    printf("enc,core1_matmul,%.2f,%.2f\n", enc_c1_mul, 100.0 * enc_c1_mul / enc_total);


    /* ================= DEC ================= */

    double dec_total = avg_dec_total;

    double dec_unpack = dec_prof.unpack / (double)prof_runs;
    double dec_ntt    = dec_prof.ntt / (double)prof_runs;
    double dec_mul    = dec_prof.matmul / (double)prof_runs;
    double dec_invntt = dec_prof.invntt / (double)prof_runs;
    double dec_sub    = dec_prof.sub_reduce / (double)prof_runs;
    double dec_msg    = dec_prof.tomsg / (double)prof_runs;

    printf("\n# DEC\n");

    printf("dec,unpack,%.2f,%.2f\n", dec_unpack, 100.0 * dec_unpack / dec_total);
    printf("dec,ntt,%.2f,%.2f\n", dec_ntt, 100.0 * dec_ntt / dec_total);
    printf("dec,matmul,%.2f,%.2f\n", dec_mul, 100.0 * dec_mul / dec_total);
    printf("dec,invntt,%.2f,%.2f\n", dec_invntt, 100.0 * dec_invntt / dec_total);
    printf("dec,sub_reduce,%.2f,%.2f\n", dec_sub, 100.0 * dec_sub / dec_total);
    printf("dec,tomsg,%.2f,%.2f\n", dec_msg, 100.0 * dec_msg / dec_total);
}

void pico_set_led(bool led_on)
{
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

int pico_led_init(void)
{
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    return cyw43_arch_init();
#endif
}

int main(void)
{
    stdio_init_all();
    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    stdio_usb_init();
    sleep_ms(5000);

    uint64_t us = time_us_64();
    uint32_t ms = to_ms_since_boot(get_absolute_time());
    printf("initial =%" PRIu64 "us (%ums)\n", us, ms);

    pico_set_led(true);


    uint64_t sum_keygen = 0, sum_enc = 0, sum_dec = 0;
    unsigned int i;
    int r = 0;

    for (i = 0; i < NTESTS; i++) {
        uint64_t dkg=0, den=0, ddc=0;
        r = test_keys_timed(&dkg, &den, &ddc);
        if (r) return 1;
        sum_keygen += dkg;
        sum_enc    += den;
        sum_dec    += ddc;
    }

    // Run correctness-negative tests (not included in timings)
    r |= test_invalid_sk_a();
    r |= test_invalid_ciphertext();
    if (r)
        return 1;

    us = time_us_64();
    ms = to_ms_since_boot(get_absolute_time());
    printf("final =%" PRIu64 "us (%ums)\n", us, ms);

    
    // Report sizes
    printf("CRYPTO_SECRETKEYBYTES:  %d\n", CRYPTO_SECRETKEYBYTES);
    printf("CRYPTO_PUBLICKEYBYTES:  %d\n", CRYPTO_PUBLICKEYBYTES);
    printf("CRYPTO_CIPHERTEXTBYTES: %d\n", CRYPTO_CIPHERTEXTBYTES);

   print_profile_results(
    NTESTS,
    sum_keygen/NTESTS,
    sum_enc/NTESTS,
    sum_dec/NTESTS
  );

    pico_set_led(false);
    
    return 0;
}
