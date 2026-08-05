# MPU6050 Library for PULP

Simple MPU6050 driver for PULP-based platforms using the PULP I2C driver.

## Features

- Initialize MPU6050 over I2C
- Read 3-axis accelerometer data
- Read 3-axis gyroscope data
- Read accelerometer and gyroscope data in a single burst transaction
- Physical unit conversion included
  - Accelerometer: milli-g (mg)
  - Gyroscope: milli-degrees per second (mdps)

## Default Configuration

| Parameter | Value |
|-----------|-------|
| I2C Address | `0x69` (AD0 = VCC) |
| I2C Speed | 400 kHz |
| Accelerometer Range | ±2 g |
| Gyroscope Range | ±250 dps |
| Sample Rate | 125 Hz |
| Digital Low Pass Filter | 44 Hz (Accel), 42 Hz (Gyro) |

## Data Types

### `accel_data_t`

Accelerometer measurement in milli-g.

```c
typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
} accel_data_t;
```

### `gyro_data_t`

Gyroscope measurement in milli-degrees per second.

```c
typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
} gyro_data_t;
```

## API Reference

### `mpu6050_open()`

Initializes the I2C peripheral and opens communication with the sensor.

```c
i2c_t *mpu6050_open(void);
```

**Returns**

- `i2c_t*` on success
- `NULL` if I2C initialization fails

---

### `mpu6050_init()`

Configures the MPU6050 and verifies communication using the `WHO_AM_I` register.

```c
int mpu6050_init(i2c_t *i2c);
```

**Parameters**

| Parameter | Description |
|----------|-------------|
| `i2c` | Pointer returned by `mpu6050_open()` |

**Returns**

| Value | Description |
|-------|-------------|
| `MPU6050_OK` | Initialization successful |
| `MPU6050_ERR_COMM` | I2C communication failed |
| `MPU6050_ERR_WHO_AM_I` | Device ID mismatch |
| `MPU6050_ERR_CFG` | Configuration failed |

---

### `mpu6050_read_accel()`

Reads 3-axis accelerometer data.

```c
int mpu6050_read_accel(i2c_t *i2c, accel_data_t *out);
```

**Parameters**

| Parameter | Description |
|----------|-------------|
| `i2c` | I2C handle |
| `out` | Output accelerometer data |

**Output Unit**

- `x`, `y`, `z` in **milli-g (mg)**

**Returns**

- `MPU6050_OK`
- `MPU6050_ERR_READ`

---

### `mpu6050_read_gyro()`

Reads 3-axis gyroscope data.

```c
int mpu6050_read_gyro(i2c_t *i2c, gyro_data_t *out);
```

**Parameters**

| Parameter | Description |
|----------|-------------|
| `i2c` | I2C handle |
| `out` | Output gyroscope data |

**Output Unit**

- `x`, `y`, `z` in **milli-degrees per second (mdps)**

**Returns**

- `MPU6050_OK`
- `MPU6050_ERR_READ`

---

### `mpu6050_read_all()`

Reads accelerometer and gyroscope data using a single 14-byte burst read.

```c
int mpu6050_read_all(
    i2c_t *i2c,
    accel_data_t *accel,
    gyro_data_t *gyro
);
```

**Parameters**

| Parameter | Description |
|----------|-------------|
| `i2c` | I2C handle |
| `accel` | Accelerometer output |
| `gyro` | Gyroscope output |

**Returns**

- `MPU6050_OK`
- `MPU6050_ERR_READ`

## Error Codes

| Code | Description |
|------|-------------|
| `MPU6050_OK` | Operation successful |
| `MPU6050_ERR_I2C_OPEN` | Failed to open I2C peripheral |
| `MPU6050_ERR_COMM` | I2C communication failed |
| `MPU6050_ERR_WHO_AM_I` | Incorrect device ID |
| `MPU6050_ERR_CFG` | Sensor configuration failed |
| `MPU6050_ERR_READ` | Failed to read sensor data |

## Example

```c
#include "mpu6050.h"

int main(void)
{
    i2c_t *i2c = mpu6050_open();
    if (i2c == NULL)
        return -1;

    if (mpu6050_init(i2c) != MPU6050_OK)
        return -1;

    accel_data_t accel;
    gyro_data_t gyro;

    while (1)
    {
        if (mpu6050_read_all(i2c, &accel, &gyro) == MPU6050_OK)
        {
            printf("Accel: %ld %ld %ld mg\n",
                   accel.x, accel.y, accel.z);

            printf("Gyro : %ld %ld %ld mdps\n",
                   gyro.x, gyro.y, gyro.z);
        }
    }

    return 0;
}
```

## Notes

- The library assumes the MPU6050 AD0 pin is connected to VCC, resulting in I2C address `0x69`.
- Sensor values are automatically converted from raw ADC values to physical units.
- `mpu6050_read_all()` is recommended when both accelerometer and gyroscope data are required because it performs a single burst read over I2C.