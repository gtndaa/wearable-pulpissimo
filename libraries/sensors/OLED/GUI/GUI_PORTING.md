# uGUI Porting & Implementation Guide

This document explains how to use and integrate the **uGUI** framework and our **Gesture Engine** into your RISC-V bare-metal hardware project (such as Pulpissimo). 

Because uGUI is extremely lightweight and does not rely on heavy framebuffers, it is perfectly suited for monochrome OLED displays (128x64).

---

## 1. System Architecture

To keep the memory footprint small, the GUI system is divided into three distinct modules:

1. **uGUI Core (`ugui.c`)**: Handles the rendering of primitive graphics (lines, circles, text) and basic button widgets.
2. **Gesture Engine (`gesture.c`)**: A custom-built state machine that detects complex touch interactions (e.g., Swipe Left, Double Click, Long Press).
3. **Porting Layer (`gui_port.c`)**: The "bridge" that connects your physical hardware (OLED and Touch Sensor) to both uGUI and the Gesture Engine simultaneously.

---

## 2. How to Use (Integration in `main.c`)

To display the GUI and read touch inputs on your hardware, you only need to interact with a few functions in your main superloop.

### A. Initialization & Drawing
Include the porting header, provide a time variable (`g_tick_ms`), and initialize the system **after** your OLED hardware is ready.

```c
#include "oled.h"
#include "../GUI/gui_port.h"

// 1. Required global variable for system time (milliseconds)
volatile uint32_t g_tick_ms = 0; 

int main(void) {
    // 2. Initialize hardware
    OLED_Init();
    
    // 3. Initialize GUI System
    gui_port_init(); 
    
    // 4. Draw your UI
    UG_FillScreen(C_BLACK); 
    UG_FontSelect(&FONT_6X10);
    UG_PutString(10, 10, "Hello RISC-V!");
    OLED_Update(); // Push to physical screen
    
    // ...
```

### B. Handling Touch & Gestures
Inside your `while(1)` loop, read from your I2C touch sensor and feed the coordinates to `port_touch()`. Then, poll for any completed gestures.

```c
    while(1) {
        // 1. Read actual hardware I2C sensor (example)
        int16_t touch_x, touch_y;
        bool is_pressed = CST816S_ReadTouch(&touch_x, &touch_y);
        
        // 2. Feed the raw data to the GUI & Gesture Engine
        port_touch(is_pressed, touch_x, touch_y);
        
        // 3. Check if a gesture (like a swipe) occurred
        gesture_event_t event;
        if (gui_port_poll_gesture(&event)) {
            // Do something based on the gesture!
            if (event.type == GESTURE_SWIPE_LEFT) {
                // Change screen to the next page
            } else if (event.type == GESTURE_DOUBLE_CLICK) {
                // Confirm action
            }
        }
        
        // 4. Increment the system time (crucial for gesture timing)
        g_tick_ms++; 
    }
}
```

---

## 3. Modifying the Port (For Different Hardware)

If you ever change your display model or your microcontroller, you **do not** need to rewrite the GUI. You only need to edit two functions inside `gui_port.c`:

1. **`framework_pixel()`**: Change the contents of this function to call your new display's `DrawPixel` function.
2. **`gesture_tick_ms()`**: Change this to return your new microcontroller's millisecond hardware timer (or keep using the `g_tick_ms` loop counter).

---

## 4. Linux Simulation (Logic Verification)

Even though this GUI is now designed for hardware, you can still verify your gesture logic on a Linux PC without needing the physical board.

We have provided a comprehensive unit test suite that simulates 10 different touch scenarios (e.g., drifting clicks, delayed double-clicks, accurate swipes).

To run the simulation:
```bash
cd test
make test
```
All 32 logic assertions will run independently, ensuring your UI logic is robust before flashing it to the device.
