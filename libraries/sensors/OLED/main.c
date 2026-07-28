#include "oled.h"
#include "GUI/gui_port.h"

// Variables for GUI time
volatile uint32_t g_tick_ms = 0;
void Test_Pixel(void);

// Dummy micros() to fix linker error in pulp-runtime's i2c.c
uint32_t micros(void) {
    return 0;
}

// Dummy pe_start to fix linker error for cluster
void pe_start(void *arg) {}


void Test_Line(void);
void Test_Kotak(void);
void Test_Tangga(void);
void Test_Papancatur(void);
void Test_Text(void);
void Test_Circle(void);

int main(void)
{
    // 1. Initialize Hardware OLED
    OLED_Init();
    OLED_Clear();

    // 2. Initialize GUI System
    gui_port_init();

    // 3. Draw Initial Screen
    UG_FillScreen(C_BLACK);
    UG_FontSelect(&FONT_6X10);
    UG_PutString(10, 10, "Halo RISC-V!");
    UG_DrawLine(0, 25, 127, 25, C_WHITE);
    
    // Push the drawn frame to the OLED display
    OLED_Update();

    while(1)
    {
        /* 
         * TODO: Insert your I2C touch sensor reading function here.
         * Example:
         * 
         * int16_t touch_x, touch_y;
         * bool is_pressed = CST816S_ReadTouch(&touch_x, &touch_y);
         * 
         * // Forward touch data to the GUI
         * port_touch(is_pressed, touch_x, touch_y);
         */
        
        g_tick_ms++; // Increment time for gesture engine
    }

    return 0;
}