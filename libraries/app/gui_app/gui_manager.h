#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * Screen ID enumeration
 * ========================================================================= */
typedef enum {
    SCREEN_WATCH_FACE = 0,
    /* Future screens (not implemented yet):
     * SCREEN_HEART_RATE,
     * SCREEN_OXYGEN,
     * SCREEN_ACTIVITY,
     * SCREEN_STEP_COUNT,
     * SCREEN_NOTIFICATION,
     */
    SCREEN_COUNT     /* Always last — total number of screens */
} gui_screen_t;

/**
 * @brief Initialize the GUI manager, uGUI framework, and gesture engine.
 *        Must be called after OLED_Init().
 */
void gui_manager_init(void);

/**
 * @brief Feed raw touch data into the GUI system.
 *
 * Forwards input to uGUI (button handling) and the Gesture Engine
 * (swipe/click detection) via gui_port.
 *
 * @param pressed  true if finger on screen
 * @param x        X coordinate
 * @param y        Y coordinate
 */
void gui_manager_process_touch(bool pressed, int16_t x, int16_t y);

/**
 * @brief Process any pending gesture and render the active screen.
 *
 * Polls the gesture engine for completed gestures (SWIPE_LEFT/RIGHT)
 * to navigate between screens, then calls the active screen's render
 * function. Does NOT call OLED_Update().
 */
void gui_manager_render(void);

/**
 * @brief Get the currently active screen ID.
 */
gui_screen_t gui_manager_get_screen(void);

/**
 * @brief Manually set the active screen.
 */
void gui_manager_set_screen(gui_screen_t screen);

#endif /* GUI_MANAGER_H */
