/**
 * rtc_types.h - Portable RTC types for laptop compilation
 * Used when TEST_DI_LAPTOP is defined (no pulp.h available).
 * On board, this file is NOT used (real rtc.h is used instead).
 */
#ifndef RTC_TYPES_H
#define RTC_TYPES_H

#include <stdint.h>

typedef struct {
    uint8_t seconds; // 0-59
    uint8_t minutes; // 0-59
    uint8_t hours;   // 0-23
    uint8_t day;     // 1-7 (day of week)
    uint8_t date;    // 1-31
    uint8_t month;   // 1-12
    uint8_t year;    // 0-99 (offset from 2000)
} rtc_time_t;

#define RTC_OK            0
#define RTC_ERR_NULL_PTR -1

#endif // RTC_TYPES_H
