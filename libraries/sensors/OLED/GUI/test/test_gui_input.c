/**
 * @file    test_gui_input.c
 * @brief   Unit test: proves that the porting layer successfully
 *          translates raw touch data into GUI actions (click, swipe,
 *          double-click) via uGUI + Gesture Engine.
 *
 * Compile & run:
 *   cd projectICDEC/GUI/test
 *   make test
 *
 * Technique: gesture_tick_ms() is defined here as a simulated clock
 * (instead of usleep), allowing perfect timing control.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../ugui/ugui.h"
#include "../gesture.h"

/* =========================================================================
 * Simulated Clock — overrides gesture_tick_ms() in gui_port.c
 * We define it here to have 100% control over the time.
 * ========================================================================= */
static uint32_t g_sim_ms = 0;

uint32_t gesture_tick_ms(void) { return g_sim_ms; }
static void tick(uint32_t ms)  { g_sim_ms += ms;  }

/* =========================================================================
 * uGUI Porting (inlined here to avoid gesture_tick_ms double definition)
 * ========================================================================= */
static UG_GUI gui;
static UG_DEVICE device;

static void mock_pset(UG_S16 x, UG_S16 y, UG_COLOR c) {
    /* Does not draw anything — this is just for logic unit testing */
    (void)x; (void)y; (void)c;
}

static void port_init(void) {
    device.x_dim = 128;
    device.y_dim = 64;
    device.pset  = mock_pset;
    device.flush = NULL;
    UG_Init(&gui, &device);
    gesture_init();
}

/** Forwards touch data to uGUI AND Gesture Engine (core of the porting) */
static void port_feed(bool pressed, int16_t x, int16_t y) {
    UG_TouchUpdate(x, y, pressed ? TOUCH_STATE_PRESSED : TOUCH_STATE_RELEASED);
    gesture_feed(pressed, x, y);
}

static gesture_event_t drain(void)
{
    gesture_event_t ev = {0};
    gesture_poll(&ev);
    return ev;
}

/* =========================================================================
 * Mini Test Framework (ASSERT)
 * ========================================================================= */
static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, msg)                                                   \
    do {                                                                     \
        if (cond) { printf("  PASS: %s\n", msg); g_pass++;                  \
        } else    { printf("  FAIL: %s  (line %d)\n", msg, __LINE__);       \
                    g_fail++; }                                               \
    } while (0)

/* =========================================================================
 * Helper: simulate touch (press → hold → release)
 * ========================================================================= */
static void touch(int16_t sx, int16_t sy,
                  uint32_t hold_ms,
                  int16_t ex, int16_t ey)
{
    port_feed(true,  sx, sy);
    tick(hold_ms / 2);
    port_feed(true,  ex, ey);     /* Simulate finger movement */
    tick(hold_ms - hold_ms / 2);
    port_feed(false, ex, ey);
}

/* =========================================================================
 * Test Scenarios (10 Cases)
 * ========================================================================= */

static void test_idle_no_event(void)
{
    printf("\n[test_idle_no_event]\n");
    port_init();
    g_sim_ms = 0;

    gesture_event_t ev;
    ASSERT(!gesture_poll(&ev), "no event when idle");
}

static void test_click(void)
{
    printf("\n[test_click]\n");
    port_init();
    g_sim_ms = 0;

    touch(64, 32, 100, 64, 32);
    tick(GESTURE_DCLICK_WINDOW_MS + 1); /* Let double-click window expire */
    port_feed(false, 0, 0);          /* One more tick to run expiry logic */

    UG_Update(); /* Prove uGUI doesn't crash */

    gesture_event_t ev;
    bool got = gesture_poll(&ev);

    ASSERT(got,                      "event received");
    ASSERT(ev.type == GESTURE_CLICK, "type == CLICK");
    ASSERT(ev.start_x == 64,        "start_x correct");
    ASSERT(ev.start_y == 32,        "start_y correct");
    ASSERT(ev.duration_ms <= 110,   "duration within range");
}

static void test_double_click(void)
{
    printf("\n[test_double_click]\n");
    port_init();
    g_sim_ms = 0;

    touch(64, 32, 80, 64, 32);         /* First tap */
    tick(150);                          /* Gap < GESTURE_DCLICK_WINDOW_MS */
    touch(64, 32, 80, 64, 32);         /* Second tap */
    tick(10);

    UG_Update();

    gesture_event_t ev;
    bool got = gesture_poll(&ev);

    ASSERT(got,                              "event received");
    ASSERT(ev.type == GESTURE_DOUBLE_CLICK,  "type == DOUBLE_CLICK");
}

static void test_double_click_window_expired(void)
{
    printf("\n[test_double_click_window_expired]\n");
    port_init();
    g_sim_ms = 0;

    touch(64, 32, 80, 64, 32);
    tick(GESTURE_DCLICK_WINDOW_MS + 100);  /* Window expires */
    port_feed(false, 0, 0);             /* Trigger expiry in gesture_poll */

    gesture_event_t ev;
    bool got = gesture_poll(&ev);

    ASSERT(got,                      "event received");
    ASSERT(ev.type == GESTURE_CLICK, "type == CLICK (not double, window expired)");
}

