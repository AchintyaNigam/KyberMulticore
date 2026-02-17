#include <stddef.h>
#include <string.h>
#include "kem.h"
#include "randombytes.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include <inttypes.h>
#include <stdio.h>
#include "pico/time.h"

#define NTESTS 1000
#define SIGNAL_PIN 3

static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t key_a[CRYPTO_BYTES];
static uint8_t key_b[CRYPTO_BYTES];

int main(void)
{
    stdio_init_all();
    stdio_usb_init();

    gpio_init(SIGNAL_PIN);
    gpio_set_dir(SIGNAL_PIN, GPIO_OUT);
    gpio_put(SIGNAL_PIN, 0);

    sleep_ms(5000);   // time to open serial monitor

    printf("\n==== ENERGY PROFILING START ====\n");
    printf("NTESTS = %d\n", NTESTS);

    // /* =========================
    //    KEYGEN BATCH
    //    ========================= */
    // printf("\n--- KEYGEN batch start ---\n");
    // gpio_put(SIGNAL_PIN, 1);

    // for (int i = 0; i < NTESTS; i++) {
    //     crypto_kem_keypair(pk, sk);
    // }

    // gpio_put(SIGNAL_PIN, 0);
    // printf("--- KEYGEN batch end ---\n");
    // sleep_ms(3000);   // allow Arduino to finish printing


    // /* =========================
    //    ENCAP BATCH
    //    ========================= */
    // printf("\n--- ENCAP batch start ---\n");

    // // Need a valid keypair first
    // crypto_kem_keypair(pk, sk);

    // gpio_put(SIGNAL_PIN, 1);

    // for (int i = 0; i < NTESTS; i++) {
    //     crypto_kem_enc(ct, key_b, pk);
    // }

    // gpio_put(SIGNAL_PIN, 0);
    // printf("--- ENCAP batch end ---\n");
    // sleep_ms(3000);


    /* =========================
       DECAP BATCH
       ========================= */
    printf("\n--- DECAP batch start ---\n");

    // Generate fresh ciphertexts
    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, key_b, pk);

    gpio_put(SIGNAL_PIN, 1);

    for (int i = 0; i < NTESTS; i++) {
        crypto_kem_dec(key_a, ct, sk);
    }

    gpio_put(SIGNAL_PIN, 0);
    printf("--- DECAP batch end ---\n");

    printf("\n==== ENERGY PROFILING COMPLETE ====\n");

    // printf("CRYPTO_SECRETKEYBYTES:  %d\n", CRYPTO_SECRETKEYBYTES);
    // printf("CRYPTO_PUBLICKEYBYTES:  %d\n", CRYPTO_PUBLICKEYBYTES);
    // printf("CRYPTO_CIPHERTEXTBYTES: %d\n", CRYPTO_CIPHERTEXTBYTES);

    // while (1) sleep_ms(1000);
}
