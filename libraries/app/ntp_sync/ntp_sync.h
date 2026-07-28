#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include <stdint.h>

// Platform switch
#ifdef TEST_DI_LAPTOP
    typedef void i2c_t;
    #include "rtc_types.h"
#else
    #include "pulp.h"
    #include "../../sensors/DS1307/rtc.h"
#endif

// ===== Error Codes =====
#define NTP_OK              0
#define NTP_ERR_SOCKET     -1   // Failed to open network socket
#define NTP_ERR_DNS        -2   // Failed to resolve hostname
#define NTP_ERR_CONNECT    -3   // Failed to connect to NTP server
#define NTP_ERR_SEND       -4   // Failed to send NTP packet
#define NTP_ERR_TIMEOUT    -5   // Server response timeout (5s)
#define NTP_ERR_RTC_SET    -6   // Failed to write time to RTC
#define NTP_ERR_INVALID    -7   // Invalid timestamp from server

// ===== Configuration =====
#define NTP_SERVER         "pool.ntp.org"
#define NTP_PORT           123
#define NTP_PACKET_SIZE    48
#define NTP_TIMEOUT_SEC    5
#define NTP_UTC_OFFSET_WIB 25200  // UTC+7 (7 * 3600)

// ===== Public API =====

// Sync time from internet to RTC (call once)
int ntp_sync_now(i2c_t *rtc_handle);

// Set RTC time manually without internet
int ntp_manual_set(i2c_t *rtc_handle, rtc_time_t *time);

#endif // NTP_SYNC_H
