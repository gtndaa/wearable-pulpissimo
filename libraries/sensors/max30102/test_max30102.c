/**
 * @file test_max30102.c
 * @brief Unit Tests for MAX30102 Pulse Oximeter & Heart Rate Sensor Library
 */

#ifdef SOFTWARE_TEST
    #include <stdint.h>
    #include <string.h>
    // Mock I2C structures for host PC testing
    typedef struct {
        int id;
        char cs;
        unsigned int max_baudrate;
    } i2c_dev_t;
    typedef void i2c_t;

    // A simple state machine to mock MAX30102 registers
    static uint8_t mock_regs[256] = {0};
    static uint8_t last_reg_addr = 0;

    void i2c_dev_init(i2c_dev_t *dev) {
        memset(dev, 0, sizeof(i2c_dev_t));
    }
    
    i2c_t* i2c_open(i2c_dev_t *dev) {
        return (i2c_t*)1; // Return dummy valid pointer
    }
    
    int i2c_write(i2c_t *dev, unsigned char *data, int length, int send_stop) {
        if (length == 2) {
            // Write register
            mock_regs[data[0]] = data[1];
        } else if (length == 1) {
            // Set address pointer
            last_reg_addr = data[0];
        }
        return 0;
    }

    int i2c_read(i2c_t *dev_i2c, unsigned char *rx_buff, int length, int pending) {
        if (last_reg_addr == 0xFF) { // Part ID
            rx_buff[0] = 0x15;
        } else if (last_reg_addr == 0x09) { // MODE_CONFIG
            // Auto-clear reset bit to prevent infinite loop during test
            rx_buff[0] = mock_regs[last_reg_addr];
            mock_regs[last_reg_addr] &= ~0x40; // MAX30102_MODE_RESET
        } else {
            for(int i=0; i<length; i++) {
                rx_buff[i] = mock_regs[last_reg_addr + i];
            }
        }
        return 0;
    }
#else
    #include "pulp.h"
#endif
#include "max30102.h"
#include "max30102_regs.h"
#include <stdio.h>
#include <stdlib.h>

int tests_passed = 0;
int tests_failed = 0;

#define ASSERT(condition, test_name) \
    do { \
        if (!(condition)) { \
            printf("[FAIL] %s\n", test_name); \
            tests_failed++; \
        } else { \
            printf("[PASS] %s\n", test_name); \
            tests_passed++; \
        } \
    } while (0)

max30102_t sensor;

void test_initialization() {
    printf("\n--- Test: Initialization ---\n");
    max30102_status_t status = max30102_init(&sensor, 0); // Use I2C port 0
    ASSERT(status == MAX30102_OK, "max30102_init returns OK");
}

void test_part_id() {
    printf("\n--- Test: Part ID ---\n");
    uint8_t part_id = 0;
    max30102_status_t status = max30102_get_part_id(&sensor, &part_id);
    ASSERT(status == MAX30102_OK, "max30102_get_part_id returns OK");
    ASSERT(part_id == MAX30102_EXPECTED_PART_ID, "Part ID is 0x15");
}

void test_calibration() {
    printf("\n--- Test: Calibration (LED Amplitude) ---\n");
    // Set typical current for 7mA
    max30102_status_t status = max30102_set_led_amplitude(&sensor, 0x24, 0x24);
    ASSERT(status == MAX30102_OK, "max30102_set_led_amplitude returns OK");
}

void test_fifo_read() {
    printf("\n--- Test: FIFO Read ---\n");
    uint32_t red_val = 0, ir_val = 0;
    
    // Clear FIFO first
    max30102_status_t status = max30102_clear_fifo(&sensor);
    ASSERT(status == MAX30102_OK, "FIFO clear returns OK");
    
    // Read (might be empty initially)
    status = max30102_read_fifo(&sensor, &red_val, &ir_val);
    ASSERT(status == MAX30102_OK, "FIFO read returns OK");
    
    printf("Read FIFO values -> Red: %u, IR: %u\n", red_val, ir_val);
}

void test_continuous_read() {
    printf("\n--- Test: Continuous FIFO Reading ---\n");

    uint32_t red_val = 0;
    uint32_t ir_val  = 0;

    while (1) {
        max30102_status_t status =
            max30102_read_fifo(&sensor, &red_val, &ir_val);

        switch (status) {
            case MAX30102_OK:
                printf("RED : %lu\tIR : %lu\n",
                       (unsigned long)red_val,
                       (unsigned long)ir_val);
                break;

            case MAX30102_ERROR_NO_DATA:
                // Belum ada sampel baru, tidak perlu print apa-apa
                break;

            default:
                printf("Read Error (%d)\n", status);
                break;
        }

        for (volatile int i = 0; i < 100000; i++);
    }
}

int main() {
    printf("==========================================\n");
    printf("      MAX30102 UNIT TEST RUNNER           \n");
    printf("==========================================\n");
    
    test_initialization();
    test_part_id();
    test_calibration();
    test_fifo_read();
    test_continuous_read();
    
    printf("\n==========================================\n");
    printf("TEST RESULTS: %d PASS, %d FAIL\n", tests_passed, tests_failed);
    printf("==========================================\n");
    
    if (tests_failed > 0) return -1;
    return 0;
}

#ifndef SOFTWARE_TEST
void pe_start(void) {}

// Dummy micros() to satisfy linker error in I2C driver
uint32_t micros() {
    return 0;
}
#endif
