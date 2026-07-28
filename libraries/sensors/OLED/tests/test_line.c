#include "../oled.h"

void Test_Line(void)
{
   // OLED_DrawLine(10, 10, 10, 60, true); // vertical line
   // OLED_DrawLine(10, 10, 60, 10, true); // horizontal line
    OLED_DrawLine(25, 25, 75, 60, true); // diagonal line
    OLED_DrawLine(25, 60, 75, 25, true); // diagonal line
}
