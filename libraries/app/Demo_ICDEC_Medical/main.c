#include "gui_port.h"
#include "max30102.h"
#include "oled.h"
#include "ppg_hr.h"
#include "rtc.h"
#include <stdbool.h>
#include <stdio.h>

static max30102_t sensor;
static ppg_hr_t hr_filter;
static i2c_t *rtc_i2c;

static max30102_config_t build_sparkfun_example_config(void) {
  max30102_config_t config = max30102_get_default_config();
  config.sample_rate = MAX30102_SR_100;
  config.adc_range = MAX30102_ADC_RANGE_4096;
  config.pulse_width = MAX30102_PULSE_WIDTH_18BIT;
  config.fifo_avg = MAX30102_FIFO_AVG_4;
  config.ir_led_current = 0x1F;
  config.red_led_current = 0x0A;
  return config;
}

volatile uint32_t g_tick_ms = 0;

int main(void) {
  printf("==========================================\n");
  printf("   ICDEC Medical Watch Face Integration   \n");
  printf("==========================================\n");

  // 1. Initialize MAX30102 FIRST
  printf("Initializing MAX30102...\n");
  sensor.i2c_dev = NULL;
  max30102_status_t init_status = max30102_init(&sensor, 0);
  if (init_status != MAX30102_OK) {
    printf("\n[ERROR] Sensor MAX30102 NOT DETECTED (Status: %d)!\n",
           init_status);
    printf("Please check your wiring, plug it in securely, and reset the "
           "board.\n");
    while (1) {
      pos_delay_busy_ms(1000);
    } // Halt here
  }

  max30102_config_t config = build_sparkfun_example_config();
  printf("Configuring MAX30102...\n");
  max30102_configure(&sensor, &config);

  printf("Initializing PPG HR Filter...\n");
  ppg_hr_init(&hr_filter);

  // 2. Initialize OLED and GUI SECOND
  printf("Initializing OLED...\n");
  OLED_Init();     // Initialize hardware I2C & Screen
  gui_port_init(); // Initialize uGUI
  OLED_Clear();

  // Initial UI Draw
  UG_FontSelect(&FONT_8X8);
  UG_PutString(10, 10, "ICDEC Medical");
  OLED_Update();

  // Variables for the loop
  int print_counter = 0;
  int i = 0;

  ppg_hr_result_t hr_result = {0}; // Store last result
  uint32_t last_raw_ir = 0;
  int samples_read = 0;
  max30102_status_t last_err_status = MAX30102_OK;

  uint32_t last_ui_update = pos_tick_get_counter_ms();
  bool finger_present = true;
  uint32_t no_finger_start_time = 0;

  while (1) {
    // SWITCH I2C TO MAX30102 (100 kHz, Addr 0x57)
    if (sensor.i2c_dev == NULL)
    {
        /* reopen device */
        i2c_dev_t    max_conf;
        i2c_dev_init(&max_conf);
        max_conf.id = 0;
        max_conf.cs = MAX30102_I2C_ADDR << 1;
        max_conf.max_baudrate = 100000;
        sensor.i2c_dev = i2c_open(&max_conf);
    }

    // Drain the FIFO: read all available samples
    while (1) {
        max30102_sample_t sample = {0};
        max30102_status_t status = max30102_read_sample(&sensor, &sample);
        
        if (status == MAX30102_OK) {
            last_raw_ir = sample.ir;
            samples_read++;
            
            if (sample.ir < 50000) {
                if (finger_present) {
                    finger_present = false;
                    no_finger_start_time = pos_tick_get_counter_ms();
                } else if (pos_tick_get_counter_ms() - no_finger_start_time >= 3000) {
                    ppg_hr_init(&hr_filter);
                    hr_result.avg_bpm = 0;
                    hr_result.bpm = 0;
                    no_finger_start_time = pos_tick_get_counter_ms(); // prevent spamming init
                }
            } else {
                finger_present = true;
            }

            // SELALU jalankan filter agar tidak ada lompatan waktu (time gap)
            // yang merusak algoritma saat jari sedikit bergeser.
            hr_result = ppg_hr_process(&hr_filter, (int32_t)sample.ir, pos_tick_get_counter_ms());
        } else {
            if (status != MAX30102_ERROR_NO_DATA) {
                last_err_status = status;
            }

            i2c_close((i2c_t *) sensor.i2c_dev);
            sensor.i2c_dev = NULL;

            break; // No more data in FIFO
        }
    }

    uint32_t now = pos_tick_get_counter_ms();
    if (now - last_ui_update >= 1000) {
      rtc_time_t  _rtc_time;

      int retval;

      i2c_dev_t    rtc_conf;
      i2c_t     * _i2c;
      i2c_dev_init(&rtc_conf);
      rtc_conf.id = 0;
      rtc_conf.cs = 0x68 << 1;
      rtc_conf.max_baudrate = 100000;
      _i2c = i2c_open(&rtc_conf);

      last_ui_update = now;
      print_counter++;

      // Generate fake time
      int s = print_counter % 60;
      int m = (33 + (print_counter / 60)) % 60;
      int h = 13 + ((33 + (print_counter / 60)) / 60);
      if (h >= 24) h = h % 24;

      char date_str[20] = "24/07/2026";
      char time_str[20];

      retval = rtc_get_time(_i2c, &_rtc_time);
      if (retval == RTC_OK)
      {
        // overide with real time
        s = _rtc_time.seconds;
        m = _rtc_time.minutes;
        h = _rtc_time.hours;
      }
      else
      {
        /* fake date */
        _rtc_time.date = 24;
        _rtc_time.month = 7;
        _rtc_time.year = 26;
      }

      snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", h, m, s);
      snprintf(date_str, sizeof(date_str), "%02u/%02u/20%02u", _rtc_time.date, _rtc_time.month, _rtc_time.year);
      
      // Update BPM text every 5 seconds
      static char bpm_str[30] = "HR: --- bpm"; // Hold previous value
      if (print_counter % 5 == 0) {
        if (hr_result.avg_bpm > 0) {
          snprintf(bpm_str, sizeof(bpm_str), "HR: %d bpm", (int)hr_result.avg_bpm);
        } else {
          snprintf(bpm_str, sizeof(bpm_str), "HR: --- bpm");
        }
      }

      // Update OLED layout
      OLED_Clear();

      UG_FontSelect(&FONT_8X8); // Jam label
      UG_PutString(10, 10, "ICDEC Medical");

      UG_FontSelect(&FONT_6X8);
      UG_PutString(28, 45, date_str);

      UG_FontSelect(&FONT_12X16); // Jam
      UG_PutString(10, 25, time_str);

      UG_FontSelect(&FONT_8X8); // BPM
      UG_PutString(15, 55, bpm_str);

      // SWITCH I2C TO OLED (400 kHz, Addr 0x3C)
      i2c_dev_t oled_conf;
      i2c_dev_init(&oled_conf);
      oled_conf.id = 0;
      oled_conf.cs = 0x3C << 1;
      oled_conf.max_baudrate = 400000;
      i2c_open(&oled_conf);

      OLED_Update();

      pos_delay_busy_ms(5); // Small delay to prevent I2C bus spam

      i2c_close(_i2c);
    }
    else
    {
      pos_delay_busy_ms(5); // Small delay to prevent I2C bus spam
    }
  }

  return 0;
}

void pe_start(void) {}

// Dummy micros() to satisfy linker error in I2C driver
uint32_t micros(void) { return 0; }
