#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

/* #######################
 * # Low Level constants #
 * #######################
 */

// This us the V_s voltage provided to MPXV6615V
const double V_REF = 5.;
// Multiple MPXV6615V V_OUT is scaled by via resistor divider
// to allow 3V3 ADC to interface with ~5V sensor
// Note: Max voltage ADC should see is REF_VOLTAGE * 0.92 (at atmospheric pressure)
const double V_OUT_MULTIPLE = 2400./(1000+2400);
const uint ADC_0 = 0;
const uint ADC_1 = 1;

/* #############################
 * # Data Processing Constants #
 * #############################
*/

// buckets raw samples will be placed in
const uint NUM_BUCKETS = 1 << 11;
// bits to shift raw adc reading right to find bucket
const uint BUCKET_SHIFT = 1;
// Percentiles to be logged
// should be in ascending order and not exceeding 100
const uint PERCENTILES[] = {5, 50, 95};
// volts per bit in raw ADC measurement
const float CONVERSION_FACTOR = 3.3f / (1 << 12);

/**
 * returns gauge pressure in kPa i.e. delta from atmospheric (e.g. -101.3 would
 * be perfect vacuum under standard atmospheric pressure)
 * @param accumulator total of all samples (raw adc values)
 * @param num_samples number of samples
 * @return gauge pressure in kPa
 */
double convert_raw(uint accumulator, uint num_samples)
{
    // voltage under 0 vacuum (i.e. atmospheric pressure)
    const static double OFFSET = V_REF * 0.92;
    // response kpa/v
    const static double SCALE = V_REF * 0.007652;

    double raw_avg = ((double)accumulator) / num_samples;
    double v_out = (raw_avg * CONVERSION_FACTOR) / V_OUT_MULTIPLE;
    double vac = (v_out - OFFSET) / SCALE;
    return vac;
}

typedef struct ChannelData
{
    uint adc_num;
    uint sample_cnt;
    uint accumulator;
    uint sample_distribution[];
} ChannelData_t;

/**
 * Initialize a ChannelData
 * @param adc_num ADC number channel should be created for
 * @return initialized channel data struct with all fields zeroed except `adc_num`
 */
ChannelData_t* init_channel_data(uint adc_num)
{
    // change me if fields added to ChannelData
    ChannelData_t* channel = (ChannelData_t*) calloc(3+NUM_BUCKETS, sizeof(unsigned int));
    channel->adc_num = adc_num;
    return channel;
}

/**
 * Zero out all fields in ChannelData except `adc_num`
 * @param channel struct to reset
 */
void reset(ChannelData_t* channel)
{
    void* start_t = &channel->sample_cnt;
    // change me if fields added to channel data, should preserve adc_num but clear everything else
    memset(start_t, 0, 2+NUM_BUCKETS*sizeof(unsigned int));
}

/**
 * Record a single measurement
 *
 * Note: in addition to recording value
 * modifies ChannelData to store measurement.
 *
 * @param channel channel to measure
 * @return measured adc_value
 */
uint sample(ChannelData_t* channel)
{
    adc_select_input(channel->adc_num);
    uint adc_val = adc_read();
    channel->accumulator += adc_val;
    uint bucket = adc_val >> BUCKET_SHIFT;
    channel->sample_distribution[bucket]++;
    channel->sample_cnt++;
    return adc_val;
}

/**
 * Logs statistics for channel
 *
 * Statistics are average and vacuum percentiles.
 * Percentiles recorded as the largest value (least vacuum) in the percentile,
 * e.g. for samples (-3, -2, -1), 33.3 percentile would be -3, 66.6 percentile would be -2 etc.
 *
 * @param channel channel to log
 */
void log_stats(ChannelData_t* channel)
{
    printf("Channel %d | avg %6.1f, percentiles [", channel->adc_num, convert_raw(channel->accumulator, channel->sample_cnt));
    unsigned int samples = channel->sample_distribution[0];
    unsigned int bucket_i = 0;
    uint num_percentiles = sizeof(PERCENTILES)/sizeof(unsigned int);
    for (unsigned int* percentile_p = (unsigned int*) PERCENTILES; percentile_p < PERCENTILES + num_percentiles; percentile_p++)
    {
        // slightly lossy avoiding floats, but fine for large numbers of samples
        unsigned int samples_needed = (channel->sample_cnt * *percentile_p)/100;
        while (samples < samples_needed)
        {
            bucket_i++;
            samples += channel->sample_distribution[bucket_i];
        }
        unsigned int raw_value = bucket_i << BUCKET_SHIFT;
        double vacuum_kpa = convert_raw(raw_value, 1);
        printf("%d: %6.1f, ", *percentile_p, vacuum_kpa);
    }
    printf("]\n");
}

int main() {
    stdio_init_all();
    adc_init();

    ChannelData_t** channels = calloc(2, sizeof(ChannelData_t*));
    channels[0] = init_channel_data(ADC_0);
    channels[1] = init_channel_data(ADC_1);

    uint cnt = 0;

    while (1)
    {
        sample(channels[0]);
        sample(channels[1]);

        cnt++;

        if (cnt == 1000) {
            double pres_0 = convert_raw(channels[0]->accumulator, channels[0]->sample_cnt);
            double pres_1 = convert_raw(channels[1]->accumulator, channels[1]->sample_cnt);
            log_stats(channels[0]);
            log_stats(channels[1]);
            cnt = 0;
            reset(channels[0]);
            reset(channels[1]);
        }
        sleep_ms(1);
    }
}