static void test_long_press(void)
{
    printf("\n[test_long_press]\n");
    port_init();
    g_sim_ms = 0;

    touch(64, 32, GESTURE_LONG_PRESS_MS + 200, 65, 32);

    gesture_event_t ev;
    bool got = gesture_poll(&ev);

    ASSERT(got,                            "event received");
    ASSERT(ev.type == GESTURE_LONG_PRESS,  "type == LONG_PRESS");
    ASSERT(ev.duration_ms >= GESTURE_LONG_PRESS_MS, "duration >= threshold");
}

static void test_swipe_left(void)
{
    printf("\n[test_swipe_left]\n");
    port_init();
    g_sim_ms = 0;

    touch(100, 32, 150, 30, 32);   /* Move 70 px left */

    UG_Update();

    gesture_event_t ev = drain();
    ASSERT(ev.type == GESTURE_SWIPE_LEFT,         "type == SWIPE_LEFT");
    ASSERT(ev.delta_x < -GESTURE_SWIPE_MIN_PX,   "delta_x negative");
}

static void test_swipe_right(void)
{
    printf("\n[test_swipe_right]\n");
    port_init();
    g_sim_ms = 0;

    touch(20, 32, 150, 100, 32);

    gesture_event_t ev = drain();
    ASSERT(ev.type == GESTURE_SWIPE_RIGHT,        "type == SWIPE_RIGHT");
    ASSERT(ev.delta_x > GESTURE_SWIPE_MIN_PX,    "delta_x positive");
}

static void test_swipe_up(void)
{
    printf("\n[test_swipe_up]\n");
    port_init();
    g_sim_ms = 0;

    touch(64, 55, 150, 64, 5);

    gesture_event_t ev = drain();
    ASSERT(ev.type == GESTURE_SWIPE_UP, "type == SWIPE_UP");
}

static void test_swipe_down(void)
{
    printf("\n[test_swipe_down]\n");
    port_init();
    g_sim_ms = 0;

    touch(64, 5, 150, 64, 55);

    gesture_event_t ev = drain();
    ASSERT(ev.type == GESTURE_SWIPE_DOWN, "type == SWIPE_DOWN");
}

static void test_click_tiny_drift(void)
{
    printf("\n[test_click_tiny_drift]\n");
    port_init();
    g_sim_ms = 0;

    /* 5 px drift — within GESTURE_CLICK_MAX_PX → still a click */
    touch(64, 32, 100, 69, 32);
    tick(GESTURE_DCLICK_WINDOW_MS + 1);
    port_feed(false, 0, 0);

    gesture_event_t ev;
    bool got = gesture_poll(&ev);
    ASSERT(got,                      "event received");
    ASSERT(ev.type == GESTURE_CLICK, "small drift still CLICK");
}

static void test_type_str(void)
{
    printf("\n[test_type_str]\n");
    ASSERT(strcmp(gesture_type_str(GESTURE_NONE),         "NONE")         == 0, "NONE");
    ASSERT(strcmp(gesture_type_str(GESTURE_CLICK),        "CLICK")        == 0, "CLICK");
    ASSERT(strcmp(gesture_type_str(GESTURE_DOUBLE_CLICK), "DOUBLE_CLICK") == 0, "DOUBLE_CLICK");
    ASSERT(strcmp(gesture_type_str(GESTURE_LONG_PRESS),   "LONG_PRESS")   == 0, "LONG_PRESS");
    ASSERT(strcmp(gesture_type_str(GESTURE_SWIPE_LEFT),   "SWIPE_LEFT")   == 0, "SWIPE_LEFT");
    ASSERT(strcmp(gesture_type_str(GESTURE_SWIPE_RIGHT),  "SWIPE_RIGHT")  == 0, "SWIPE_RIGHT");
    ASSERT(strcmp(gesture_type_str(GESTURE_SWIPE_UP),     "SWIPE_UP")     == 0, "SWIPE_UP");
    ASSERT(strcmp(gesture_type_str(GESTURE_SWIPE_DOWN),   "SWIPE_DOWN")   == 0, "SWIPE_DOWN");
}

static void test_ugui_port(void)
{
    printf("\n[test_ugui_port]\n");
    port_init();
    ASSERT(UG_GetXDim() == 128,  "display width == 128");
    ASSERT(UG_GetYDim() == 64,   "display height == 64");
    UG_FillScreen(C_BLACK);
    UG_DrawLine(0, 0, 127, 63, C_WHITE);
    UG_Update();
    ASSERT(true, "uGUI rendering does not crash");
}


/* =========================================================================
 * Main
 * ========================================================================= */
int main(void)
{
    printf("==========================================\n");
    printf("  UNIT TEST: GUI Input Porting (uGUI)\n");
    printf("==========================================\n");

    test_idle_no_event();
    test_click();
    test_double_click();
    test_double_click_window_expired();
    test_long_press();
    test_swipe_left();
    test_swipe_right();
    test_swipe_up();
    test_swipe_down();
    test_click_tiny_drift();
    test_type_str();
    test_ugui_port();

    /* Summary */
    printf("\n==========================================\n");
    printf("  %d PASSED, %d FAILED\n", g_pass, g_fail);
    printf("==========================================\n");

    return g_fail == 0 ? 0 : 1;
}
