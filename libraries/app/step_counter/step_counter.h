#ifndef STEP_COUNTER_H
#define STEP_COUNTER_H

#include <stdint.h>
#include "rtc.h"

// FOR TUNING
// Threshold in mg²
// current threshold: ~200-400mg from baseline
// threshold = (1000 + 200)² - 1000² = ~440000 mg²
// increase if too sensitive or decrease if too insensitive
#define STEP_THRESHOLD_MG2       1440000L   // (1200mg)²

// RELAX WALK 1.5-2 Steps/sec
#define STEP_DEBOUNCE_MS         250
#define STEP_WINDOW_SIZE         4

typedef struct {
    uint32_t    step_count;                     // detected step count
    long        last_step_time_ms;              // timestamp
    int32_t     window[STEP_WINDOW_SIZE];       // buffer moving average
    int         window_idx;                     // index buffer idx
    int32_t     window_sum;                     // buffer sum
    int         window_full;                    // full buffer flag
    int32_t     prev_avg;                       // previous moving average
    int         above_threshold;               // above threshold flag
} step_counter_t;

void step_counter_init(step_counter_t *sc);
int  step_counter_update(step_counter_t *sc, int32_t x_mg, int32_t y_mg, int32_t z_mg, long now_ms);
void step_counter_reset(step_counter_t *sc);
uint32_t step_counter_get(const step_counter_t *sc);

typedef struct {
    uint8_t last_date;
    uint8_t last_month;
    uint8_t last_year;
    int     valid;
} day_tracker_t;

void day_tracker_init(day_tracker_t *dt);
int day_tracker_check_rollover(day_tracker_t *dt, i2c_t *i2c_handle);

#endif 