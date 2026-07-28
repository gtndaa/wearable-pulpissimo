#include "../oled.h"

void Test_Text(void)
{
    // Clear screen first
    OLED_Clear();

    // 1. Basic text test
    OLED_DrawString(0, 0, "Hello hehe", true);
    
    // 2. Lowercase and symbols test
    OLED_DrawString(0, 16, "yeayy udah bisa", true);
    
    // 3. Numbers and brackets
    //OLED_DrawString(0, 32, "(2009/02/14)", true);
    
    // 4. Text wrap test (will auto-wrap if it exceeds OLED_WIDTH)
    OLED_DrawString(0, 32, "selebrasi dulu wle, full senyum hihi", true);
}
