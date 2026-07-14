#include <stdint.h>
#include <stdio.h>
#include "pulp.h"
#include "implem/tick.h"
//#include "lis3dhtr.h" 
#include "mpu6050.h"
#include "step_counter.h"

void pe_start(void) {}

int main(void)
{
    pos_tick_init();
    printf("Step Counter\n\r");

    /* */
    i2c_t *i2c = mpu6050_open();
    if (i2c == NULL) return -1;
    if (mpu6050_init(i2c) != MPU6050_OK) return -1;
    /**/

    /*
    i2c_t *i2c = lis3dhtr_open();
    if (i2c == NULL) return -1;
    if (lis3dhtr_init(i2c) != LIS3DHTR_OK) return -1;
    */


    step_counter_t sc;
    step_counter_init(&sc);
    printf("[OK] step_counter_init succeeded\n\r");
    printf("Mulai deteksi langkah...\n\r");

    accel_data_t accel;
    uint32_t     last_print_ms = 0;
    uint32_t     last_steps    = 0;

    while (1) {
        long now_ms = pos_tick_get_counter_ms();

        int ret = mpu6050_read_accel(i2c, &accel);
        if (ret != MPU6050_OK) {
            printf("[ERROR] accel read failed, code=%d\n\r", ret);
            continue;
        }
        /* 
        int ret = lis3dhtr_read_accel(i2c, &accel);
        if (ret != LIS3DHTR_OK) {
            printf("[ERROR] accel read failed, code=%d\n\r", ret);
            continue;
        }
        */

        int stepped = step_counter_update(&sc, accel.x, accel.y, accel.z, now_ms);

        if (stepped) {
            printf("[STEP] total=%lu\n\r", (unsigned long)step_counter_get(&sc));
        }

        if ((long)(now_ms - last_print_ms) >= 5000) {
            uint32_t current_steps = step_counter_get(&sc);
            uint32_t steps_in_5s   = current_steps - last_steps;
            printf("--- 5s summary: total=%lu, steps_in_window=%lu ---\n\r",
                   (unsigned long)current_steps,
                   (unsigned long)steps_in_5s);
            last_steps    = current_steps;
            last_print_ms = now_ms;
        }

        for (volatile int d = 0; d < 100000; d++);
    }

    i2c_close(i2c);
    return 0;
}