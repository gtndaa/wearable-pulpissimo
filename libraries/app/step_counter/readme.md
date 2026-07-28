# Step Counter

A lightweight pedometer library for PULPissimo using threshold-based peak detection on accelerometer magnitude.

Sensor-agnostic: works with any accelerometer that outputs milli-g integer values (tested with LIS3DHTR and MPU6050).

---

## Algorithm

**Method:** Threshold-based peak detection on acceleration magnitude squared.

**References:**
- Brajdic, A. & Harle, R. (2013). *Walk detection and step counting on unconstrained smartphones*. UbiComp 2013. (basis for threshold + debounce approach)
- Weinberg, H. (2002). *Using the ADXL202 in Pedometer and Personal Navigation Applications*. Analog Devices AN-602. (basis for magnitude vector calculation)

**How it works:**
1. Compute magnitude squared: `|a|² = (x/16)² + (y/16)² + (z/16)²` : division prevents int32 overflow before squaring
2. Apply 4-sample moving average to reduce sensor noise
3. Detect when smoothed signal crosses above threshold then back below. one full crossing = one step
4. Debounce: ignore steps that arrive faster than 250ms (~4 steps/sec max)

---

## Files

```
step_counter.h    — thresholds, state struct, function declarations
step_counter.c    — algorithm implementation
test.c            — usage example (uses LIS3DHTR or MPU6050)
```

---

## Tuning Parameters

Defined in `step_counter.h`:

|       Parameter      |Default|                                Description                                              |
|----------------------|-------|-----------------------------------------------------------------------------------------|
| `STEP_THRESHOLD_MG2` | `400` | Magnitude² threshold. Raise if too many false steps, lower if steps are missed.         |
| `STEP_DEBOUNCE_MS`   | `250` | Minimum time between two valid steps in ms. Lower for running, higher for slow walking. |
| `STEP_WINDOW_SIZE`   | `4`   | Moving average window size. Larger = smoother but slower to react.                      |

**Typical threshold values by activity:**

|   Activity  | Suggested `STEP_THRESHOLD_MG2` | Suggested `STEP_DEBOUNCE_MS` |
|-------------|--------------------------------|------------------------------|
|  Slow walk  |             300–400            |            250               |
| Normal walk |             400–600            |            200               |
|  Fast walk  |             600–800            |            180               |
|   Running   |             800–1200           |            150               |

> **Note:** Threshold depends heavily on sensor placement and sensitivity. When tuning, use the debug approach: print `mag_sq` values while walking and set the threshold between the resting value and the peak walking value.

---

## Data Structure

```c
typedef struct {
    uint32_t  step_count;                   // total steps detected
    long      last_step_time_ms;            // timestamp of last step
    int32_t   window[STEP_WINDOW_SIZE];     // moving average buffer
    int       window_idx;
    int32_t   window_sum;
    int       window_full;
    int32_t   prev_avg;
    int       above_threshold;
} step_counter_t;
```

The struct holds all algorithm state. Declare one per sensor/instance and pass it to every `step_counter_update` call.

---

## API

### `step_counter_init`

```c
void step_counter_init(step_counter_t *sc);
```

Initializes all state to zero. Must be called once before `step_counter_update`.

---

### `step_counter_update`

```c
int step_counter_update(step_counter_t *sc,
                        int32_t x_mg, int32_t y_mg, int32_t z_mg,
                        long now_ms);
```

Updates the algorithm with a new accelerometer sample. Call this every time a new sample is available from the sensor.

**Parameters:**
- `sc` — pointer to initialized `step_counter_t`
- `x_mg`, `y_mg`, `z_mg` — acceleration in milli-g on each axis
- `now_ms` — current timestamp in milliseconds from `pos_tick_get_counter_ms()`

**Returns:** `1` if a step was detected this sample, `0` otherwise.

---

### `step_counter_get`

```c
uint32_t step_counter_get(const step_counter_t *sc);
```

Returns the total step count accumulated since init or last reset.

---

### `step_counter_reset`

```c
void step_counter_reset(step_counter_t *sc);
```

Resets the step count to zero without clearing the filter state.

---

## Usage Example

### With LIS3DHTR

```c
#include "pulp.h"
#include "implem/tick.h"
#include "lis3dhtr.h"
#include "step_counter.h"

int main(void)
{
    pos_tick_init();

    i2c_t *i2c = lis3dhtr_open();
    if (i2c == NULL) return -1;
    if (lis3dhtr_init(i2c) != LIS3DHTR_OK) return -1;

    step_counter_t sc;
    step_counter_init(&sc);

    accel_data_t accel;
    while (1) {
        long now_ms = pos_tick_get_counter_ms();

        if (lis3dhtr_read_accel(i2c, &accel) == LIS3DHTR_OK) {
            int stepped = step_counter_update(&sc, accel.x_mg, accel.y_mg, accel.z_mg, now_ms);
            if (stepped)
                printf("[STEP] total=%lu\n\r", (unsigned long)step_counter_get(&sc));
        }

        for (volatile int d = 0; d < 100000; d++);
    }
}
```

### With MPU6050

```c
#include "pulp.h"
#include "implem/tick.h"
#include "mpu6050.h"
#include "step_counter.h"

int main(void)
{
    pos_tick_init();

    i2c_t *i2c = mpu6050_open();
    if (i2c == NULL) return -1;
    if (mpu6050_init(i2c) != MPU6050_OK) return -1;

    step_counter_t sc;
    step_counter_init(&sc);

    mpu6050_accel_t accel;
    while (1) {
        long now_ms = pos_tick_get_counter_ms();

        if (mpu6050_read_accel(i2c, &accel) == MPU6050_OK) {
            int stepped = step_counter_update(&sc, accel.x, accel.y, accel.z, now_ms);
            if (stepped)
                printf("[STEP] total=%lu\n\r", (unsigned long)step_counter_get(&sc));
        }

        for (volatile int d = 0; d < 100000; d++);
    }
}
```

---

## Makefile

### With LIS3DHTR

```makefile
PULP_APP        = test
PULP_APP_C_SRCS = test.c \
                  step_counter.c \
                  ../../sensors/lis3dhtr/lis3dhtr.c

PULP_CFLAGS     = -O3 -g \
                  -I../../sensors/lis3dhtr

include $(PULPRT_HOME)/rules/pulp.mk
```

### With MPU6050

```makefile
PULP_APP        = test
PULP_APP_C_SRCS = test.c \
                  step_counter.c \
                  ../../sensors/mpu6050/mpu6050.c

PULP_CFLAGS     = -O3 -g \
                  -I../../sensors/mpu6050

include $(PULPRT_HOME)/rules/pulp.mk
```

---

## Notes

**Why magnitude squared instead of magnitude:**
Computing `sqrt(x²+y²+z²)` requires floating point which is not supported on RV32I without soft-float overhead. Using magnitude squared `x²+y²+z²` gives the same peak detection behavior since the threshold is also squared.

**Why divide by 16 before squaring:**
At ±2g full scale, axis values can reach ~2000mg. Squaring directly would overflow `int32_t` (max ~2.1 billion). Dividing by 16 first keeps values within safe range before squaring.

**Timing dependency:**
`pos_tick_init()` must be called before the main loop. Without it, `pos_tick_get_counter_ms()` returns invalid values and debounce will not work correctly.