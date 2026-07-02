/**
 * @file max30102.c
 * @brief MAX30102 Driver Implementation
 *
 * Implements the MAX30102 library API for PULP/Nusacore FPGA boards.
 */

#include "max30102.h"
#include "max30102_regs.h"
#include <stdio.h>

// PULP Runtime includes
#ifdef SOFTWARE_TEST
    // Mock I2C structures for host PC testing
    typedef struct {
        int id;
        char cs;
        unsigned int max_baudrate;
    } i2c_dev_t;
    typedef void i2c_t;

    extern i2c_t *i2c_open(i2c_dev_t *dev);
    extern int i2c_write(i2c_t *dev, unsigned char *data, int length, int send_stop);
    extern int i2c_read(i2c_t *dev_i2c, unsigned char *rx_buff, int length, int pending);
    extern void i2c_dev_init(i2c_dev_t *dev);
#else
    #include "pulp.h"
#endif

// -------------------------------------------------------------------------
// INTERNAL I2C HELPER FUNCTIONS
// -------------------------------------------------------------------------

static max30102_status_t i2c_write_reg(max30102_t *dev, uint8_t reg, uint8_t value) {
    if (!dev || !dev->i2c_dev) return MAX30102_ERROR_INVALID_PARAM;
    
    // In PULP runtime, to write a register, we send the register address followed by the data
    uint8_t tx_data[2] = {reg, value};
    
    // Write 2 bytes: register address and value. send_stop = 1 (send stop bit)
    int ret = i2c_write((i2c_t *)dev->i2c_dev, tx_data, 2, 1);
    
    if (ret != 0) {
        printf("I2C WRITE ERROR (%d)\n", ret);
        return MAX30102_ERROR_I2C;
    }
    
    printf("[WRITE] Reg 0x%02X <- 0x%02X\n", reg, value);
    
    return MAX30102_OK;
}

static max30102_status_t i2c_read_reg(max30102_t *dev, uint8_t reg, uint8_t *value) {
    if (!dev || !dev->i2c_dev || !value) return MAX30102_ERROR_INVALID_PARAM;

    // Write register address without stop (send_stop = 0)
    int ret = i2c_write((i2c_t *)dev->i2c_dev, &reg, 1, 0);
    if (ret != 0) {
        printf("I2C WRITE ERROR (%d)\n", ret);
        return MAX30102_ERROR_I2C;
    }

    // Read 1 byte and send stop (pending = 0)
    ret = i2c_read((i2c_t *)dev->i2c_dev, value, 1, 0);
    if (ret <= 0) {
        printf("I2C READ ERROR (%d)\n", ret);
        return MAX30102_ERROR_I2C;
    }

    printf("[READ] Reg 0x%02X\n", reg);
    printf("[READ] Value = 0x%02X\n", *value);

    return MAX30102_OK;
}

static max30102_status_t i2c_read_regs(max30102_t *dev, uint8_t reg, uint8_t *data, int len) {
    if (!dev || !dev->i2c_dev || !data || len <= 0) return MAX30102_ERROR_INVALID_PARAM;

    // Write register address without stop (send_stop = 0)
    int ret = i2c_write((i2c_t *)dev->i2c_dev, &reg, 1, 0);
    if (ret != 0) {
        printf("I2C WRITE ERROR (%d)\n", ret);
        return MAX30102_ERROR_I2C;
    }

    // Read len bytes and send stop (pending = 0)
    ret = i2c_read((i2c_t *)dev->i2c_dev, data, len, 0);
    if (ret <= 0) {
        printf("I2C READ ERROR (%d)\n", ret);
        return MAX30102_ERROR_I2C;
    }

    return MAX30102_OK;
}

// -------------------------------------------------------------------------
// PUBLIC API IMPLEMENTATION
// -------------------------------------------------------------------------

