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

#define NTESTS 100
#define SIGNAL_PIN 2

static int test_keys(void)
{
    static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    static uint8_t sk[CRYPTO_SECRETKEYBYTES];
    static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
    static uint8_t key_a[CRYPTO_BYTES];
    static uint8_t key_b[CRYPTO_BYTES];

    // Alice generates a public key
    crypto_kem_keypair(pk, sk);

    // Bob derives a secret key and creates a response
    crypto_kem_enc(ct, key_b, pk);
    // Alice uses Bobs response to get her shared key
    crypto_kem_dec(key_a, ct, sk);
    if (memcmp(key_a, key_b, CRYPTO_BYTES))
    {
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

    // Alice generates a public key
    crypto_kem_keypair(pk, sk);

    // Bob derives a secret key and creates a response
    crypto_kem_enc(ct, key_b, pk);

    // Replace secret key with random values
    randombytes(sk, CRYPTO_SECRETKEYBYTES);

    // Alice uses Bobs response to get her shared key
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
    static size_t pos;

    do
    {
        randombytes(&b, sizeof(uint8_t));
    } while (!b);
    randombytes((uint8_t *)&pos, sizeof(size_t));

    // Alice generates a public key
    crypto_kem_keypair(pk, sk);

    // Bob derives a secret key and creates a response
    crypto_kem_enc(ct, key_b, pk);

    // Change some byte in the ciphertext (i.e., encapsulated key)
    ct[pos % CRYPTO_CIPHERTEXTBYTES] ^= b;

    // Alice uses Bobs response to get her shared key
    crypto_kem_dec(key_a, ct, sk);

    if (!memcmp(key_a, key_b, CRYPTO_BYTES))
    {
        printf("ERROR invalid ciphertext\n");
        return 1;
    }

    return 0;
}

int main(void)
{

    stdio_init_all();
    gpio_init(SIGNAL_PIN);
    gpio_set_dir(SIGNAL_PIN, true);
    unsigned int i;
    int r;
    stdio_usb_init();
    sleep_ms(5000);
    uint64_t us = time_us_64();                          // monotonic µs since boot
    uint32_t ms = to_ms_since_boot(get_absolute_time()); // ms since boot
    printf(" initial =%" PRIu64 "us (%ums)\n", us, ms);
    gpio_put(SIGNAL_PIN, 1);
    for (i = 0; i < NTESTS; i++)
    {
        r = test_keys();
        r |= test_invalid_sk_a();
        r |= test_invalid_ciphertext();
        if (r)
            return 1;
    }
    gpio_put(SIGNAL_PIN, 0);
        us = time_us_64();                      // monotonic µs since boot
    ms = to_ms_since_boot(get_absolute_time()); // ms since boot
    printf("final =%" PRIu64 "us (%ums)\n", us, ms);
    printf("CRYPTO_SECRETKEYBYTES:  %d\n", CRYPTO_SECRETKEYBYTES);
    printf("CRYPTO_PUBLICKEYBYTES:  %d\n", CRYPTO_PUBLICKEYBYTES);
    printf("CRYPTO_CIPHERTEXTBYTES: %d\n", CRYPTO_CIPHERTEXTBYTES);

    return 0;
}
