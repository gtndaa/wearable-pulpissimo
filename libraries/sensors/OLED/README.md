# Pulpissimo OLED Graphics Driver

A lightweight, bare-metal C driver for I2C-based OLED displays (SSD1306 / SH1106), built for the Pulpissimo RISC-V platform. It provides display initialization, basic control, and a collection of graphics drawing functions — all communicating over the UDMA I2C peripheral.

## Hardware Requirements
- **Platform**: Pulpissimo (RISC-V)
- **Display**: SSD1306 / SH1106 OLED (128×64 resolution)
- **Interface**: I2C (default address `0x3C`)

## Getting Started

1. Source the `pulp-runtime` SDK into your terminal session:
   ```bash
   source /path/to/pulp-runtime/configs/pulpissimo.sh
   ```
2. Build and run:
   ```bash
   make clean all run
   ```

> **Tip:** To run on the simulator instead of real hardware, append `platform=gvsoc` to the command.

## API Reference

### Initialization

| Function | Description |
|----------|-------------|
| `void OLED_Init(void)` | Sets up the I2C peripheral and configures the OLED with default settings (contrast, memory mode, charge pump, etc.). |
| `void OLED_Clear(void)` | Zeroes out the internal framebuffer. Note that this won't affect the physical screen until you call `OLED_Update()`. |
| `void OLED_Update(void)` | Flushes the entire framebuffer to the OLED screen over I2C. Call this once you're done drawing. |

### Display Control

| Function | Description |
|----------|-------------|
| `void OLED_DisplayOn(void)` | Turns the display panel on. |
| `void OLED_DisplayOff(void)` | Turns the display panel off (enters sleep / low-power mode). |

### Drawing Primitives

| Function | Description |
|----------|-------------|
| `void OLED_DrawPixel(uint8_t x, uint8_t y, bool color)` | Sets a single pixel at `(x, y)`. |
| `void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool color)` | Draws a line from `(x0, y0)` to `(x1, y1)` using Bresenham's algorithm. |
| `void OLED_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool color)` | Draws a rectangle outline at `(x, y)` with the given width and height. |
| `void OLED_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool color)` | Draws a filled (solid) rectangle at `(x, y)` with the given width and height. |

> **Color parameter:** `true` turns the pixel on (white), `false` turns it off (black).

## Example

```c
#include "oled.h"

int main(void) {
    // Initialize the display
    OLED_Init();

    // Start with a clean screen
    OLED_Clear();

    // Draw a diagonal line and a filled box
    OLED_DrawLine(0, 0, 127, 63, true);
    OLED_FillRect(10, 10, 20, 20, true);

    // Push the framebuffer to the screen
    OLED_Update();

    return 0;
}
```
