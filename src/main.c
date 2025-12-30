#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

// This us the V_s voltage provided to MPXV6615V
const double V_REF = 5.;
// Multiple MPXV6615V V_OUT is scaled by via resistor divider
// to allow 3V3 ADC to interface with ~5V sensor
// Note: Max voltage ADC should see is REF_VOLTAGE * 0.92 (at atmospheric pressure)
const double V_OUT_MULTIPLE = 2400./(1000+2400);

const uint ADC_0 = 0;
const uint ADC_1 = 1;

// volts per bit in raw ADC measurement
const float CONVERSION_FACTOR = 3.3f / (1 << 12);

/**
 * returns gauge pressure in kPa i.e. delta from atmospheric (e.g. -101.3 would
 * be perfect vacuum under standard atmospheric pressure)
 * @param accumulator total of all samples (raw adc values)
 * @param num_samples number of samples
 * @return gauge pressure in kPa
 */
double convert_raw(uint** accumulator, uint num_samples)
{
    // voltage under 0 vacuum (i.e. atmospheric pressure)
    const static double OFFSET = V_REF * 0.92;
    // response kpa/v
    const static double SCALE = V_REF * 0.007652;

    double raw_avg = ((double)**accumulator) / num_samples;
    double v_out = (raw_avg * CONVERSION_FACTOR) / V_OUT_MULTIPLE;
    double vac = (v_out - OFFSET) / SCALE;
    **accumulator = 0;
    return vac;
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
            double pres_0 = convert_raw(&adc_accumulators, cnt);
            uint* p_elem_1 = adc_accumulators+1;
            double pres_1 = convert_raw(&p_elem_1, cnt);
            printf("%f, %f\n", pres_0, pres_1);
            cnt = 0;
        }
        sleep_ms(5);
    }
}