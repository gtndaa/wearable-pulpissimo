#ifndef WATCH_FACE_H
#define WATCH_FACE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the watch face screen state.
 */
void watch_face_init(void);

/**
 * @brief Set the current time to display.
 *
 * @param hours   0-23
 * @param minutes 0-59
 * @param seconds 0-59
 */
void watch_face_set_time(uint8_t hours, uint8_t minutes, uint8_t seconds);

/**
 * @brief Set the current date to display.
 *
 * @param day   Day of week 1-7 (1=Minggu, 2=Senin, ..., 7=Sabtu)
 * @param date  Day of month 1-31
 * @param month Month 1-12
 * @param year  Full year (e.g. 2026)
 */
void watch_face_set_date(uint8_t day, uint8_t date, uint8_t month, uint16_t year);

/**
 * @brief Render the watch face to the uGUI framebuffer.
 *
 * Draws the time (HH:MM) centered on screen and
 * the date (Sen, DD/MM/YYYY) below it using FONT_6X10.
 * Does NOT call OLED_Update(); the caller is responsible for flushing.
 */
void watch_face_render(void);

/**
 * @brief Get the formatted time string (for testing/debug).
 *
 * @param buf   Output buffer (must be at least 6 bytes: "HH:MM\0")
 * @param size  Size of buffer
 */
void watch_face_get_time_str(char *buf, uint8_t size);

/**
 * @brief Get the formatted date string (for testing/debug).
 *
 * @param buf   Output buffer (must be at least 16 bytes: "Sen, DD/MM/YYYY\0")
 * @param size  Size of buffer
 */
void watch_face_get_date_str(char *buf, uint8_t size);

#endif /* WATCH_FACE_H */
