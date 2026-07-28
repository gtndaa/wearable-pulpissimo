/**
 * ntp_sync.h - NTP Time Synchronization Module for STM32
 *
 * Hardware: W5500 Ethernet (SPI) + DS1307 RTC (I2C)
 * Platform: STM32 Nucleo F401RE via PlatformIO (Arduino framework)
 *
 * Ported from PULPissimo RISC-V bare-metal implementation.
 * Core NTP logic (packet parsing, timestamp conversion) is preserved.
 * Hardware access layer replaced with Arduino Ethernet + RTClib.
 */
#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include <Arduino.h>
#include <RTClib.h>

// ===== Error Codes (preserved from original) =====
#define NTP_OK              0
#define NTP_ERR_SOCKET     -1   // Failed to open network socket
#define NTP_ERR_DNS        -2   // Failed to resolve hostname
#define NTP_ERR_CONNECT    -3   // Failed to connect to NTP server
#define NTP_ERR_SEND       -4   // Failed to send NTP packet
#define NTP_ERR_TIMEOUT    -5   // Server response timeout
#define NTP_ERR_RTC_SET    -6   // Failed to write time to RTC
#define NTP_ERR_INVALID    -7   // Invalid timestamp from server
#define NTP_ERR_ETH_INIT   -8   // Ethernet hardware init failed
#define NTP_ERR_RTC_INIT   -9   // RTC hardware init failed

// ===== Configuration =====
#define NTP_SERVER_IP      IPAddress(162, 159, 200, 1)  // Cloudflare NTP (no DNS needed)
#define NTP_PORT           123
#define NTP_PACKET_SIZE    48
#define NTP_TIMEOUT_MS     5000   // 5 second timeout
#define NTP_UTC_OFFSET_WIB 25200  // UTC+7 hours (7 * 3600 seconds)
#define NTP_UNIX_DELTA     2208988800UL  // Seconds between 1900 (NTP) and 1970 (Unix)

// ===== Network Configuration =====
// Static IP for the board (no DHCP needed)
#define BOARD_IP           IPAddress(192, 168, 1, 111)
#define BOARD_GATEWAY      IPAddress(192, 168, 1, 1)
#define BOARD_SUBNET       IPAddress(255, 255, 255, 0)

// W5500 Chip Select pin = D10 (PB6 on Nucleo F401RE)
#define W5500_CS_PIN       10

// ===== Public API =====

/**
 * Initialize all hardware (Ethernet W5500 + RTC DS1307).
 * Must be called once in setup() before using other functions.
 * Returns NTP_OK on success, or negative error code on failure.
 */
int ntp_init(RTC_DS1307 &rtc);

/**
 * Sync time from internet NTP server to DS1307 RTC.
 * Sends UDP packet to Cloudflare NTP, parses response,
 * converts to WIB (UTC+7), and writes to RTC.
 * Returns NTP_OK on success, or negative error code on failure.
 */
int ntp_sync_now(RTC_DS1307 &rtc);

/**
 * Set RTC time manually without internet connection.
 * Useful for testing or when network is unavailable.
 * Returns NTP_OK on success, or negative error code on failure.
 */
int ntp_manual_set(RTC_DS1307 &rtc,
                   uint8_t year, uint8_t month, uint8_t date,
                   uint8_t hour, uint8_t minute, uint8_t second);

/**
 * Read current time from RTC and print to Serial.
 * Convenience function for debugging.
 */
void ntp_print_rtc_time(RTC_DS1307 &rtc);

#endif // NTP_SYNC_H
