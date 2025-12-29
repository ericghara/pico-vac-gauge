#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

// This is LED pin for pico. It does NOT work for pico W
const uint LEDPIN = 25;
const uint ADC_0 = 0;
const uint ADC_1 = 1;

const float CONVERSION_FACTOR = 3.3f / (1 << 12);

double convert_raw(uint accumulator, uint num_samples)
{
    double raw_avg = ((double)accumulator) / num_samples;
    double volts = raw_avg * CONVERSION_FACTOR;
    return volts;
}

int main() {
    stdio_init_all();
    adc_init();

    uint* adc_accumulators = calloc(2, sizeof(uint));
    uint cnt = 0;

    while (1)
    {
        adc_select_input(ADC_0);
        adc_accumulators[0] += adc_read();
        adc_select_input(ADC_1);
        adc_accumulators[1] += adc_read();

        cnt++;

        if (cnt == 100) {
            double volts_0 = convert_raw(adc_accumulators[0], cnt);
            double volts_1 = convert_raw(adc_accumulators[1], cnt);
            printf("%f, %f\n", volts_0, volts_1);
            cnt = 0;
            memset(adc_accumulators, 0, sizeof(uint) * 2);
        }
        sleep_ms(5);
    }
}