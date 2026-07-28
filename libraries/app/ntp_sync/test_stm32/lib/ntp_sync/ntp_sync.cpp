/**
 * ntp_sync.cpp - NTP Time Synchronization for STM32
 *
 * Ported from PULPissimo RISC-V bare-metal implementation.
 *
 * 3 core functions (same architecture as original):
 *   1. ntp_eth_connect()      - Initialize W5500 + open UDP socket
 *   2. ntp_fetch_timestamp()  - Send NTP request + extract timestamp
 *   3. ntp_write_to_rtc()     - Convert UTC→WIB + write to DS1307
 *
 * Hardware access replaced:
 *   - PULPissimo i2c_write()  → Arduino RTClib (rtc.adjust())
 *   - PULPissimo lwip/socket  → Arduino Ethernet (EthernetUDP)
 */

#include "ntp_sync.h"
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

// ===== Private Variables =====

// MAC address for W5500 (can be any unique value on your LAN)
static byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// UDP socket instance for NTP communication
static EthernetUDP udp;

// NTP packet buffer (48 bytes, same as original)
static byte ntpPacket[NTP_PACKET_SIZE];

// ===================================================
// INTERNAL: Initialize W5500 Ethernet hardware
// (Replaces old ntp_network_connect for board mode)
// ===================================================
static int ntp_eth_connect(void)
{
    Serial.println("[NTP] Initializing W5500 Ethernet...");

    // Initialize Ethernet with DHCP (Auto IP), timeout 10 detik
    Ethernet.init(W5500_CS_PIN);
    Serial.println("[NTP] Meminta IP Address dari DHCP Router... (max 10 detik)");
    
    if (Ethernet.begin(mac, 10000, 4000) == 0) {
        Serial.println("[NTP WARN] DHCP Gagal! Kabel LAN belum dicolok atau router tidak merespon.");
        Serial.println("[NTP WARN] Menggunakan IP statis sebagai fallback...");
        Ethernet.begin(mac, BOARD_IP, IPAddress(8, 8, 8, 8), BOARD_GATEWAY, BOARD_SUBNET);
    }

    // Check if W5500 hardware is present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("[NTP ERR -1] W5500 chip not detected on SPI bus!");
        Serial.println("[NTP] Check wiring: GREEN=D13(SCK), PURPLE=D12(MISO), BLUE=D11(MOSI), WHITE=D10(CS)");
        return NTP_ERR_ETH_INIT;
    }

    // Check cable connection
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("[NTP WARN] Ethernet cable not connected (link down)");
        // Continue anyway - some W5500 modules don't support link detection
    }

    Serial.print("[NTP] Board IP: ");
    Serial.println(Ethernet.localIP());

    // Open UDP socket on any local port
    udp.begin(8888);
    Serial.println("[NTP] UDP socket opened on port 8888");

    return NTP_OK;
}

// ===================================================
// INTERNAL: Send NTP request and extract timestamp
// (Core logic preserved from original ntp_fetch_timestamp)
// ===================================================
static uint32_t ntp_fetch_timestamp(int *err)
{
    Serial.println("[NTP] Sending NTP request to 162.159.200.1:123...");

    // Build NTP request packet (same as original: first byte = 0x1B)
    memset(ntpPacket, 0, NTP_PACKET_SIZE);
    ntpPacket[0] = 0x1B;  // LI=0, Version=3, Mode=3 (client)

    // Send UDP packet to NTP server
    udp.beginPacket(NTP_SERVER_IP, NTP_PORT);
    udp.write(ntpPacket, NTP_PACKET_SIZE);
    if (udp.endPacket() == 0) {
        Serial.println("[NTP ERR -4] Failed to send NTP packet");
        *err = NTP_ERR_SEND;
        return 0;
    }

    // Wait for response with timeout
    unsigned long startMs = millis();
    int packetSize = 0;
    while (packetSize == 0) {
        packetSize = udp.parsePacket();
        if (millis() - startMs > NTP_TIMEOUT_MS) {
            Serial.println("[NTP ERR -5] Timeout: no response in 5 seconds");
            *err = NTP_ERR_TIMEOUT;
            return 0;
        }
        delay(10);
    }

    // Read response
    udp.read(ntpPacket, NTP_PACKET_SIZE);

    // Extract Transmit Timestamp from bytes 40-43
    // (EXACT SAME logic as original PULPissimo code)
    uint32_t ts = (uint32_t)ntpPacket[40] << 24 |
                  (uint32_t)ntpPacket[41] << 16 |
                  (uint32_t)ntpPacket[42] << 8  |
                  (uint32_t)ntpPacket[43];

    Serial.print("[NTP] Raw timestamp: ");
    Serial.println(ts);

    *err = NTP_OK;
    return ts;
}

