#include "oled.h"
#include "pulp.h"
#include "ssd1306_cmd.h"

#define OLED_I2C_ADDR       0x3c 

//Private Variables
/* 128 × 64 / 8 = 1024 Bytes */
static uint8_t framebuffer[OLED_WIDTH * OLED_HEIGHT / 8];

//Private Function Prototypes
static void OLED_I2C_Init(void);
static void OLED_WriteCommand(uint8_t cmd);
static void OLED_WriteData(const uint8_t *data, uint16_t length);

//Public Functions

void OLED_Init(void)
{
    /* Initialize I2C peripheral */
    OLED_I2C_Init();

    /* Display OFF */
    OLED_WriteCommand(OLED_CMD_DISPLAY_OFF);

    //Display Configuration
    OLED_WriteCommand(OLED_CMD_SET_MULTIPLEX);
    OLED_WriteCommand(0x3F);

    OLED_WriteCommand(OLED_CMD_SET_CONTRAST);
    OLED_WriteCommand(0xCF);

    OLED_WriteCommand(OLED_CMD_MEMORY_MODE);
    OLED_WriteCommand(0x00);

    OLED_WriteCommand(OLED_CMD_SEGMENT_REMAP);

    OLED_WriteCommand(OLED_CMD_COM_SCAN_DEC);

    OLED_WriteCommand(OLED_CMD_SET_COM_PINS);
    OLED_WriteCommand(0x12);

    OLED_WriteCommand(OLED_CMD_CHARGE_PUMP);
    OLED_WriteCommand(0x14);

    OLED_WriteCommand(OLED_CMD_NORMAL_DISPLAY);

    OLED_Clear();
    OLED_Update();

    OLED_WriteCommand(OLED_CMD_DISPLAY_ON);
}

void OLED_Clear(void)
{
    for (uint16_t i = 0; i < sizeof(framebuffer); i++) {
        framebuffer[i] = 0x00;
    }
}

void OLED_Update(void)
{
    OLED_WriteCommand(OLED_CMD_COLUMN_ADDR);
    OLED_WriteCommand(0);
    OLED_WriteCommand(127);

    OLED_WriteCommand(OLED_CMD_PAGE_ADDR);
    OLED_WriteCommand(0);
    OLED_WriteCommand(7);

    OLED_WriteData(framebuffer, sizeof(framebuffer));
}

void OLED_DisplayOn(void)
{
    OLED_WriteCommand(OLED_CMD_DISPLAY_ON);
}

void OLED_DisplayOff(void)
{
    OLED_WriteCommand(OLED_CMD_DISPLAY_OFF);
}

void OLED_InvertDisplay(bool enable)
{
    if (enable)
    {
        OLED_WriteCommand(OLED_CMD_INVERSE_DISPLAY);
    }
    else
    {
        OLED_WriteCommand(OLED_CMD_NORMAL_DISPLAY);
    }
}

void OLED_DrawPixel(uint8_t x,
                    uint8_t y,
                    bool color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT)
    {
        return;
    }

    uint16_t index = x + (y / 8) * OLED_WIDTH;
    uint8_t bit = 1 << (y % 8);

    if (color)
    {
        framebuffer[index] |= bit;
    }
    else
    {
        framebuffer[index] &= ~bit;
    }
}

void OLED_DrawLine(uint8_t x0,
                   uint8_t y0,
                   uint8_t x1,
                   uint8_t y1,
                   bool color)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? (y0 - y1) : (y1 - y0); // dy is negative absolute value
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy, e2; /* error value e_xy */

    for (;;) {
        OLED_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; } /* e_xy+e_x > 0 */
        if (e2 <= dx) { err += dx; y0 += sy; } /* e_xy+e_y < 0 */
    }
}

void OLED_DrawRect(uint8_t x,
                   uint8_t y,
                   uint8_t width,
                   uint8_t height,
                   bool color)
{
    if (width == 0 || height == 0) return;
    
    OLED_DrawLine(x, y, x + width - 1, y, color); // Top
    OLED_DrawLine(x, y + height - 1, x + width - 1, y + height - 1, color); // Bottom
    OLED_DrawLine(x, y, x, y + height - 1, color); // Left
    OLED_DrawLine(x + width - 1, y, x + width - 1, y + height - 1, color); // Right
}

void OLED_FillRect(uint8_t x,
                   uint8_t y,
                   uint8_t width,
                   uint8_t height,
                   bool color)
{
    if (width == 0 || height == 0) return;

    for (uint8_t i = 0; i < height; i++) {
        OLED_DrawLine(x, y + i, x + width - 1, y + i, color);
    }
}

void OLED_DrawCircle(uint8_t x0,
                     uint8_t y0,
                     uint8_t r,
                     bool color)
{
    int x = -r, y = 0, err = 2 - 2 * r; /* II. Quadrant */ 
    do {
        OLED_DrawPixel(x0 - x, y0 + y, color); /*   I. Quadrant */
        OLED_DrawPixel(x0 - y, y0 - x, color); /*  II. Quadrant */
        OLED_DrawPixel(x0 + x, y0 - y, color); /* III. Quadrant */
        OLED_DrawPixel(x0 + y, y0 + x, color); /*  IV. Quadrant */
        r = err;
        if (r <= y) err += ++y * 2 + 1;           /* e_xy+e_y < 0 */
        if (r > x || err > y) err += ++x * 2 + 1; /* e_xy+e_x > 0 or no 2nd y-step */
    } while (x < 0);
}


//Private Variables (I2C)
static i2c_t *i2c_dev = NULL;

//Private Functions

static void OLED_I2C_Init(void)
{
    if (i2c_dev != NULL) return; // already initialized

    i2c_dev_t dev;
    i2c_dev_init(&dev);
    dev.id  = 0;                            // I2C peripheral index 0
    dev.cs  = (OLED_I2C_ADDR << 1);         // 7-bit address shifted left for R/W bit
    dev.max_baudrate = 400000;              // 400 kHz (Fast mode)

    i2c_dev = i2c_open(&dev);
}

static void OLED_WriteCommand(uint8_t cmd)
{
    if (i2c_dev == NULL) return;

    unsigned char buf[2];
    buf[0] = 0x00;  // Co=0, D/C#=0 → control byte for command
    buf[1] = cmd;

    i2c_write(i2c_dev, buf, 2, 1); // 1 = send STOP
}

static void OLED_WriteData(const uint8_t *data,
                           uint16_t length)
{
    if (i2c_dev == NULL) return;

    /*
     * I2C_CMD_BUFFER_SIZE = 48, so send data in chunks
     * to avoid exceeding UDMA buffer limit.
     * Each chunk: 1 byte control (0x40) + max 32 bytes data.
     */
    #define OLED_CHUNK_SIZE 32

    unsigned char buf[OLED_CHUNK_SIZE + 1];
    uint16_t offset = 0;

    while (offset < length)
    {
        uint16_t remaining = length - offset;
        uint16_t chunk = (remaining > OLED_CHUNK_SIZE) ? OLED_CHUNK_SIZE : remaining;

        buf[0] = 0x40; // Co=0, D/C#=1 → control byte for data

        for (uint16_t i = 0; i < chunk; i++)
        {
            buf[1 + i] = data[offset + i];
        }

        i2c_write(i2c_dev, buf, chunk + 1, 1); // 1 = send STOP

        offset += chunk;
    }
}