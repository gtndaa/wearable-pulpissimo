#ifndef GUI_PORT_H
#define GUI_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "ugui/ugui.h"
#include "gesture.h"

// OLED display dimensions (128x64)
#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64

/**
 * @brief Initializes uGUI and the Gesture Engine (Main Porting System)
 */
void gui_port_init(void);

/**
 * @brief Feeds raw data from the touch screen sensor into the system.
 * 
 * This function forwards data to uGUI (for button handling) and 
 * to the Gesture Engine (for detecting swipe/double-click).
 * 
 * @param pressed true if pressed, false if released
 * @param x X Coordinate
 * @param y Y Coordinate
 */
void port_touch(bool pressed, int16_t x, int16_t y);

/**
 * @brief Checks if a new gesture has been successfully translated
 * 
 * @param event Pointer to the gesture event structure to be filled
 * @return true if there is a gesture, false if there is none
 */
bool gui_port_poll_gesture(gesture_event_t *event);

#endif // GUI_PORT_H
