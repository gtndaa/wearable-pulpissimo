/**
 * @file test_ppg_hr.c
 * @brief Hardware Test / Bring-up Runner for the PPG Heart Rate Library
 *
 * Runs against real PULP/Nusacore FPGA hardware only (no host-side mock),
 * exactly like test_max30102.c. Uses max30102_read_sample() purely for
 * raw IR/RED acquisition (unmodified driver, out of scope here) and feeds
 * the raw IR value into ppg_hr_process() to exercise the ported
 * SparkFun PBA algorithm end-to-end.
 *
 * Output format on the streaming section intentionally matches
 * SparkFun's original example sketch:
 *   IR=<value>, BPM=<value>, Avg BPM=<value>
 *
 * KNOWN LIMITATION: the board's micros()/millis() timer bring-up is a
 * separate, outstanding hardware issue (micros() currently always
 * returns 0 - see the stub at the bottom of this file, same as in
 * test_max30102.c). ppg_hr_process() takes its timestamp as an explicit
 * parameter rather than reading a system timer itself, specifically so
 * this test can run today using a software millisecond counter derived
 * from the configured sample rate as a placeholder. Once real timer
 * bring-up lands, replace sw_now_ms() below with the real millisecond
 * timer and nothing else in this file (or in ppg_hr itself) needs to
 * change.
 */

#include <stdbool.h>

#include "pulp.h"
#include "max30102.h"
#include "max30102_regs.h"
#include "ppg_hr.h"
#include <stdio.h>

int tests_passed = 0;
int tests_failed = 0;

#define ASSERT(condition, test_name) \
    do { \
        if (!(condition)) { \
            printf("[FAIL] %s\n", test_name); \
            tests_failed++; \
        } else { \
            printf("[PASS] %s\n", test_name); \
            tests_passed++; \
        } \
    } while (0)

max30102_t sensor;
ppg_hr_t hr;

struct callback_arg_t
{
    long             last_print;
    bool             result_is_valid;
    ppg_hr_result_t  hr_result;
};

// -------------------------------------------------------------------------
// Placeholder millisecond clock (see KNOWN LIMITATION above)
// -------------------------------------------------------------------------
// Advances by one sample period per call. Sample period is derived from
// whatever sample_rate the sensor is currently configured with, so it
// stays consistent whether the FIFO is running at 100 sps, 400 sps, etc.
static uint32_t sw_clock_ms = 0;

static uint32_t sw_now_ms(uint32_t sample_period_ms) {
    sw_clock_ms += sample_period_ms;
    return sw_clock_ms;
}

// -------------------------------------------------------------------------
// Helpers to compute the REAL FIFO output period
// -------------------------------------------------------------------------

/**
 * @brief Decode max30102_sample_rate_t into the actual ADC sample rate
 *        in Hz (the raw conversion rate, before any FIFO averaging).
 */
static uint32_t sample_rate_to_hz(max30102_sample_rate_t sr) {
    switch (sr) {
        case MAX30102_SR_50:   return 50;
        case MAX30102_SR_100:  return 100;
        case MAX30102_SR_200:  return 200;
        case MAX30102_SR_400:  return 400;
        case MAX30102_SR_800:  return 800;
        case MAX30102_SR_1000: return 1000;
        case MAX30102_SR_1600: return 1600;
        case MAX30102_SR_3200: return 3200;
        default:                return 100; // safe fallback
    }
}

/**
 * @brief Decode max30102_fifo_avg_t into its averaging divisor.
 */
static uint32_t fifo_avg_to_divisor(max30102_fifo_avg_t avg) {
    switch (avg) {
        case MAX30102_FIFO_AVG_1:  return 1;
        case MAX30102_FIFO_AVG_2:  return 2;
        case MAX30102_FIFO_AVG_4:  return 4;
        case MAX30102_FIFO_AVG_8:  return 8;
        case MAX30102_FIFO_AVG_16: return 16;
        case MAX30102_FIFO_AVG_32: return 32;
        default:                    return 1; // safe fallback
    }
}

/**
 * @brief Compute the REAL interval (ms) between successive FIFO entries.
 *
 * IMPORTANT: config.sample_rate is the internal ADC conversion rate, NOT
 * the rate at which entries actually land in the FIFO. When FIFO
 * averaging is enabled, the sensor averages fifo_avg raw ADC samples
 * together before pushing a single entry to the FIFO, so the real
 * output rate is (sample_rate / fifo_avg), and the real period between
 * entries is correspondingly longer. Getting this wrong silently skews
 * every BPM calculation by exactly the averaging factor, since the
 * software clock (sw_now_ms()) has no other way to know how much real
 * time passed between reads on this platform (see KNOWN LIMITATION at
 * the top of this file).
 */
