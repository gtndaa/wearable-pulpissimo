/**
 * @file    main.c
 * @brief   Board Test: Watch Face GUI on OLED with DS1307 RTC (PULPissimo RISC-V)
 *
 * Reads real time and date from the DS1307 RTC chip over I2C and
 * renders it live on the 128x64 OLED display using uGUI & GUI Manager.
 */

#include "oled.h"
#include "GUI/gui_port.h"
#include "gui_manager.h"
#include "screens/watch_face.h"
#include "DS1307/rtc.h"

/* System tick for gesture engine timing */
volatile uint32_t g_tick_ms = 0;

/* Dummy stubs required by pulp-runtime linker */
uint32_t micros(void) { return 0; }
void pe_start(void *arg) { (void)arg; }

int main(void)
{
    /* ── 1. Initialize Hardware ─────────────────────────────── */
    OLED_Init();
    OLED_Clear();

    /* ── 2. Initialize DS1307 RTC ───────────────────────────── */
    i2c_t *i2c_handle = NULL;
    i2c_dev_t dev_conf;
    int rtc_status = rtc_init(&dev_conf, &i2c_handle);

    /* ── 3. Initialize GUI System ───────────────────────────── */
    gui_manager_init();

    /* Initial fallback time (in case RTC read fails) */
    watch_face_set_time(14, 30, 0);
    watch_face_set_date(2, 22, 7, 2026);
    gui_manager_render();
    OLED_Update();

    uint8_t last_second = 0xFF;

    /* ── 4. Main Superloop ───────────────────────────────────── */
    while (1) {
        /* Read real time from DS1307 RTC chip over I2C */
        if (rtc_status == RTC_OK && i2c_handle != NULL) {
            rtc_time_t rtc_now;
            if (rtc_get_time(i2c_handle, &rtc_now) == RTC_OK) {
                // Only update display when second changes (saves CPU / I2C bus)
                if (rtc_now.seconds != last_second) {
                    last_second = rtc_now.seconds;

                    watch_face_set_time(rtc_now.hours, rtc_now.minutes, rtc_now.seconds);
                    watch_face_set_date(rtc_now.day, rtc_now.date, rtc_now.month, 2000 + rtc_now.year);

                    gui_manager_render();
                    OLED_Update();
                }
            }
        } else {
            // Fallback rendering if RTC is not connected
            gui_manager_render();
            OLED_Update();
        }

        g_tick_ms++;
    }

    return 0;
}
