#include "../oled.h"

#define BOX_SIZE        8       // size of each square
#define BOARD_SIZE      8       // size of chessboard

void Test_Papancatur(void)
{
    for (uint8_t row = 0; row < BOARD_SIZE; row++)
    {
        for (uint8_t col = 0; col < BOARD_SIZE; col++)
        {
            if((row + col) % 2 == 0)   
            {
                OLED_FillRect(col * BOX_SIZE,
                              row * BOX_SIZE,
                              BOX_SIZE,
                              BOX_SIZE,
                              true);
            }
        }
    }
}
