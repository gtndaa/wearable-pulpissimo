#include "step_counter.h"

static int32_t magnitude_sq(int32_t x, int32_t y, int32_t z)
{
    // divide to prevent overflow int32
    // max mg per axis ~16000mg (±16g range), 16000/16=1000, 1000²=1000000
    int32_t xd = x / 16;
    int32_t yd = y / 16;
    int32_t zd = z / 16;
    return (xd * xd) + (yd * yd) + (zd * zd);
}
static int32_t moving_average(step_counter_t *sc, int32_t new_val)
{
    sc->window_sum -= sc->window[sc->window_idx];

    sc->window[sc->window_idx] = new_val;
    sc->window_sum += new_val; 

    sc->window_idx = (sc->window_idx + 1) % STEP_WINDOW_SIZE;

    if (sc->window_idx == 0) sc->window_full = 1; // flag window full

    int count = sc->window_full ? STEP_WINDOW_SIZE : sc->window_idx;
    if (count == 0) return new_val;

    return sc->window_sum / count;
}

void step_counter_init(step_counter_t *sc)
{
    int i;
    sc->step_count      = 0;
    sc->last_step_time_ms = 0;
    sc->window_idx      = 0;
    sc->window_sum      = 0;
    sc->window_full     = 0;
    sc->prev_avg        = 0;
    sc->above_threshold = 0;

    for (i = 0; i < STEP_WINDOW_SIZE; i++) {
        sc->window[i] = 0;
    }
}

int step_counter_update(step_counter_t *sc, int32_t x_mg, int32_t y_mg, int32_t z_mg, long now_ms)
{
    int32_t mag_sq = magnitude_sq(x_mg, y_mg, z_mg);
    int32_t avg = moving_average(sc, mag_sq);

    // STEP_THRESHOLD_MG2 / 256 = 1440000 / 256 = 5625
    const int32_t threshold = STEP_THRESHOLD_MG2 / 256;

    int step_detected = 0;

    if (!sc->above_threshold && avg > threshold) {
        sc->above_threshold = 1;

    } else if (sc->above_threshold && avg <= threshold) {
        sc->above_threshold = 0;

        // Debounce: ignore fast steps
        long dt = now_ms - sc->last_step_time_ms;
        if (dt >= STEP_DEBOUNCE_MS) {
            sc->step_count++;
            sc->last_step_time_ms = now_ms;
            step_detected = 1;
        }
    }

    sc->prev_avg = avg;
    return step_detected;
}

void step_counter_reset(step_counter_t *sc)
{
    sc->step_count        = 0;
    sc->last_step_time_ms = 0;
}

uint32_t step_counter_get(const step_counter_t *sc)
{
    return sc->step_count;
}

void day_tracker_init(day_tracker_t *dt)
{
    dt->last_date  = 0;
    dt->last_month = 0;
    dt->last_year  = 0;
    dt->valid      = 0;
}

int day_tracker_check_rollover(day_tracker_t *dt, i2c_t *i2c_handle)
{
    if (dt == NULL || i2c_handle == NULL) return RTC_ERR_NULL_PTR;

    rtc_time_t t;
    int ret = rtc_get_time(i2c_handle, &t);
    if (ret != RTC_OK) {
        return ret; // failed reading time from RTC
    }

    if (!dt->valid) {
        dt->last_date  = t.date;
        dt->last_month = t.month;
        dt->last_year  = t.year;
        dt->valid      = 1;
        return 0;
    }

    if (t.date != dt->last_date || t.month != dt->last_month || t.year != dt->last_year) {
        dt->last_date  = t.date;
        dt->last_month = t.month;
        dt->last_year  = t.year;
        return 1; // day changed
    }

    return 0;
}