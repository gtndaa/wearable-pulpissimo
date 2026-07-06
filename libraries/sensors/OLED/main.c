#include "oled.h"

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

int main(void)
{
    OLED_Init();
    OLED_Clear();

    Test_Tangga();      /// change if u want test the other function

    OLED_Update();

    while(1)
    {
    }

    return 0;
}