/**
 * @file max30102.h
 * @brief MAX30102 Pulse Oximeter & Heart Rate Sensor - Public API
 *
 * Simple interface for the MAX30102 sensor on PULP/Nusacore FPGA boards.
 */

#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>

// MAX30102 Status Codes
typedef enum {
    MAX30102_OK = 0,
    MAX30102_ERROR_I2C = -1,
    MAX30102_ERROR_NOT_FOUND = -2,
    MAX30102_ERROR_INVALID_PARAM = -3,
    MAX30102_ERROR_NO_DATA = -4
} max30102_status_t;

// Context structure for the sensor
typedef struct {
    int i2c_port;     // I2C port number (usually 0)
    uint8_t i2c_addr; // I2C address (default 0x57)
    void* i2c_dev;    // Pointer to PULP I2C device handle
} max30102_t;

/**
 * @brief Initialize the MAX30102 driver and hardware
 * 
 * @param dev Pointer to MAX30102 context
 * @param i2c_port I2C port number
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_init(max30102_t *dev, int i2c_port);

/**
 * @brief Soft reset the MAX30102
 * 
 * @param dev Pointer to MAX30102 context
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_reset(max30102_t *dev);

/**
 * @brief Read the Part ID of the sensor
 * 
 * @param dev Pointer to MAX30102 context
 * @param part_id Pointer to store the read part ID (should be 0x15)
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_get_part_id(max30102_t *dev, uint8_t *part_id);

/**
 * @brief Set the LED current amplitude (Calibration)
 * 
 * @param dev Pointer to MAX30102 context
 * @param red_pa Red LED current amplitude (0x00 to 0xFF)
 * @param ir_pa IR LED current amplitude (0x00 to 0xFF)
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_set_led_amplitude(max30102_t *dev, uint8_t red_pa, uint8_t ir_pa);

/**
 * @brief Clear the FIFO read/write pointers
 * 
 * @param dev Pointer to MAX30102 context
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_clear_fifo(max30102_t *dev);

/**
 * @brief Read one sample (Red and IR data) from the FIFO
 * 
 * @param dev Pointer to MAX30102 context
 * @param red_data Pointer to store 18-bit Red LED data
 * @param ir_data Pointer to store 18-bit IR LED data
 * @return MAX30102_OK on success, error code otherwise
 */
max30102_status_t max30102_read_fifo(max30102_t *dev, uint32_t *red_data, uint32_t *ir_data);

#endif // MAX30102_H
