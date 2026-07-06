#include "../oled.h"

void Test_Tangga(void)
{
    uint8_t x = 10;     // starting point on x axis
    uint8_t y = 56;     // starting point on y axis

    uint8_t step_width = 20;    // width of each step
    uint8_t step_height = 8;    // height of each step

    for (int i = 0; i < 5; i++)
    {
        OLED_DrawLine(x, y, x + step_width, y, true);   // draw horizontal line

        x += step_width;

        if (i < 4)
        {
            OLED_DrawLine(x, y, x, y - step_height, true);     // draw vertical line

            y -= step_height;
        }
    }
}
