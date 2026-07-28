/**
 * test.c - Unit Test for NTP Sync (Task #659)
 *
 * Compile: make
 * Run:     ./test_ntp
 */

#include <stdio.h>
#include "ntp_sync.h"

// Fix linker errors on PULPissimo board
uint32_t micros(void) { return 0; }
void pe_start(void *arg) { (void)arg; }

int main(void)
{
    printf("==========================================\n");
    printf("  UNIT TEST: NTP SYNC (Task #659)         \n");
#ifdef TEST_DI_LAPTOP
    printf("  Mode: Laptop (Wi-Fi)                    \n");
    i2c_t *rtc_handle = NULL;
#else
    printf("  Mode: Board (Ethernet)                  \n");
    i2c_dev_t dev;
    i2c_t *rtc_handle = NULL;
    int rc_init = rtc_init(&dev, &rtc_handle);
    if (rc_init != RTC_OK) {
        printf("[INIT] Gagal buka I2C RTC (error: %d)\n", rc_init);
        return 1;
    }
    printf("[INIT] I2C RTC berhasil dibuka\n");
#endif
    printf("==========================================\n");

    // TEST 1: Sync from internet (call once)
    printf("\n--- TEST 1: Sync from Internet ---\n");

    int rc = ntp_sync_now(rtc_handle);

    if (rc == NTP_OK)
        printf("[TEST 1] PASSED\n");
    else
        printf("[TEST 1] FAILED (error: %d)\n", rc);

    // TEST 2: Manual set (no internet needed)
    printf("\n--- TEST 2: Manual Set ---\n");

    rtc_time_t waktu = {
        .seconds = 0,  .minutes = 30, .hours = 14,
        .day     = 1,  .date    = 14, .month = 7,
        .year    = 26
    };

    rc = ntp_manual_set(rtc_handle, &waktu);

    if (rc == NTP_OK)
        printf("[TEST 2] PASSED\n");
    else
        printf("[TEST 2] FAILED (error: %d)\n", rc);

    // Summary
    printf("\n==========================================\n");
    printf("  ALL TESTS COMPLETE                      \n");
    printf("==========================================\n");

    return 0;
}