static uint32_t compute_fifo_period_ms(const max30102_config_t *config) {
    uint32_t sps = sample_rate_to_hz(config->sample_rate);
    uint32_t avg = fifo_avg_to_divisor(config->fifo_avg);
    // period_ms = 1000 * avg / sps, computed this way (not 1000/(sps/avg))
    // to avoid integer-division rounding since sps/avg may not divide evenly.
    return (1000 * avg) / sps;
}

// -------------------------------------------------------------------------
// Sensor configuration matching SparkFun's Example5_HeartRate.ino
// -------------------------------------------------------------------------

/**
 * @brief Build the sensor configuration that actually runs under
 *        SparkFun's Example5_HeartRate.ino - NOT this driver's generic
 *        max30102_get_default_config(), which uses different LED drive
 *        currents.
 *
 * The example sketch does:
 *   particleSensor.setup();                     // MAX30105::setup() defaults:
 *                                                //   sampleRate=400, adcRange=4096,
 *                                                //   pulseWidth=411 (18-bit), sampleAverage=4,
 *                                                //   powerLevel=0x1F (~6.4 mA) for all LEDs
 *   particleSensor.setPulseAmplitudeRed(0x0A);   // Red dimmed to ~2.4 mA - used only as a
 *                                                //   visual "finger is here" indicator, it is
 *                                                //   NEVER read by checkForBeat()
 *   particleSensor.setPulseAmplitudeGreen(0);    // Green off - MAX30102 has no Green channel
 *                                                //   (SpO2 mode is Red+IR only), so there is
 *                                                //   nothing to set here on this driver
 *
 * checkForBeat()/ppg_hr only ever consumes the IR channel, so what
 * matters for matching the algorithm's real-world behavior is that IR
 * stays at setup()'s untouched default (0x1F), not this driver's usual
 * default (0x24). sample_rate/adc_range/pulse_width/fifo_avg already
 * line up with max30102_get_default_config(), only the LED currents
 * differ from SparkFun's example.
 */
