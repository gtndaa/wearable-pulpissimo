/**
 * @file    gesture.h
 * @brief   Gesture recognition engine — pure C, no LVGL dependency
 *
 * Converts a stream of raw touch samples (pressed/x/y) into discrete
 * gesture events: CLICK, DOUBLE_CLICK, LONG_PRESS, SWIPE_*.
 *
 * Because this module has zero dependencies on LVGL or any hardware
 * API, it can be compiled and unit-tested on any host.
 *
 * Integration:
 *   1.  Call gesture_init() once.
 *   2.  Call gesture_feed() on every new touch sample (both platforms).
 *   3.  Call gesture_poll() from a periodic callback to get pending events.
 *
 * The engine needs a millisecond time source.  Provide it by implementing:
 *   uint32_t gesture_tick_ms(void);   ← defined by the caller
 */

#ifndef GESTURE_H
#define GESTURE_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * Gesture types
 * ========================================================================= */

typedef enum {
    GESTURE_NONE         = 0,
    GESTURE_CLICK,            /**< Short tap, minimal movement             */
    GESTURE_DOUBLE_CLICK,     /**< Two taps within GESTURE_DCLICK_WINDOW   */
    GESTURE_LONG_PRESS,       /**< Held for ≥ GESTURE_LONG_PRESS_MS        */
    GESTURE_SWIPE_LEFT,       /**< Right → Left                            */
    GESTURE_SWIPE_RIGHT,      /**< Left → Right                            */
    GESTURE_SWIPE_UP,         /**< Down → Up                               */
    GESTURE_SWIPE_DOWN        /**< Up → Down                               */
} gesture_type_t;

/* =========================================================================
 * Tunable thresholds
 * ========================================================================= */

/** Maximum contact time (ms) for a tap to be counted as a click */
#define GESTURE_CLICK_MAX_MS       300

/** Minimum hold time (ms) for a long press */
#define GESTURE_LONG_PRESS_MS      600

/** Maximum inter-tap gap (ms) for a double-click */
#define GESTURE_DCLICK_WINDOW_MS   400

/** Maximum position drift (px) for a tap to remain a click (not a swipe) */
#define GESTURE_CLICK_MAX_PX        10

/** Minimum displacement (px) to classify as a swipe */
#define GESTURE_SWIPE_MIN_PX        20

/* =========================================================================
 * Event type
 * ========================================================================= */

typedef struct {
    gesture_type_t type;

    int16_t  start_x;     /**< Touch origin X                    */
    int16_t  start_y;     /**< Touch origin Y                    */
    int16_t  end_x;       /**< Touch release X                   */
    int16_t  end_y;       /**< Touch release Y                   */
    int16_t  delta_x;     /**< end_x − start_x (+ = right)       */
    int16_t  delta_y;     /**< end_y − start_y (+ = down)        */
    uint32_t duration_ms; /**< Contact duration                  */
} gesture_event_t;

/* =========================================================================
 * Tick source — implement in the platform layer (main.c or input_driver.c)
 * ========================================================================= */

/**
 * @brief Returns elapsed milliseconds since system start.
 *
 * Linux:   implement using clock_gettime(CLOCK_MONOTONIC)
 * RISC-V:  implement using a hardware timer or the tick counter in main.c
 */
uint32_t gesture_tick_ms(void);

/* =========================================================================
 * Public API
 * ========================================================================= */

/** Initialise (or reset) the gesture engine. Call once at startup. */
void gesture_init(void);

/**
 * @brief Feed a raw touch sample into the engine.
 *
 * Call on every new reading from the touch hardware (or SDL mouse event).
 *
 * @param pressed  true = finger on screen / mouse button held
 * @param x        Horizontal position (pixels, 0 = left edge)
 * @param y        Vertical position   (pixels, 0 = top edge)
 */
void gesture_feed(bool pressed, int16_t x, int16_t y);

/**
 * @brief Poll for a completed gesture event.
 *
 * Non-blocking.  Clears the event after reading.
 *
 * @param[out] event  Filled on return if an event is available.
 * @return            true if an event was ready, false if none.
 */
bool gesture_poll(gesture_event_t *event);

/** Human-readable string for a gesture type (for logging / tests). */
const char *gesture_type_str(gesture_type_t type);

#endif /* GESTURE_H */
