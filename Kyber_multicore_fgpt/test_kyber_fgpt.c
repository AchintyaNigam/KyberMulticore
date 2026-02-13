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

static int test_keys_timed()
{
    static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    static uint8_t sk[CRYPTO_SECRETKEYBYTES];
    static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    static uint8_t key_a[CRYPTO_BYTES];
    static uint8_t key_b[CRYPTO_BYTES];

    crypto_kem_keypair(pk, sk);

    // encaps
    crypto_kem_enc(ct, key_b, pk);

    // decaps
    crypto_kem_dec(key_a, ct, sk);

    if (memcmp(key_a, key_b, CRYPTO_BYTES))
    {
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


    unsigned int i;
    int r = 0;

    for (i = 0; i < NTESTS; i++)
    {
        r = test_keys_timed();
        if (r)
            return 1;
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

    printf("\n--- Fine-grained (per run averages) ---\n");

    printf("\nKeygen breakdown:\n");
    printf(" hash+genA: %.2f us\n", kg_prof.hash_gena / (double)prof_runs);
    printf(" noise:     %.2f us\n", kg_prof.noise / (double)prof_runs);
    printf(" NTT:       %.2f us\n", kg_prof.ntt / (double)prof_runs);
    printf(" matmul:    %.2f us\n", kg_prof.matmul / (double)prof_runs);
    printf(" add/red:   %.2f us\n", kg_prof.add_reduce / (double)prof_runs);
    printf(" pack:      %.2f us\n", kg_prof.pack / (double)prof_runs);

    printf("\nEnc breakdown:\n");
    printf(" unpack: %.2f us\n", enc_prof.unpack / (double)prof_runs);
    printf(" frommsg: %.2f us\n", enc_prof.frommsg / (double)prof_runs);
    printf(" NTT:       %.2f us\n", enc_prof.ntt / (double)prof_runs);
    printf(" gen_at: %.2f us\n", enc_prof.gen_at / (double)prof_runs);
    printf(" noise: %.2f us\n", enc_prof.noise / (double)prof_runs);
    printf(" matmul: %.2f us\n", enc_prof.matmul / (double)prof_runs);
    printf(" invntt: %.2f us\n", enc_prof.invntt / (double)prof_runs);
    printf(" add/red: %.2f us\n", enc_prof.add_reduce / (double)prof_runs);
    printf(" pack: %.2f us\n", enc_prof.pack / (double)prof_runs);

    printf("\nDec breakdown:\n");
    printf(" unpack: %.2f us\n", dec_prof.unpack / (double)prof_runs);
    printf(" ntt: %.2f us\n", dec_prof.ntt / (double)prof_runs);
    printf(" matmul: %.2f us\n", dec_prof.matmul / (double)prof_runs);
    printf(" invntt: %.2f us\n", dec_prof.invntt / (double)prof_runs);
    printf(" sub/red: %.2f us\n", dec_prof.sub_reduce / (double)prof_runs);
    printf(" tomsg: %.2f us\n", dec_prof.tomsg / (double)prof_runs);

    
    pico_set_led(false);
    
    return 0;
}
