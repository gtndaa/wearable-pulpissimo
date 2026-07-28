/**
 * @file ppg_hr.h
 * @brief PPG Heart Rate Detection - Public API
 *
 * This library is a direct, unmodified port of SparkFun's Optical Heart
 * Rate Detection algorithm (PBA - Peripheral Beat Amplitude), originally
 * implemented by Maxim Integrated / Nathan Seidle in
 * SparkFun_MAX3010x_Sensor_Library/src/heartRate.cpp, plus the BPM
 * averaging logic from that library's Example5_HeartRate.ino.
 *
 * PORTING NOTES (what changed vs. what did not):
 *   - The beat-detection math (averageDCEstimator, lowPassFIRFilter,
 *     mul16, checkForBeat, all FIR coefficients, all thresholds) is
 *     copied EXACTLY. No constant, no operator, no type width was
 *     changed.
 *   - The ORIGINAL SparkFun code keeps its filter state in global
 *     `static` variables, which only works for a single, global
 *     detector instance. Here that same state is grouped into a
 *     `ppg_hr_t` context struct so the library is reentrant (e.g. if
 *     you ever need two channels), following the same pattern as
 *     max30102_t in this codebase. This is a structural change only -
 *     it does not alter the algorithm's behavior in any way.
 *   - The original algorithm is already 100% integer (int16_t/int32_t),
 *     so it already satisfies the "no floating point" hardware
 *     constraint with zero changes needed.
 *   - The BPM calculation in SparkFun's example sketch used
 *     `beatsPerMinute = 60 / (delta / 1000.0)` (float), then truncated
 *     it into a byte. That is mathematically identical to the integer
 *     expression `60000 / delta_ms` truncated the same way, so this
 *     port uses the integer form to avoid floating point while
 *     producing the exact same result.
 *
 * This library is explicitly NOT responsible for:
 *   - I2C / sensor register access (see max30102.h/.c)
 *   - DC blocking / band-pass filtering upstream of this stage
 *   - SpO2 estimation
 *
 * It consumes raw (or upstream-filtered) IR samples, one at a time,
 * and reports beat detection + instantaneous/average BPM.
 */

#ifndef PPG_HR_H
#define PPG_HR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// -------------------------------------------------------------------------
// CONFIGURATION CONSTANTS
// -------------------------------------------------------------------------

/** Number of beat intervals averaged for beatAvg (matches SparkFun's RATE_SIZE = 4). */
#define PPG_HR_RATE_SIZE   4

/** Size of the FIR circular buffer (matches SparkFun's cbuf[32]). */
#define PPG_HR_CBUF_SIZE   32

// -------------------------------------------------------------------------
// CONTEXT STRUCTURE
// -------------------------------------------------------------------------

/**
 * @brief PPG heart-rate detector state.
 *
 * All fields below mirror, one-to-one, the global static variables in
 * SparkFun's heartRate.cpp and the RATE_SIZE / rates[] / rateSpot /
 * lastBeat / beatsPerMinute / beatAvg variables from Example5_HeartRate.ino.
 * Field names and types are kept identical to the originals wherever
 * possible so the algorithm can be visually diffed against the source.
 */
typedef struct {
    // --- PBA beat-detection state (SparkFun heartRate.cpp, unmodified) ---
    int16_t IR_AC_Max;
    int16_t IR_AC_Min;
    int16_t IR_AC_Signal_Current;
    int16_t IR_AC_Signal_Previous;
    int16_t IR_AC_Signal_min;
    int16_t IR_AC_Signal_max;
    int16_t IR_Average_Estimated;
    int16_t positiveEdge;
    int16_t negativeEdge;
    int32_t ir_avg_reg;
    int16_t cbuf[PPG_HR_CBUF_SIZE];
    uint8_t offset;

    // --- BPM averaging state (SparkFun Example5_HeartRate.ino, unmodified) ---
    uint8_t rates[PPG_HR_RATE_SIZE];
    uint8_t rateSpot;
    uint32_t lastBeat;       ///< timestamp (ms) of the last detected beat
    int32_t beatsPerMinute;  ///< instantaneous BPM of the most recent beat
    int32_t beatAvg;         ///< moving average BPM over PPG_HR_RATE_SIZE beats
} ppg_hr_t;

/**
 * @brief Result of processing one sample through the full pipeline
 *        (beat detection + BPM averaging).
 */
typedef struct {
    bool beat_detected;  ///< true if a beat was detected on this sample
    int32_t bpm;          ///< instantaneous BPM (0 if no beat yet / rejected)
    int32_t avg_bpm;      ///< current moving-average BPM
} ppg_hr_result_t;

// -------------------------------------------------------------------------
// PUBLIC API
// -------------------------------------------------------------------------

/**
 * @brief Initialize / reset a detector context to the same initial state
 *        SparkFun's heartRate.cpp globals start in (IR_AC_Max = 20,
 *        IR_AC_Min = -20, everything else zeroed) plus a cleared BPM
 *        averaging state.
 *
 * @param hr Pointer to detector context
 */
void ppg_hr_init(ppg_hr_t *hr);

/**
 * @brief Direct port of SparkFun's checkForBeat(). Algorithm unmodified.
 *
 * Feeds one raw IR sample through the DC estimator and 12-tap FIR low
 * pass filter, tracks the AC signal's zero crossings and peak/trough
 * amplitude, and reports a beat on a qualifying rising edge (AC swing
 * strictly between 20 and 1000 counts) - exactly as in the original.
 *
 * @param hr Pointer to detector context
 * @param sample Raw IR sample (only the low 16 bits are used internally,
 *               exactly as in the original, which takes a uint16_t
 *               inside averageDCEstimator())
 * @return true if a beat was detected on this sample
 */
bool ppg_hr_check_for_beat(ppg_hr_t *hr, int32_t sample);

/**
 * @brief Full pipeline: beat detection + BPM instantaneous/average update,
 *        equivalent to the body of SparkFun's example loop() that surrounds
 *        checkForBeat().
 *
 * Because this platform's millis()/micros() timer bring-up is a separate,
 * outstanding hardware issue (see project notes), the caller supplies the
 * current timestamp explicitly rather than this library reading a system
 * timer itself. Pass any monotonically increasing millisecond counter.
 *
 * @param hr Pointer to detector context
 * @param ir_sample Raw IR sample for this tick
 * @param now_ms Current time in milliseconds (monotonic, caller-supplied)
 * @return ppg_hr_result_t with beat_detected flag, instantaneous BPM, and
 *         the current moving-average BPM
 */
ppg_hr_result_t ppg_hr_process(ppg_hr_t *hr, int32_t ir_sample, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif // PPG_HR_H