#include "gui_port.h"
#include "../oled.h"

// Main uGUI structure
static UG_GUI gui;

// ---------------------------------------------------------
// 1. Display Driver — bridges uGUI pixel calls to oled.h
// ---------------------------------------------------------
static void framework_pixel(UG_S16 x, UG_S16 y, UG_COLOR c) {
    // C_WHITE means pixel ON for monochrome OLED
    OLED_DrawPixel((uint8_t)x, (uint8_t)y, (c == C_WHITE));
}

// ---------------------------------------------------------
// 2. Tick Function (Time) for Gesture Engine
// ---------------------------------------------------------
// The engine needs time in milliseconds to calculate press & swipe durations.
// On bare-metal RISC-V, we use a global counter incremented in main loop.
extern volatile uint32_t g_tick_ms;

uint32_t gesture_tick_ms(void) {
    return g_tick_ms;
}

// ---------------------------------------------------------
// 3. Porting Initialization
// ---------------------------------------------------------
static UG_DEVICE device;

void gui_port_init(void) {
    // Setup uGUI device
    device.x_dim = DISPLAY_WIDTH;
    device.y_dim = DISPLAY_HEIGHT;
    device.pset = framework_pixel;
    device.flush = NULL;
    
    // A. Initialize uGUI with 128x64 display
    UG_Init(&gui, &device);
    UG_FillScreen(C_BLACK);
    
    // B. Initialize Gesture Recognition Engine
    gesture_init();
}

// ---------------------------------------------------------
// 4. Forwarding Sensor Input to the System
// ---------------------------------------------------------
void port_touch(bool pressed, int16_t x, int16_t y) {
    // Forward to uGUI (uGUI requires constant state: TOUCH_STATE_PRESSED/RELEASED)
    // uGUI doesn't recognize swipe/double-click, it only knows pressed/released.
    uint8_t ugui_state = pressed ? TOUCH_STATE_PRESSED : TOUCH_STATE_RELEASED;
    UG_TouchUpdate(x, y, ugui_state);
    
    // Forward to Gesture Engine (to be translated into complex actions)
    gesture_feed(pressed, x, y);
}

// ---------------------------------------------------------
// 5. Polling Gesture Translation Results
// ---------------------------------------------------------
bool gui_port_poll_gesture(gesture_event_t *event) {
    return gesture_poll(event);
}
