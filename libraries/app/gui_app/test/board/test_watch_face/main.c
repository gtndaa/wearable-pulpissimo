/**
 * @file    main.c
 * @brief   Board Test: Watch Face on OLED with DS1307 RTC (PULPissimo RISC-V)
 *
 * Template diambil dari Demo_ICDEC_Medical.
 * Hanya menampilkan JAM (HH:MM:SS) dan TANGGAL (DD/MM/YYYY) di layar OLED.
 * Font dan layout mengikuti gaya Demo Medical.
 */

#include "gui_port.h"
#include "oled.h"
#include "rtc.h"
#include <stdbool.h>

static i2c_t *rtc_i2c;
volatile uint32_t g_tick_ms = 0;

static void format_2digit(char *buf, int val) {
  buf[0] = '0' + (val / 10);
  buf[1] = '0' + (val % 10);
}

int main(void) {
  // 1. Initialize DS1307 RTC
  i2c_dev_t rtc_conf;
  int rtc_status = rtc_init(&rtc_conf, &rtc_i2c);

  int h = 0, m = 0, s = 0;
  int date = 1, month = 1, year = 26;

  if (rtc_status == RTC_OK && rtc_i2c != NULL) {
    // Switch I2C ke RTC
    i2c_dev_t ds_conf;
    i2c_dev_init(&ds_conf);
    ds_conf.id = 0;
    ds_conf.cs = 0x68 << 1;
    ds_conf.max_baudrate = 100000;
    i2c_open(&ds_conf);

    rtc_time_t t;
    if (rtc_get_time(rtc_i2c, &t) == RTC_OK) {
      h = t.hours;
      m = t.minutes;
      s = t.seconds;
      date = t.date;
      month = t.month;
      year = t.year;
    }
  }

  // 2. Initialize OLED and GUI
  i2c_dev_t oled_conf;
  i2c_dev_init(&oled_conf);
  oled_conf.id = 0;
  oled_conf.cs = 0x3C << 1;
  oled_conf.max_baudrate = 400000;
  i2c_open(&oled_conf);

  OLED_Init();
  gui_port_init();
  OLED_Clear();

  // Initial UI Draw
  UG_FontSelect(&FONT_8X8);
  UG_PutString(20, 25, "Loading...");
  OLED_Update();

  // Variables for the loop
  uint32_t last_ui_update = pos_tick_get_counter_ms();

  while (1) {
    uint32_t now = pos_tick_get_counter_ms();

    if (now - last_ui_update >= 1000) {
      last_ui_update += 1000;

      s++;
      if (s >= 60) {
        s = 0;
        m++;
        if (m >= 60) {
          m = 0;
          h++;
          if (h >= 24) {
            h = 0;
            date++;
          }
        }
      }

      char time_str[] = "00:00:00";
      format_2digit(&time_str[0], h);
      format_2digit(&time_str[3], m);
      format_2digit(&time_str[6], s);
      
      char date_str[] = "00/00/2000";
      format_2digit(&date_str[0], date);
      format_2digit(&date_str[3], month);
      format_2digit(&date_str[8], year);

      i2c_open(&oled_conf);

      OLED_Clear();

      UG_FontSelect(&FONT_8X8);
      UG_PutString(20, 10, "Watch Face");

      UG_FontSelect(&FONT_12X16);
      UG_PutString(10, 25, time_str);

      UG_FontSelect(&FONT_6X8);
      UG_PutString(28, 45, date_str);

      OLED_Update();
    }

    pos_delay_busy_ms(5);
  }

  return 0;
}

void pe_start(void) {}
uint32_t micros(void) { return 0; }
