/**
 * @file    gesture.c
 * @brief   Gesture recognition engine — implementation
 *
 * State machine:
 *
 *   IDLE ──(press)──► TRACKING ──(release)──► classify() ──► IDLE
 *                                                  │
 *                                           CLICK detected
 *                                                  │
 *                                     ┌────────────▼────────────┐
 *                                     │   STATE_AWAIT_DCLICK    │
 *                                     │   (second press keeps   │
 *                                     │    this state intact)   │
 *                                     └────────────┬────────────┘
 *                                       second tap │  timeout
 *                                    DOUBLE_CLICK ◄┤► CLICK (already emitted)
 *
 * Long-press detection:
 *   - Primary:  detected while still held (emitted immediately at threshold).
 *   - Fallback: detected in classify() on release, in case the read_cb
 *               wasn't polled during the hold (important for bare-metal where
 *               the polling interval may be coarser than the LP threshold).
 */

#include "gesture.h"
#include <string.h>

/* =========================================================================
 * Helpers
 * ========================================================================= */
static inline int16_t abs16(int16_t v) { return v < 0 ? (int16_t)-v : v; }

/* =========================================================================
 * Internal state
 * ========================================================================= */
typedef enum {
    STATE_IDLE = 0,
    STATE_TRACKING,
    STATE_AWAIT_DCLICK
} engine_state_t;

static engine_state_t g_state;
static bool           g_was_pressed;
static bool           g_long_press_fired;

/* Recorded at press-down */
static int16_t  g_start_x;
static int16_t  g_start_y;
static uint32_t g_press_ms;

/* Updated during contact */
static int16_t  g_cur_x;
static int16_t  g_cur_y;

/* Double-click bookkeeping */
static uint32_t g_last_release_ms;

/* Pending output */
static gesture_event_t g_pending;
static bool            g_event_ready;

/* =========================================================================
 * Emit
 * ========================================================================= */
static void emit(gesture_type_t type,
                 int16_t sx, int16_t sy,
                 int16_t ex, int16_t ey,
                 uint32_t dur_ms)
{
    g_pending.type        = type;
    g_pending.start_x     = sx;
    g_pending.start_y     = sy;
    g_pending.end_x       = ex;
    g_pending.end_y       = ey;
    g_pending.delta_x     = (int16_t)(ex - sx);
    g_pending.delta_y     = (int16_t)(ey - sy);
    g_pending.duration_ms = dur_ms;
    g_event_ready         = true;
}

/* =========================================================================
 * Classify a completed contact (called on release)
 * ========================================================================= */
static void classify(int16_t ex, int16_t ey, uint32_t release_ms)
{
    uint32_t dur  = release_ms - g_press_ms;
    int16_t  dx   = (int16_t)(ex - g_start_x);
    int16_t  dy   = (int16_t)(ey - g_start_y);
    int16_t  adx  = abs16(dx);
    int16_t  ady  = abs16(dy);
    int16_t  dist = (int16_t)(adx > ady ? adx : ady);

    /* Long-press was already emitted while held — nothing more to do */
    if (g_long_press_fired) {
        g_state = STATE_IDLE;
        return;
    }

    /* ── Swipe: movement dominates ─────────────────────────────── */
    if (dist >= GESTURE_SWIPE_MIN_PX) {
        gesture_type_t dir;
        if (adx >= ady)
            dir = (dx < 0) ? GESTURE_SWIPE_LEFT  : GESTURE_SWIPE_RIGHT;
        else
            dir = (dy < 0) ? GESTURE_SWIPE_UP    : GESTURE_SWIPE_DOWN;
        emit(dir, g_start_x, g_start_y, ex, ey, dur);
        g_state = STATE_IDLE;
        return;
    }

    /* ── Long press (fallback): held too long, minimal movement ─── */
    if (dur >= GESTURE_LONG_PRESS_MS && dist <= GESTURE_CLICK_MAX_PX) {
        emit(GESTURE_LONG_PRESS, g_start_x, g_start_y, ex, ey, dur);
        g_state = STATE_IDLE;
        return;
    }

    /* ── Click / double-click: short, stationary ────────────────── */
    if (dur <= GESTURE_CLICK_MAX_MS && dist <= GESTURE_CLICK_MAX_PX) {

        if (g_state == STATE_AWAIT_DCLICK) {
            uint32_t gap = release_ms - g_last_release_ms;
            if (gap <= GESTURE_DCLICK_WINDOW_MS) {
                emit(GESTURE_DOUBLE_CLICK,
                     g_start_x, g_start_y, ex, ey, dur);
                g_state = STATE_IDLE;
                return;
            }
        }

        /* First (or lone) click */
        emit(GESTURE_CLICK, g_start_x, g_start_y, ex, ey, dur);
        g_last_release_ms = release_ms;
        g_state           = STATE_AWAIT_DCLICK;
        return;
    }

    g_state = STATE_IDLE;
}