max30102_status_t max30102_init(max30102_t *dev, int i2c_port) {
    if (!dev) return MAX30102_ERROR_INVALID_PARAM;
    
    dev->i2c_port = i2c_port;
    dev->i2c_addr = MAX30102_I2C_ADDR;
    
    i2c_dev_t conf;
    i2c_dev_init(&conf);
    conf.id = i2c_port;
    conf.cs = dev->i2c_addr << 1;
    conf.max_baudrate = 100000;
    
    printf("[INIT] Opening I2C...\n");
    printf("I2C Port : %d\n", conf.id);
    printf("Address  : 0x%02X\n", conf.cs);
    printf("Baudrate : %d\n", conf.max_baudrate);
    
    dev->i2c_dev = (void *)i2c_open(&conf);
    if (!dev->i2c_dev) {
        printf("ERROR: Failed to open I2C port %d\n", i2c_port);
        return MAX30102_ERROR_I2C;
    }
    printf("OK\n\n");
    
    // Check Part ID to verify connection
    printf("[INIT] Reading Part ID...\n");
    uint8_t part_id = 0;
    if (max30102_get_part_id(dev, &part_id) != MAX30102_OK || part_id != MAX30102_EXPECTED_PART_ID) {
        printf("FAIL\n");
        printf("ERROR: MAX30102 not found! Expected Part ID: 0x%02X, Got: 0x%02X\n", MAX30102_EXPECTED_PART_ID, part_id);
        return MAX30102_ERROR_NOT_FOUND;
    }
    printf("OK\n\n");
    
    // Soft Reset
    printf("[INIT] Reset...\n");
    max30102_reset(dev);
    printf("OK\n\n");
    
    printf("[INIT] Config FIFO...\n");
    // Set configuration: SpO2 Mode
    i2c_write_reg(dev, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    
    // Set SPO2 Config: 4096 ADC Range, 400 Sample Rate, 18-bit Pulse Width
    i2c_write_reg(dev, MAX30102_REG_SPO2_CONFIG, MAX30102_SPO2_ADC_4096 | MAX30102_SPO2_SR_400 | MAX30102_SPO2_PW_411);
    
    // Set FIFO Config: Average 4 samples, Rollover enabled
    i2c_write_reg(dev, MAX30102_REG_FIFO_CONFIG, MAX30102_SMP_AVE_4 | MAX30102_FIFO_ROLLOVER_EN);
    
    // Default Calibration: Red=0x24 (~7mA), IR=0x24 (~7mA)
    max30102_set_led_amplitude(dev, 0x24, 0x24);
    
    // Enable A_FULL (FIFO Almost Full) and PPG_RDY Interrupts
    i2c_write_reg(dev, MAX30102_REG_INT_ENABLE_1, MAX30102_INT_A_FULL_EN | MAX30102_INT_PPG_RDY_EN);
    i2c_write_reg(dev, MAX30102_REG_INT_ENABLE_2, 0x00);
    
    max30102_clear_fifo(dev);
    printf("OK\n\n");
    
    return MAX30102_OK;
}

max30102_status_t max30102_reset(max30102_t *dev) {
    max30102_status_t status = i2c_write_reg(dev, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET);
    if (status != MAX30102_OK) return status;
    
    // Wait for reset bit to clear
    uint8_t mode = MAX30102_MODE_RESET;
    while (mode & MAX30102_MODE_RESET) {
        status = i2c_read_reg(dev, MAX30102_REG_MODE_CONFIG, &mode);
        if (status != MAX30102_OK) return status;
        
        // Add a small delay if possible, or busy wait
        for(volatile int i=0; i<10000; i++);
    }
    
    return MAX30102_OK;
}

max30102_status_t max30102_get_part_id(max30102_t *dev, uint8_t *part_id) {
    return i2c_read_reg(dev, MAX30102_REG_PART_ID, part_id);
}

max30102_status_t max30102_set_led_amplitude(max30102_t *dev, uint8_t red_pa, uint8_t ir_pa) {
    i2c_write_reg(dev, MAX30102_REG_LED1_PA, red_pa);
    i2c_write_reg(dev, MAX30102_REG_LED2_PA, ir_pa);
    return MAX30102_OK;
}

max30102_status_t max30102_clear_fifo(max30102_t *dev) {
    i2c_write_reg(dev, MAX30102_REG_FIFO_WR_PTR, 0x00);
    i2c_write_reg(dev, MAX30102_REG_OVF_COUNTER, 0x00);
    i2c_write_reg(dev, MAX30102_REG_FIFO_RD_PTR, 0x00);
    return MAX30102_OK;
}

max30102_status_t max30102_read_fifo(max30102_t *dev, uint32_t *red_data, uint32_t *ir_data) {
    if (!dev || !red_data || !ir_data) return MAX30102_ERROR_INVALID_PARAM;
    
    uint8_t wr_ptr, rd_ptr;
    i2c_read_reg(dev, MAX30102_REG_FIFO_WR_PTR, &wr_ptr);
    i2c_read_reg(dev, MAX30102_REG_FIFO_RD_PTR, &rd_ptr);
    
    // Check if new data is available
    if (wr_ptr == rd_ptr)
    {
        return MAX30102_ERROR_NO_DATA;
    }
    
    // Read 6 bytes of data (3 bytes Red, 3 bytes IR)
    uint8_t buffer[6];
    i2c_read_regs(dev, MAX30102_REG_FIFO_DATA, buffer, 6);
    
    // Combine 3 bytes into 18-bit data for RED
    *red_data = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];
    *red_data &= 0x03FFFF; // Mask to 18-bit
    
    // Combine 3 bytes into 18-bit data for IR
    *ir_data = ((uint32_t)buffer[3] << 16) | ((uint32_t)buffer[4] << 8) | buffer[5];
    *ir_data &= 0x03FFFF; // Mask to 18-bit
    
    return MAX30102_OK;
}
