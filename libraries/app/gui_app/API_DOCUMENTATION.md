# API Documentation: GUI Application Layer (Watch Face)

## Overview

This module provides a lightweight GUI application layer for displaying a **Watch Face (Clock & Date)** on a 128x64 monochrome OLED display. Built on top of the uGUI framework and Gesture Engine.

**Target Platform**: PULPissimo RISC-V SoC (64 KB RAM)  
**Display**: SSD1306 / SH1106 OLED 128x64 monochrome  
**RAM Footprint**: < 2 KB (including OLED framebuffer)

---

## Architecture

```text
┌─────────────────────────────────────────────────┐
│                  main.c (User App)              │
│   gui_manager_init() → process_touch() → render │
├─────────────────────────────────────────────────┤
│           gui_manager.h / gui_manager.c         │
│        Screen Navigation & Gesture Router       │
├──────────────┬──────────────────────────────────┤
│ screens/     │                                  │
│ watch_face   │  (Future: heart_rate, oxygen...) │
├──────────────┴──────────────────────────────────┤
│    Friend's Library (sensors/OLED/GUI/)          │
│    gui_port ─► uGUI Core + Gesture Engine       │
├─────────────────────────────────────────────────┤
│         OLED Driver (sensors/OLED/oled.h)       │
└─────────────────────────────────────────────────┘
```

---

## API Reference

### gui_manager.h

| Function | Description |
|---|---|
| `void gui_manager_init(void)` | Initialize uGUI, Gesture Engine, and all screens. Call after `OLED_Init()`. |
| `void gui_manager_process_touch(bool pressed, int16_t x, int16_t y)` | Feed raw touch data into the system. |
| `void gui_manager_render(void)` | Poll gestures for navigation, then render the active screen. |
| `gui_screen_t gui_manager_get_screen(void)` | Get the currently active screen ID. |
| `void gui_manager_set_screen(gui_screen_t screen)` | Manually set the active screen. |

**Screen IDs** (`gui_screen_t` enum):
- `SCREEN_WATCH_FACE` (0) — Clock & Date display

---

### screens/watch_face.h

| Function | Description |
|---|---|
| `void watch_face_init(void)` | Initialize watch face state with default time/date. |
| `void watch_face_set_time(uint8_t hours, uint8_t minutes, uint8_t seconds)` | Set time (24h format). |
| `void watch_face_set_date(uint8_t day, uint8_t date, uint8_t month, uint16_t year)` | Set date. `day`: 1=Min, 2=Sen, ..., 7=Sab. |
| `void watch_face_render(void)` | Draw time (HH:MM) and date (Sen, DD/MM/YYYY) to uGUI framebuffer. |
| `void watch_face_get_time_str(char *buf, uint8_t size)` | Get formatted time string. Buffer ≥ 6 bytes. |
| `void watch_face_get_date_str(char *buf, uint8_t size)` | Get formatted date string. Buffer ≥ 16 bytes. |

---

## Integration Example (Bare-Metal RISC-V `main.c`)

```c
#include "oled.h"
#include "gui_app/gui_manager.h"
#include "gui_app/screens/watch_face.h"
#include "DS1307/rtc.h"

int main(void) {
    // 1. Init hardware (I2C, RTC, OLED)
    i2c_t *rtc_handle;
    rtc_init(&dev, &rtc_handle);
    
    // 2. BACA RTC SEKALI SAJA (Mazhab pos_tick)
    rtc_time_t t;
    rtc_get_time(rtc_handle, &t);
    
    // Switch I2C ke OLED
    OLED_Init();

    // 3. Init GUI system
    gui_manager_init();
    watch_face_set_time(t.hours, t.minutes, t.seconds);
    watch_face_set_date(t.day, t.date, t.month, 2000 + t.year);

    // 4. Main loop
    uint32_t last_ui_update = pos_tick_get_counter_ms();
    
    while (1) {
        uint32_t now = pos_tick_get_counter_ms();
        
        // Software Timekeeping (Tiap 1000ms jam bertambah sendiri)
        if (now - last_ui_update >= 1000) {
            last_ui_update += 1000;
            
            // Logika penambahan detik/menit/jam manual di sini...
            // Kemudian update UI:
            // watch_face_set_time(h, m, s);
            
            gui_manager_render();
            OLED_Update();
        }
        
        pos_delay_busy_ms(5);
    }
}
```

---

## Building & Testing (Linux PC)

### Run Unit Tests
```bash
cd wearable-pulpissimo/libraries/app/gui_app
make test
```

### Run Visual Simulation
```bash
make sim
```

### Build & Run on Board (Hardware Test)
```bash
cd wearable-pulpissimo/libraries/app/gui_app/test/board/test_watch_face
./build.sh

# In Terminal 1:
./run_openocd.sh

# In Terminal 2:
./run_gdb.sh
```

### Clean Build Artifacts
```bash
make clean
```

---

## Adding New Screens (Future)

1. Create `screens/your_screen.h` and `screens/your_screen.c`
2. Implement `your_screen_init()` and `your_screen_render()`
3. Add screen ID to `gui_screen_t` enum in `gui_manager.h`
4. Add render call in `gui_manager_render()` switch statement
5. Add source to `Makefile`
