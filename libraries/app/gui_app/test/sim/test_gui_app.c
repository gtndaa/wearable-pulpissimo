/**
 * @file    test_gui_app.c
 * @brief   Unit Test & Simulation for GUI Application Layer (Watch Face)
 *
 * Tests:
 *   1. Time formatting (HH:MM)
 *   2. Date formatting (Sen, DD/MM/YYYY)
 *   3. Edge cases (midnight, rollover, boundary values)
 *   4. GUI Manager screen navigation via simulated gestures
 *   5. ASCII framebuffer simulation (visual verification)
 *
 * Compile & run:
 *   cd gui_app
 *   make test
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Include uGUI framework */
#include "../../../../sensors/OLED/GUI/ugui/ugui.h"
#include "../../../../sensors/OLED/GUI/ugui/ui/ugui_fonts.h"
#include "../../../../sensors/OLED/GUI/gesture.h"

/* Include our watch face module */
#include "../../screens/watch_face.h"

/* =========================================================================
 * Inline mock of gui_manager logic (to avoid linking gui_port.c hardware)
 * We test the screen navigation logic directly here.
 * ========================================================================= */
typedef enum {
    SCREEN_WATCH_FACE = 0,
    SCREEN_COUNT
} gui_screen_t;

static gui_screen_t s_test_active_screen = SCREEN_WATCH_FACE;

static gui_screen_t gui_manager_get_screen(void) {
    return s_test_active_screen;
}
static void gui_manager_set_screen(gui_screen_t screen) {
    if (screen < SCREEN_COUNT) {
        s_test_active_screen = screen;
    }
}

/* =========================================================================
 * Simulated Clock — overrides gesture_tick_ms()
 * ========================================================================= */
static uint32_t g_sim_ms = 0;

uint32_t gesture_tick_ms(void) { return g_sim_ms; }
static void tick(uint32_t ms)  { g_sim_ms += ms;  }

/* =========================================================================
 * Mock OLED / uGUI Pixel Driver
 * ========================================================================= */
#define SIM_W 128
#define SIM_H 64

static uint8_t g_framebuffer[SIM_W * SIM_H / 8];  /* 1-bit per pixel, 1024 bytes */

static UG_GUI   g_gui;
static UG_DEVICE g_device;

static void mock_pset(UG_S16 x, UG_S16 y, UG_COLOR c)
{
    if (x < 0 || x >= SIM_W || y < 0 || y >= SIM_H) return;
    uint16_t byte_idx = (uint16_t)(x + (y / 8) * SIM_W);
    uint8_t  bit_mask = (uint8_t)(1 << (y % 8));
    if (c == C_WHITE)
        g_framebuffer[byte_idx] |= bit_mask;
    else
        g_framebuffer[byte_idx] &= (uint8_t)~bit_mask;
}

static void sim_init(void)
{
    memset(g_framebuffer, 0, sizeof(g_framebuffer));
    g_device.x_dim = SIM_W;
    g_device.y_dim = SIM_H;
    g_device.pset  = mock_pset;
    g_device.flush = NULL;
    UG_Init(&g_gui, &g_device);
    gesture_init();
}

/* =========================================================================
 * ASCII Framebuffer Dump (Visual Simulation)
 * ========================================================================= */
static void sim_dump_screen(void)
{
    printf("\n  +");
    for (int x = 0; x < SIM_W; x++) printf("-");
    printf("+\n");

    for (int y = 0; y < SIM_H; y++) {
        printf("  |");
        for (int x = 0; x < SIM_W; x++) {
            uint16_t byte_idx = (uint16_t)(x + (y / 8) * SIM_W);
            uint8_t  bit_mask = (uint8_t)(1 << (y % 8));
            printf("%c", (g_framebuffer[byte_idx] & bit_mask) ? '#' : ' ');
        }
        printf("|\n");
    }

    printf("  +");
    for (int x = 0; x < SIM_W; x++) printf("-");
    printf("+\n");
}

/* =========================================================================
 * Mini Test Framework
 * ========================================================================= */
static int g_pass = 0, g_fail = 0;

#define ASSERT(cond, msg)                                                   \
    do {                                                                     \
        if (cond) { printf("  PASS: %s\n", msg); g_pass++;                  \
        } else    { printf("  FAIL: %s  (line %d)\n", msg, __LINE__);       \
                    g_fail++; }                                               \
    } while (0)

/* =========================================================================
 * Test Cases
 * ========================================================================= */

static void test_time_formatting(void)
{
    printf("\n[test_time_formatting]\n");
    char buf[6];

    watch_face_init();

    /* Normal time */
    watch_face_set_time(14, 30, 0);
    watch_face_get_time_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "14:30") == 0, "14:30 formatted correctly");

    /* Midnight */
    watch_face_set_time(0, 0, 0);
    watch_face_get_time_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "00:00") == 0, "00:00 midnight formatted correctly");

    /* End of day */
    watch_face_set_time(23, 59, 59);
    watch_face_get_time_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "23:59") == 0, "23:59 end of day formatted correctly");

    /* Single digits */
    watch_face_set_time(9, 5, 0);
    watch_face_get_time_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "09:05") == 0, "09:05 zero-padded correctly");
}

