#include <stdio.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/stdio_usb.h"   // For stdio_usb_connected()
#include "pico/cyw43_arch.h"

void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}

int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
#endif
}

int main() {
    stdio_init_all();
     int rc = pico_led_init();
    hard_assert(rc == PICO_OK);

    // Wait for the USB serial connection to be ready (important for USB CDC)
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    while (true) {
        pico_set_led(true);
        uint32_t rand_num = get_rand_32();
        printf("Random 32-bit number: %08" PRIx32 "\n", rand_num);
        sleep_ms(2000); // Print every second

        pico_set_led(false);
        sleep_ms(2000); // Print every second
    }

    return 0;
}
