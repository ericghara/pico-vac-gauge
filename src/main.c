#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

// This is LED pin for pico. It does NOT work for pico W
const uint LEDPIN = 25;

int main() {
    stdio_init_all();
    adc_init();
    adc_select_input(1);
    const float conversionFactor = 3.3f / (1 << 12);

    int cnt = 0;
    uint tot = 0;

    while (1)
    {
        tot += adc_read();
        cnt++;

        if (cnt == 100) {
            double raw_avg = ((double)tot) / cnt;
            double volts = raw_avg * conversionFactor;
            printf("%f\n", volts);
            cnt = 0;
            tot = 0;
        }
        sleep_ms(5);
    }
}