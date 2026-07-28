/**
 * @file ppg_hr.c
 * @brief PPG Heart Rate Detection Implementation
 *
 * Direct port of SparkFun's Optical Heart Rate Detection (PBA algorithm),
 * originally by Maxim Integrated / Nathan Seidle
 * (SparkFun_MAX3010x_Sensor_Library/src/heartRate.cpp), plus the BPM
 * averaging logic from that library's Example5_HeartRate.ino.
 *
 * See ppg_hr.h for a full description of what was and was not changed
 * during the port. In short: none of the algorithm's math, constants,
 * or integer types were changed - only the storage of state was moved
 * from file-global statics into a per-instance context struct.
 */

#include "ppg_hr.h"
#include <string.h>

// -------------------------------------------------------------------------
// FIR FILTER COEFFICIENTS (SparkFun heartRate.cpp, unmodified)
// -------------------------------------------------------------------------

static const uint16_t FIRCoeffs[12] = {
    172, 321, 579, 927, 1360, 1858, 2390, 2916, 3391, 3768, 4012, 4096
};

// -------------------------------------------------------------------------
// INTERNAL HELPERS (SparkFun heartRate.cpp, unmodified math)
// -------------------------------------------------------------------------

/**
 * Average DC Estimator - ported unmodified from SparkFun heartRate.cpp.
 * Original signature: int16_t averageDCEstimator(int32_t *p, uint16_t x)
 */
static int16_t averageDCEstimator(int32_t *p, uint16_t x) {
    *p += ((((int32_t)x << 15) - *p) >> 4);
    return (int16_t)(*p >> 15);
}

/**
 * Integer multiplier - ported unmodified from SparkFun heartRate.cpp.
 * Original signature: int32_t mul16(int16_t x, int16_t y)
 */
static int32_t mul16(int16_t x, int16_t y) {
    return ((int32_t)x * (int32_t)y);
}

/**
 * Low Pass FIR Filter - ported unmodified from SparkFun heartRate.cpp.
 * Original signature: int16_t lowPassFIRFilter(int16_t din)
 *
 * Only change: cbuf/offset moved from file-global statics into *hr so
 * multiple detector instances don't share filter history.
 */
static int16_t lowPassFIRFilter(ppg_hr_t *hr, int16_t din) {
    hr->cbuf[hr->offset] = din;

    int32_t z = mul16(FIRCoeffs[11], hr->cbuf[(hr->offset - 11) & 0x1F]);

    for (uint8_t i = 0; i < 11; i++) {
        z += mul16(FIRCoeffs[i],
                    (int16_t)(hr->cbuf[(hr->offset - i) & 0x1F] +
                               hr->cbuf[(hr->offset - 22 + i) & 0x1F]));
    }

    hr->offset++;
    hr->offset %= 32; // Wrap condition

    return (int16_t)(z >> 15);
}

// -------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION
// -------------------------------------------------------------------------

void ppg_hr_init(ppg_hr_t *hr) {
    if (!hr) return;

    // --- PBA state: same initial values as SparkFun's global statics ---
    hr->IR_AC_Max = 20;
    hr->IR_AC_Min = -20;
    hr->IR_AC_Signal_Current = 0;
    hr->IR_AC_Signal_Previous = 0; // original leaves this uninitialized;
                                    // zeroing is the safe, deterministic choice
    hr->IR_AC_Signal_min = 0;
    hr->IR_AC_Signal_max = 0;
    hr->IR_Average_Estimated = 0;  // original leaves this uninitialized too
    hr->positiveEdge = 0;
    hr->negativeEdge = 0;
    hr->ir_avg_reg = 0;
    memset(hr->cbuf, 0, sizeof(hr->cbuf));
    hr->offset = 0;

    // --- BPM averaging state ---
    memset(hr->rates, 0, sizeof(hr->rates));
    hr->rateSpot = 0;
    hr->lastBeat = 0;
    hr->beatsPerMinute = 0;
    hr->beatAvg = 0;
}

