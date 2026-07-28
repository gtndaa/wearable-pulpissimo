#include "watch_face.h"

/* Include uGUI framework from friend's library (read-only, no modifications) */
#include "../../../sensors/OLED/GUI/ugui/ugui.h"
#include "../../../sensors/OLED/GUI/ugui/ui/ugui_fonts.h"

#include <stdio.h>
#include <string.h>

/* =========================================================================
 * Display constants (OLED 128x64, FONT_6X10)
 * ========================================================================= */
#define SCREEN_W  128
#define SCREEN_H   64
#define FONT_W      6   /* FONT_6X10 character width  */
#define FONT_H     10   /* FONT_6X10 character height */

/* =========================================================================
 * Internal state — very small footprint (~20 bytes static RAM)
 * ========================================================================= */
static uint8_t  s_hours;
static uint8_t  s_minutes;
static uint8_t  s_seconds;

static uint8_t  s_day;      /* Day of week 1-7 */
static uint8_t  s_date;     /* Day of month 1-31 */
static uint8_t  s_month;    /* Month 1-12 */
static uint16_t s_year;     /* Full year */

/* Day name lookup (Indonesian, 3-char abbreviation) */
static const char * const s_day_names[8] = {
    "???",   /* index 0 — unused */
    "Min",   /* 1 = Minggu (Sunday) */
    "Sen",   /* 2 = Senin  (Monday) */
    "Sel",   /* 3 = Selasa (Tuesday) */
    "Rab",   /* 4 = Rabu   (Wednesday) */
    "Kam",   /* 5 = Kamis  (Thursday) */
    "Jum",   /* 6 = Jumat  (Friday) */
    "Sab"    /* 7 = Sabtu  (Saturday) */
};

/* =========================================================================
 * Public API
 * ========================================================================= */

void watch_face_init(void)
{
    s_hours   = 0;
    s_minutes = 0;
    s_seconds = 0;
    s_day     = 2;   /* Default: Senin */
    s_date    = 1;
    s_month   = 1;
    s_year    = 2026;
}

void watch_face_set_time(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    s_hours   = hours;
    s_minutes = minutes;
    s_seconds = seconds;
}

void watch_face_set_date(uint8_t day, uint8_t date, uint8_t month, uint16_t year)
{
    s_day   = (day >= 1 && day <= 7) ? day : 1;
    s_date  = date;
    s_month = month;
    s_year  = year;
}

void watch_face_get_time_str(char *buf, uint8_t size)
{
    if (!buf || size < 6) return;
    snprintf(buf, size, "%02u:%02u", s_hours, s_minutes);
}

void watch_face_get_date_str(char *buf, uint8_t size)
{
    if (!buf || size < 16) return;
    const char *day_str = s_day_names[s_day];
    snprintf(buf, size, "%s, %02u/%02u/%04u", day_str, s_date, s_month, s_year);
}

void watch_face_render(void)
{
    /* Format strings */
    char time_str[6];   /* "HH:MM\0" */
    char date_str[16];  /* "Sen, DD/MM/YYYY\0" */

    watch_face_get_time_str(time_str, sizeof(time_str));
    watch_face_get_date_str(date_str, sizeof(date_str));

    /* Calculate centered X positions */
    /* Time "HH:MM" = 5 chars → 5 * 6 = 30 px wide */
    int16_t time_x = (SCREEN_W - (5 * FONT_W)) / 2;
    int16_t time_y = (SCREEN_H / 2) - FONT_H - 2;  /* Slightly above center */

    /* Date "Sen, DD/MM/YYYY" = 15 chars → 15 * 6 = 90 px wide */
    uint8_t date_len = (uint8_t)strlen(date_str);
    int16_t date_x = (SCREEN_W - (date_len * FONT_W)) / 2;
    int16_t date_y = (SCREEN_H / 2) + 4;  /* Below center */

    /* Clear screen and draw */
    UG_FillScreen(C_BLACK);
    UG_FontSelect(FONT_6X10);
    UG_SetForecolor(C_WHITE);
    UG_SetBackcolor(C_BLACK);

    UG_PutString(time_x, time_y, time_str);
    UG_PutString(date_x, date_y, date_str);
}