/* =========================================================================
 * Public API
 * ========================================================================= */
void gesture_init(void)
{
    memset(&g_pending, 0, sizeof(g_pending));
    g_state            = STATE_IDLE;
    g_was_pressed      = false;
    g_long_press_fired = false;
    g_event_ready      = false;
    g_last_release_ms  = 0;
}

void gesture_feed(bool pressed, int16_t x, int16_t y)
{
    uint32_t now = gesture_tick_ms();

    /* ── Rising edge ─────────────────────────────────────────────── */
    if (pressed && !g_was_pressed) {
        g_start_x          = x;
        g_start_y          = y;
        g_cur_x            = x;
        g_cur_y            = y;
        g_press_ms         = now;
        g_long_press_fired = false;

        /*
         * KEY FIX: if we're waiting for a second tap (STATE_AWAIT_DCLICK),
         * do NOT overwrite that state with STATE_TRACKING.
         * classify() checks g_state == STATE_AWAIT_DCLICK to detect
         * the second tap of a double-click.
         */
        if (g_state != STATE_AWAIT_DCLICK) {
            g_state = STATE_TRACKING;
        }
        /* else: stay in STATE_AWAIT_DCLICK — classify() will handle it */
    }

    /* ── Sustained contact: update position, check long press ────── */
    if (pressed && g_was_pressed) {
        g_cur_x = x;
        g_cur_y = y;

        /* Emit long press immediately when threshold is crossed */
        if (!g_long_press_fired &&
            (now - g_press_ms) >= GESTURE_LONG_PRESS_MS) {
            int16_t adx = abs16((int16_t)(x - g_start_x));
            int16_t ady = abs16((int16_t)(y - g_start_y));
            if (adx <= GESTURE_CLICK_MAX_PX && ady <= GESTURE_CLICK_MAX_PX) {
                emit(GESTURE_LONG_PRESS,
                     g_start_x, g_start_y, x, y,
                     now - g_press_ms);
                g_long_press_fired = true;
                g_state            = STATE_IDLE;
            }
        }
    }

    /* ── Falling edge ────────────────────────────────────────────── */
    if (!pressed && g_was_pressed) {
        classify(g_cur_x, g_cur_y, now);
    }

    g_was_pressed = pressed;
}

bool gesture_poll(gesture_event_t *event)
{
    uint32_t now = gesture_tick_ms();

    /* Double-click window expiry: if the window closes with no second tap,
     * the CLICK was already emitted in classify(); just reset state. */
    if (g_state == STATE_AWAIT_DCLICK &&
        (now - g_last_release_ms) > GESTURE_DCLICK_WINDOW_MS) {
        g_state = STATE_IDLE;
    }

    if (!g_event_ready) return false;
    *event        = g_pending;
    g_event_ready = false;
    return true;
}

const char *gesture_type_str(gesture_type_t type)
{
    switch (type) {
        case GESTURE_NONE:         return "NONE";
        case GESTURE_CLICK:        return "CLICK";
        case GESTURE_DOUBLE_CLICK: return "DOUBLE_CLICK";
        case GESTURE_LONG_PRESS:   return "LONG_PRESS";
        case GESTURE_SWIPE_LEFT:   return "SWIPE_LEFT";
        case GESTURE_SWIPE_RIGHT:  return "SWIPE_RIGHT";
        case GESTURE_SWIPE_UP:     return "SWIPE_UP";
        case GESTURE_SWIPE_DOWN:   return "SWIPE_DOWN";
        default:                   return "UNKNOWN";
    }
}
