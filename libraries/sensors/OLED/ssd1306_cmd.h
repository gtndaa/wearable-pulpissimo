/**
 * @file ssd1306_cmd.h
 * @brief SSD1306 command definitions but also can use for SH1106
 *
 *
 * Reference: SSD1306 Datasheet
 */

 #ifndef SSD1306_CMD_H
 #define SSD1306_CMD_H

 #define OLED_CMD_DISPLAY_OFF       0xAE
 #define OLED_CMD_DISPLAY_ON        0xAF

 #define OLED_CMD_SET_CONTRAST      0x81        //followed by 1-byte contrast value
 
 #define OLED_CMD_NORMAL_DISPLAY    0xA6
 #define OLED_CMD_INVERSE_DISPLAY   0xA7

 #define OLED_CMD_MEMORY_MODE       0x20

 #define OLED_CMD_COLUMN_ADDR       0x21
 #define OLED_CMD_PAGE_ADDR         0x22

 #define OLED_CMD_SET_MULTIPLEX     0xA8
 #define OLED_CMD_SEGMENT_REMAP     0xA1
 #define OLED_CMD_COM_SCAN_DEC      0xC8
 #define OLED_CMD_SET_COM_PINS      0xDA
 #define OLED_CMD_CHARGE_PUMP       0x8D

#endif /* SSD1306_CMD_H */