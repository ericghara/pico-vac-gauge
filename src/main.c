#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

/* #######################
 * # Low Level constants #
 * #######################
 */
// should be a multiple of # of ADCs sampled
#define CAPTURE_DEPTH 50000
uint16_t capture_buffer[CAPTURE_DEPTH];

// This us the V_s voltage provided to MPXV6615V
const double V_REF = 5.;
// Multiple MPXV6615V V_OUT is scaled by via resistor divider
// to allow 3V3 ADC to interface with ~5V sensor
// Note: Max voltage ADC should see is REF_VOLTAGE * 0.92 (at atmospheric pressure)
const double V_OUT_MULTIPLE = 2400. / (1000 + 2400);
const uint ADC_0 = 0;
const uint ADC_1 = 1;
const uint ADC_CHANNEL_MASK = 1 << ADC_0 | 1 << ADC_1;

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
 * Zero out all fields in ChannelData except `adc_num`
 * @param channel struct to reset
 */
void reset(ChannelData_t* channel)
{
    void* start_p = &channel->sample_cnt;
    uint size_b = sizeof(ChannelData_t) + NUM_BUCKETS * sizeof(uint);
    // clear everything after adc_num field
    memset(start_p, 0, size_b - sizeof(uint));
}

/**
 * Initialize a ChannelData
 * @param adc_num ADC number channel should be created for
 * @return initialized channel data struct with all fields zeroed except `adc_num`
 */
ChannelData_t* init_channel_data(uint adc_num)
{
    uint size_b = sizeof(ChannelData_t) + NUM_BUCKETS * sizeof(uint);
    ChannelData_t* channel = (ChannelData_t*)malloc(size_b);
    channel->adc_num = adc_num;
    reset(channel);
    return channel;
}


/**
 * Record a single measurement
 *
 * Note: in addition to recording value
 * modifies ChannelData to store measurement.
 *
 * @param channel channel to measure
 * @param adc_val ADC reading to put
 */
void put_sample(ChannelData_t* channel, uint16_t adc_val)
{
    channel->accumulator += adc_val;
    uint bucket = adc_val >> BUCKET_SHIFT;
    channel->sample_distribution[bucket]++;
    channel->sample_cnt++;
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
    printf("Channel %d | avg %6.1f, percentiles [", channel->adc_num,
           convert_raw(channel->accumulator, channel->sample_cnt));
    unsigned int samples = channel->sample_distribution[0];
    unsigned int bucket_i = 0;
    uint num_percentiles = sizeof(PERCENTILES) / sizeof(unsigned int);
    for (unsigned int* percentile_p = (unsigned int*)PERCENTILES; percentile_p < PERCENTILES + num_percentiles;
         percentile_p++)
    {
        // slightly lossy avoiding floats, but fine for large numbers of samples
        unsigned int samples_needed = (channel->sample_cnt * *percentile_p) / 100;
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

int main()
{
    stdio_init_all();
    adc_init();

    ChannelData_t** adc_channels = calloc(2, sizeof(ChannelData_t*));
    adc_channels[0] = init_channel_data(ADC_0);
    adc_channels[1] = init_channel_data(ADC_1);

    // set to fixed input so we know where round-robin sampling will start from
    adc_select_input(adc_channels[0]->adc_num);
    adc_set_round_robin(ADC_CHANNEL_MASK);
    adc_fifo_setup(true, true, 1, false, false);
    // 48_000_000 hz / (959+1) = 50_000 samples per second, across 2 inputs = 25_000 samples per input per second
    adc_set_clkdiv(959);
    sleep_ms(1000);

    uint dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    // transfer when ADC sample ready
    channel_config_set_dreq(&cfg, DREQ_ADC);

    while (1)
    {
        dma_channel_configure(dma_chan, &cfg, capture_buffer, &adc_hw->fifo, CAPTURE_DEPTH, true);
        adc_run(true);
        // perform conversion for last round of samples while capturing next round
        for (int i = 0; i < CAPTURE_DEPTH; i++)
        {
            uint channel_i = i & 1;
            put_sample(adc_channels[channel_i], capture_buffer[i]);
        }
        log_stats(adc_channels[0]);
        log_stats(adc_channels[1]);
        reset(adc_channels[0]);
        reset(adc_channels[1]);
        // wait for this capture to finish
        dma_channel_wait_for_finish_blocking(dma_chan);
        adc_run(false);
        // drop anything remaining in FIFO to avoid contaminating next cycle
        adc_fifo_drain();
    }
}