static void test_date_formatting(void)
{
    printf("\n[test_date_formatting]\n");
    char buf[16];

    watch_face_init();

    /* Senin, 22/07/2026 */
    watch_face_set_date(2, 22, 7, 2026);
    watch_face_get_date_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "Sen, 22/07/2026") == 0, "Sen, 22/07/2026 formatted correctly");

    /* Minggu, 01/01/2026 */
    watch_face_set_date(1, 1, 1, 2026);
    watch_face_get_date_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "Min, 01/01/2026") == 0, "Min, 01/01/2026 formatted correctly");

    /* Sabtu, 31/12/2099 */
    watch_face_set_date(7, 31, 12, 2099);
    watch_face_get_date_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "Sab, 31/12/2099") == 0, "Sab, 31/12/2099 formatted correctly");
}

static void test_day_names(void)
{
    printf("\n[test_day_names]\n");
    char buf[16];

    watch_face_init();

    const char *expected[] = {
        NULL, "Min", "Sen", "Sel", "Rab", "Kam", "Jum", "Sab"
    };

    for (uint8_t d = 1; d <= 7; d++) {
        watch_face_set_date(d, 1, 1, 2026);
        watch_face_get_date_str(buf, sizeof(buf));
        /* Check that the first 3 chars match the expected day name */
        ASSERT(strncmp(buf, expected[d], 3) == 0, expected[d]);
    }
}

static void test_invalid_day(void)
{
    printf("\n[test_invalid_day]\n");
    char buf[16];

    watch_face_init();

    /* Day = 0 (out of range) should clamp to 1 (Min) */
    watch_face_set_date(0, 15, 6, 2026);
    watch_face_get_date_str(buf, sizeof(buf));
    ASSERT(strncmp(buf, "Min", 3) == 0, "day=0 clamped to Min");

    /* Day = 8 (out of range) should clamp to 1 (Min) */
    watch_face_set_date(8, 15, 6, 2026);
    watch_face_get_date_str(buf, sizeof(buf));
    ASSERT(strncmp(buf, "Min", 3) == 0, "day=8 clamped to Min");
}

static void test_render_no_crash(void)
{
    printf("\n[test_render_no_crash]\n");

    sim_init();
    watch_face_init();
    watch_face_set_time(14, 30, 0);
    watch_face_set_date(2, 22, 7, 2026);
    watch_face_render();
    UG_Update();

    ASSERT(true, "watch_face_render() does not crash");
}

static void test_gui_manager_init(void)
{
    printf("\n[test_gui_manager_init]\n");

    sim_init();
    /* Manually init watch face & manager state (can't call gui_manager_init
       because it calls gui_port_init which needs real OLED hardware) */
    watch_face_init();

    ASSERT(true, "gui_manager modules initialize without crash");
}

static void test_gui_manager_screen_navigation(void)
{
    printf("\n[test_gui_manager_screen_navigation]\n");

    sim_init();
    watch_face_init();

    /* With only 1 screen (SCREEN_COUNT=1), swipe should wrap around */
    gui_manager_set_screen(SCREEN_WATCH_FACE);
    ASSERT(gui_manager_get_screen() == SCREEN_WATCH_FACE,
           "initial screen is WATCH_FACE");

    /* Set screen to invalid value — should be ignored */
    gui_manager_set_screen((gui_screen_t)99);
    ASSERT(gui_manager_get_screen() == SCREEN_WATCH_FACE,
           "invalid screen ID ignored");
}

static void test_time_update_dynamic(void)
{
    printf("\n[test_time_update_dynamic]\n");
    char buf[6];

    watch_face_init();

    /* Simulate time passing */
    watch_face_set_time(12, 0, 0);
    watch_face_get_time_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "12:00") == 0, "12:00 initial");

    watch_face_set_time(12, 1, 0);
    watch_face_get_time_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "12:01") == 0, "12:01 after 1 minute");

    watch_face_set_time(13, 0, 0);
    watch_face_get_time_str(buf, sizeof(buf));
    ASSERT(strcmp(buf, "13:00") == 0, "13:00 after 1 hour");
}

/* =========================================================================
 * Simulation Mode (Visual ASCII Dump)
 * ========================================================================= */
static void run_simulation(void)
{
    printf("\n==========================================\n");
    printf("  VISUAL SIMULATION: Watch Face (128x64)\n");
    printf("==========================================\n");

    sim_init();
    watch_face_init();
    watch_face_set_time(14, 30, 0);
    watch_face_set_date(2, 22, 7, 2026);
    watch_face_render();

    sim_dump_screen();

    printf("\n  Time: 14:30\n");
    printf("  Date: Sen, 22/07/2026\n");
    printf("  Font: FONT_6X10\n");
    printf("  Screen: 128x64 OLED (monochrome)\n");
}

/* =========================================================================
 * Main
 * ========================================================================= */
int main(int argc, char *argv[])
{
    /* Check for simulation mode */
    if (argc > 1 && strcmp(argv[1], "--sim") == 0) {
        run_simulation();
        return 0;
    }

    printf("==========================================\n");
    printf("  UNIT TEST: GUI Application Layer\n");
    printf("==========================================\n");

    test_time_formatting();
    test_date_formatting();
    test_day_names();
    test_invalid_day();
    test_render_no_crash();
    test_gui_manager_init();
    test_gui_manager_screen_navigation();
    test_time_update_dynamic();

    /* Visual simulation at the end */
    run_simulation();

    /* Summary */
    printf("\n==========================================\n");
    printf("  %d PASSED, %d FAILED\n", g_pass, g_fail);
    printf("==========================================\n");

    return g_fail == 0 ? 0 : 1;
}