static max30102_config_t build_sparkfun_example_config(void) {
    max30102_config_t config = max30102_get_default_config();
    config.sample_rate     = MAX30102_SR_400;
    config.adc_range       = MAX30102_ADC_RANGE_4096;
    config.pulse_width     = MAX30102_PULSE_WIDTH_18BIT; // ~411 us
    config.fifo_avg        = MAX30102_FIFO_AVG_4;
    config.ir_led_current  = 0x1F; // setup()'s untouched default - this is what checkForBeat() sees
    config.red_led_current = 0x0A; // example's explicit override - indicator only, unused by the algorithm
    return config;
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

void test_sensor_bringup(void) {
    printf("\n--- Test: Sensor Bring-up ---\n");
    max30102_status_t status = max30102_init(&sensor, 0); // I2C port 0
    ASSERT(status == MAX30102_OK, "max30102_init returns OK");

    max30102_config_t config = build_sparkfun_example_config();
    status = max30102_configure(&sensor, &config);
    ASSERT(status == MAX30102_OK, "max30102_configure (SparkFun example config) returns OK");
}

void test_ppg_hr_init(void) {
    printf("\n--- Test: ppg_hr_init ---\n");
    ppg_hr_init(&hr);

    ASSERT(hr.IR_AC_Max == 20, "IR_AC_Max initialized to 20");
    ASSERT(hr.IR_AC_Min == -20, "IR_AC_Min initialized to -20");
    ASSERT(hr.beatAvg == 0, "beatAvg initialized to 0");
    ASSERT(hr.rateSpot == 0, "rateSpot initialized to 0");
    ASSERT(hr.offset == 0, "FIR offset initialized to 0");
}

/**
 * @brief Sanity-check checkForBeat() against a synthetic signal.
 *
 * Feeds a triangle-wave IR signal (DC offset + ramped AC swing large
 * enough to clear the (20, 1000) threshold) through the unmodified
 * algorithm and confirms at least one beat is detected. This does not
 * validate real physiological accuracy (that requires real hardware
 * data per the project's outstanding tuning task) - it only proves the
 * ported math runs and can trigger a beat at all, catching integration
 * mistakes like accidental type truncation or buffer indexing errors.
 */
void test_beat_detection_synthetic(void) {
    printf("\n--- Test: Beat Detection (synthetic triangle wave) ---\n");

    ppg_hr_t synth_hr;
    ppg_hr_init(&synth_hr);

    const int32_t dc_offset = 50000;
    const int32_t amplitude = 300; // > 20-count threshold after filtering
    const int32_t period_samples = 40; // ~2.5 Hz-ish for quick synthetic test

    int beats_seen = 0;
    for (int n = 0; n < period_samples * 6; n++) {
        int phase = n % period_samples;
        int32_t triangle = (phase < period_samples / 2)
                                ? (phase * (2 * amplitude) / (period_samples / 2)) - amplitude
                                : (amplitude - ((phase - period_samples / 2) * (2 * amplitude) / (period_samples / 2)));

        int32_t sample = dc_offset + triangle;

        if (ppg_hr_check_for_beat(&synth_hr, sample)) {
            beats_seen++;
        }
    }

    ASSERT(beats_seen > 0, "checkForBeat() detects at least one beat on synthetic signal");
}

void test_invalid_param(void) {
    printf("\n--- Test: Invalid Parameter Handling ---\n");
    // ppg_hr_init tolerates NULL without crashing
    ppg_hr_init(NULL);
    ASSERT(1, "ppg_hr_init(NULL) does not crash");
}

// -------------------------------------------------------------------------
// Continuous acquisition (unbuffered live streaming)
// -------------------------------------------------------------------------

/**
 * @brief Continuously acquire raw IR samples from MAX30102, process through
 *        ppg_hr_process(), and print results in real time.
 */
void run_continuous_hr_acquisition(void (*callback)(void *arg, bool sample_is_available, const void *result), void *callback_arg) {
    unsigned int last_run;

    printf("\n--- PPG Heart Rate Continuous Acquisition (Avg BPM output every 5s @ 100 sps) ---\n");

    max30102_config_t config = build_sparkfun_example_config();
    config.sample_rate = MAX30102_SR_100;

    max30102_status_t cfg_status = max30102_configure(&sensor, &config);
    if (cfg_status != MAX30102_OK) {
        printf("Failed to (re)configure sensor before acquisition (%d)\n", cfg_status);
        return;
    }

    sw_clock_ms = 0;
    uint32_t last_print_ms = 0;
    ppg_hr_init(&hr);

    const uint32_t sample_period_ms = compute_fifo_period_ms(&config);

    last_run = 0;

    while (1) {
        bool sample_is_available = false;

        max30102_sample_t sample;
        max30102_status_t status = MAX30102_ERROR_NO_DATA; /* default no data */

        ppg_hr_result_t hr_result;

        //if (last_run++ >= 100000)
        //{
            last_run = 0; /* reset last_run */

            /* do ppg stuff */
            status = max30102_read_sample(&sensor, &sample);
            if (status == MAX30102_OK) {
                uint32_t t_ms = pos_tick_get_counter_ms();
                hr_result = ppg_hr_process(&hr, (int32_t)sample.ir, t_ms);

                if (t_ms - last_print_ms >= 5000) {
                    last_print_ms = t_ms;
                    printf("[t=%us] Avg BPM=%d", (unsigned int)(t_ms / 1000), (int)hr_result.avg_bpm);
                    if (sample.ir < 50000) {
                        printf(" (No finger?)");
                    }
                    else
                    {
                        sample_is_available = true;
                    }
                    printf("\n");
                }
            }
        //}
        /**
            * if last_run less than 100000, do nothing
            * well, actually just call callback
            */

        /* call callback if not NULL */
        if (callback != NULL)
        {
            callback(callback_arg, sample_is_available, &hr_result);
        }
    }
}

/**
    * callback to be called continously by loop
    * argument:
    * arg: pointer passed by run_continuous_hr_acquisition as callback_arg
    * sample_is_available: boolean value, that indicate hr_result is available and finger detected
    * result: hr_result value, casted to generic pointer
    */
void hello_world(void *arg, bool sample_is_available, const void *result)
{
    struct callback_arg_t *_arg = (struct callback_arg_t *) arg;

    long current_tick;
    long last_print;

    /* update data if valid */
    if (sample_is_available)
    {
        _arg->result_is_valid = true; /* update validity */
        _arg->hr_result = *((const ppg_hr_result_t *) result); /* update result */
    }

    /* try to print hello world every second */
    last_print = _arg->last_print;
    current_tick = pos_tick_get_counter_ms();
    if (current_tick > (last_print + 1000))
    {
        /* update last_print */
        _arg->last_print = current_tick;

        /* do some thing */
        if (_arg->result_is_valid)
        {
            printf("\nhello world [t=%us] Avg BPM=%d\n", (unsigned int)(current_tick / 1000), (int)_arg->hr_result.avg_bpm);
        }
        else
        {
            printf("hello world\n");
        }
    }
}

int main(void) {

    struct callback_arg_t arg;

    printf("==========================================\n");
    printf("        PPG HEART RATE LIBRARY TEST        \n");
    printf("==========================================\n");

    test_sensor_bringup();
    test_ppg_hr_init();
    test_beat_detection_synthetic();
    test_invalid_param();

    printf("\n==========================================\n");
    printf("TEST RESULTS: %d PASS, %d FAIL\n", tests_passed, tests_failed);
    printf("==========================================\n");

    arg.last_print = pos_tick_get_counter_ms();
    arg.result_is_valid = false;

    run_continuous_hr_acquisition(&hello_world, &arg);

    return 0; // unreachable
}

void pe_start(void) {}

// Dummy micros() to satisfy linker error in I2C driver
// (same outstanding hardware bring-up issue as in test_max30102.c;
// not used by ppg_hr, which takes its timestamp as a parameter instead)
uint32_t micros(void) {
    return 0;
}