// ===================================================
// INTERNAL: Convert NTP timestamp → WIB → write to RTC
// (Same conversion logic as original time_sync_to_rtc)
// ===================================================
static int ntp_write_to_rtc(RTC_DS1307 &rtc, uint32_t ntp_ts)
{
    // Validate timestamp (same check as original)
    if (ntp_ts < 3913056000UL) {
        Serial.println("[NTP ERR -7] Invalid timestamp from server");
        return NTP_ERR_INVALID;
    }

    // Convert NTP epoch (1900) to Unix epoch (1970)
    uint32_t unix_time = ntp_ts - NTP_UNIX_DELTA;

    // Add WIB timezone offset (UTC+7 = +25200 seconds)
    unix_time += NTP_UTC_OFFSET_WIB;

    // Create DateTime from Unix timestamp and write to RTC
    DateTime dt(unix_time);
    rtc.adjust(dt);

    // Print result (same format as original)
    Serial.print("[NTP] Time (WIB): ");
    char buf[22];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d",
             dt.day(), dt.month(), dt.year(),
             dt.hour(), dt.minute(), dt.second());
    Serial.println(buf);
    Serial.println("[NTP] RTC synchronized!");

    return NTP_OK;
}

// ===================================================
// PUBLIC: ntp_init() - Initialize hardware
// ===================================================
int ntp_init(RTC_DS1307 &rtc)
{
    Serial.println("\n========================================");
    Serial.println("  NTP Sync Module - STM32 + W5500");
    Serial.println("========================================\n");

    // Initialize I2C and RTC DS1307
    if (!rtc.begin()) {
        Serial.println("[NTP ERR -9] DS1307 RTC not found on I2C bus!");
        Serial.println("[NTP] Check wiring: SCL=D15, SDA=D14");
        return NTP_ERR_RTC_INIT;
    }
    Serial.println("[NTP] DS1307 RTC detected on I2C");

    // Initialize SPI and W5500 Ethernet
    int rc = ntp_eth_connect();
    if (rc != NTP_OK) return rc;

    return NTP_OK;
}

// ===================================================
// PUBLIC: ntp_sync_now() - Sync once from internet
// (Same flow as original PULPissimo version)
// ===================================================
int ntp_sync_now(RTC_DS1307 &rtc)
{
    int rc;

    Serial.println("\n=== NTP SYNC START ===");

    // Fetch timestamp from NTP server
    int fetch_err = NTP_OK;
    uint32_t ts = ntp_fetch_timestamp(&fetch_err);
    if (fetch_err != NTP_OK) return fetch_err;

    // Convert and write to RTC
    rc = ntp_write_to_rtc(rtc, ts);

    if (rc == NTP_OK) {
        Serial.println("=== NTP SYNC OK ===\n");
    } else {
        Serial.println("=== NTP SYNC FAILED ===\n");
    }
    return rc;
}

// ===================================================
// PUBLIC: ntp_manual_set() - Set RTC without internet
// (Same as original PULPissimo version)
// ===================================================
int ntp_manual_set(RTC_DS1307 &rtc,
                   uint8_t year, uint8_t month, uint8_t date,
                   uint8_t hour, uint8_t minute, uint8_t second)
{
    Serial.print("[NTP MANUAL] Set: ");
    char buf[22];
    snprintf(buf, sizeof(buf), "%02d/%02d/20%02d %02d:%02d:%02d",
             date, month, year, hour, minute, second);
    Serial.println(buf);

    rtc.adjust(DateTime(2000 + year, month, date, hour, minute, second));

    Serial.println("[NTP] RTC updated manually");
    return NTP_OK;
}

// ===================================================
// PUBLIC: ntp_print_rtc_time() - Read and display time
// ===================================================
void ntp_print_rtc_time(RTC_DS1307 &rtc)
{
    DateTime now = rtc.now();

    char buf[22];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d",
             now.day(), now.month(), now.year(),
             now.hour(), now.minute(), now.second());
    Serial.print("[RTC] ");
    Serial.println(buf);
}