bool ppg_hr_check_for_beat(ppg_hr_t *hr, int32_t sample) {
    bool beatDetected = false;

    // Save current state
    hr->IR_AC_Signal_Previous = hr->IR_AC_Signal_Current;

    // Process next data sample
    // NOTE: sample is narrowed to uint16_t here exactly as in the
    // original (averageDCEstimator's parameter is uint16_t x) - this
    // is intentional, unmodified behavior from the source algorithm.
    hr->IR_Average_Estimated = averageDCEstimator(&hr->ir_avg_reg, (uint16_t)sample);
    hr->IR_AC_Signal_Current = lowPassFIRFilter(hr, (int16_t)(sample - hr->IR_Average_Estimated));

    // Detect positive zero crossing (rising edge)
    if ((hr->IR_AC_Signal_Previous < 0) && (hr->IR_AC_Signal_Current >= 0)) {
        hr->IR_AC_Max = hr->IR_AC_Signal_max; // Adjust our AC max and min
        hr->IR_AC_Min = hr->IR_AC_Signal_min;

        hr->positiveEdge = 1;
        hr->negativeEdge = 0;
        hr->IR_AC_Signal_max = 0;

        if ((hr->IR_AC_Max - hr->IR_AC_Min) > 20 && (hr->IR_AC_Max - hr->IR_AC_Min) < 1000) {
            // Heart beat!!!
            beatDetected = true;
        }
    }

    // Detect negative zero crossing (falling edge)
    if ((hr->IR_AC_Signal_Previous > 0) && (hr->IR_AC_Signal_Current <= 0)) {
        hr->positiveEdge = 0;
        hr->negativeEdge = 1;
        hr->IR_AC_Signal_min = 0;
    }

    // Find Maximum value in positive cycle
    if (hr->positiveEdge && (hr->IR_AC_Signal_Current > hr->IR_AC_Signal_Previous)) {
        hr->IR_AC_Signal_max = hr->IR_AC_Signal_Current;
    }

    // Find Minimum value in negative cycle
    if (hr->negativeEdge && (hr->IR_AC_Signal_Current < hr->IR_AC_Signal_Previous)) {
        hr->IR_AC_Signal_min = hr->IR_AC_Signal_Current;
    }

    return beatDetected;
}

ppg_hr_result_t ppg_hr_process(ppg_hr_t *hr, int32_t ir_sample, uint32_t now_ms) {
    ppg_hr_result_t result;
    result.beat_detected = false;
    result.bpm = hr->beatsPerMinute;
    result.avg_bpm = hr->beatAvg;

    if (ppg_hr_check_for_beat(hr, ir_sample) == true) {
        uint32_t delta = now_ms - hr->lastBeat;
        hr->lastBeat = now_ms;

        // Integer form of SparkFun's `60 / (delta / 1000.0)`, which is
        // mathematically identical: 60 / (delta_ms/1000) == 60000 / delta_ms.
        // Guard against delta == 0 (e.g. very first beat, or a caller
        // supplying a non-advancing clock) to avoid a divide-by-zero,
        // which the original float version silently produced Inf/NaN for.
        if (delta > 0) {
            hr->beatsPerMinute = 60000 / (int32_t)delta;

            if (hr->beatsPerMinute < 255 && hr->beatsPerMinute > 20) {
                hr->rates[hr->rateSpot++] = (uint8_t)hr->beatsPerMinute;
                hr->rateSpot %= PPG_HR_RATE_SIZE;

                hr->beatAvg = 0;
                for (uint8_t x = 0; x < PPG_HR_RATE_SIZE; x++) {
                    hr->beatAvg += hr->rates[x];
                }
                hr->beatAvg /= PPG_HR_RATE_SIZE;
            }

            result.beat_detected = true;
            result.bpm = hr->beatsPerMinute;
            result.avg_bpm = hr->beatAvg;
        }
    }

    return result;
}