#include "gui_manager.h"
#include "screens/watch_face.h"

/* Include friend's porting layer (read-only, no modifications) */
#include "../../sensors/OLED/GUI/gui_port.h"

/* =========================================================================
 * Internal state — minimal RAM: just 1 byte for active screen
 * ========================================================================= */
static gui_screen_t s_active_screen;

/* =========================================================================
 * Public API
 * ========================================================================= */

void gui_manager_init(void)
{
    /* Initialize the uGUI + Gesture Engine via friend's porting layer */
    gui_port_init();

    /* Initialize all screen modules */
    watch_face_init();

    /* Set default active screen */
    s_active_screen = SCREEN_WATCH_FACE;
}

void gui_manager_process_touch(bool pressed, int16_t x, int16_t y)
{
    /* Forward to friend's porting layer (uGUI + Gesture Engine) */
    port_touch(pressed, x, y);
}

void gui_manager_render(void)
{
    /* 1. Poll gesture engine for navigation events */
    gesture_event_t event;
    if (gui_port_poll_gesture(&event)) {
        if (event.type == GESTURE_SWIPE_LEFT) {
            /* Navigate to next screen */
            int next = (int)s_active_screen + 1;
            if (next >= (int)SCREEN_COUNT) next = 0;
            s_active_screen = (gui_screen_t)next;
        } else if (event.type == GESTURE_SWIPE_RIGHT) {
            /* Navigate to previous screen */
            int prev = (int)s_active_screen - 1;
            if (prev < 0) prev = (int)SCREEN_COUNT - 1;
            s_active_screen = (gui_screen_t)prev;
        }
    }

    /* 2. Render the active screen */
    switch (s_active_screen) {
        case SCREEN_WATCH_FACE:
            watch_face_render();
            break;
        /* Future screens:
         * case SCREEN_HEART_RATE:
         *     heart_rate_render();
         *     break;
         */
        default:
            watch_face_render();
            break;
    }
}

gui_screen_t gui_manager_get_screen(void)
{
    return s_active_screen;
}

void gui_manager_set_screen(gui_screen_t screen)
{
    if (screen < SCREEN_COUNT) {
        s_active_screen = screen;
    }
}
