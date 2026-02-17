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

#define NTESTS 1000

static int test_keys_timed(uint64_t *d_keygen, uint64_t *d_enc, uint64_t *d_dec) {
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t key_a[CRYPTO_BYTES];
  uint8_t key_b[CRYPTO_BYTES];

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

static int test_invalid_sk_a(void) {
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t key_a[CRYPTO_BYTES];
  uint8_t key_b[CRYPTO_BYTES];

  crypto_kem_keypair(pk, sk);
  crypto_kem_enc(ct, key_b, pk);
  randombytes(sk, CRYPTO_SECRETKEYBYTES);
  crypto_kem_dec(key_a, ct, sk);

  if (!memcmp(key_a, key_b, CRYPTO_BYTES)) {
    printf("ERROR invalid sk\n");
    return 1;
  }
  return 0;
}

static int test_invalid_ciphertext(void) {
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t key_a[CRYPTO_BYTES];
  uint8_t key_b[CRYPTO_BYTES];
  uint8_t b;
  size_t pos;

  do { randombytes(&b, sizeof(uint8_t)); } while(!b);
  randombytes((uint8_t *)&pos, sizeof(size_t));

  crypto_kem_keypair(pk, sk);
  crypto_kem_enc(ct, key_b, pk);

  ct[pos % CRYPTO_CIPHERTEXTBYTES] ^= b;

  crypto_kem_dec(key_a, ct, sk);

  if (!memcmp(key_a, key_b, CRYPTO_BYTES)) {
    printf("ERROR invalid ciphertext\n");
    return 1;
  }
  return 0;
}

void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
  gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
  return cyw43_arch_init();
#endif
}

int main(void) {
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
  if (r) return 1;

  us = time_us_64();
  ms = to_ms_since_boot(get_absolute_time());
  printf("final =%" PRIu64 "us (%ums)\n", us, ms);

  pico_set_led(false);

  // Report sizes
  printf("CRYPTO_SECRETKEYBYTES:  %d\n", CRYPTO_SECRETKEYBYTES);
  printf("CRYPTO_PUBLICKEYBYTES:  %d\n", CRYPTO_PUBLICKEYBYTES);
  printf("CRYPTO_CIPHERTEXTBYTES: %d\n", CRYPTO_CIPHERTEXTBYTES);

  // Report totals and averages
  printf("NTESTS: %u\n", NTESTS);
  printf("Total keygen us:  %" PRIu64 "\n", sum_keygen);
  printf("Total encaps us:  %" PRIu64 "\n", sum_enc);
  printf("Total decaps us:  %" PRIu64 "\n", sum_dec);
  printf("Avg keygen us:    %" PRIu64 "\n", sum_keygen / NTESTS);
  printf("Avg encaps us:    %" PRIu64 "\n", sum_enc / NTESTS);
  printf("Avg decaps us:    %" PRIu64 "\n", sum_dec / NTESTS);

  return 0;
}
