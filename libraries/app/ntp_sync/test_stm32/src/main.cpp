/**
 * main.cpp - NTP Time Sync for STM32 Nucleo F401RE
 *
 * Hardware connections (jumper wires from custom board):
 *   SPI (W5500 Ethernet):
 *     GREEN  wire (R246) → D13 (SCK)
 *     PURPLE wire (R244) → D12 (MISO)
 *     BLUE   wire (R259) → D11 (MOSI)
 *     WHITE  wire (R144) → D10 (CS)
 *
 *   I2C (DS1307 RTC):
 *     SCL wire → D15
 *     SDA wire → D14
 *
 *   GND: Custom board GND → Nucleo GND
 *
 * Usage:
 *   1. Connect all wires as described above
 *   2. Connect Ethernet cable to custom board's RJ45 port
 *   3. Connect STM32 Nucleo to laptop via USB
 *   4. Press Upload (→) button in VSCode
 *   5. Open Serial Monitor (plug icon) to see output
 */

#include <Arduino.h>
#include "ntp_sync.h"

// DS1307 RTC instance (communicates via I2C on D14/D15)
RTC_DS1307 rtc;

// Print RTC time every 1 second
unsigned long lastPrint = 0;
const unsigned long PRINT_INTERVAL = 1000;

void setup()
{
    // Initialize serial for debugging output
    Serial.begin(115200);
    while (!Serial) { ; }  // Wait for serial port (USB CDC)
    delay(1000);  // Give hardware time to stabilize

    Serial.println("==========================================");
    Serial.println("  STM32 + W5500 NTP Sync");
    Serial.println("  Board: Nucleo F401RE");
    Serial.println("  Custom Wearable Board (Ardunesia)");
    Serial.println("==========================================");

    // --- Step 1: Initialize all hardware ---
    int rc = ntp_init(rtc);
    if (rc != NTP_OK) {
        Serial.print("[MAIN] Init failed with error: ");
        Serial.println(rc);
        Serial.println("[MAIN] Halted. Fix wiring and reset board.");
        while (1) { delay(1000); }  // Halt on failure
    }

    // --- Step 2: Sync time from internet ---
    Serial.println("\n--- Syncing time from NTP server ---");
    rc = ntp_sync_now(rtc);
    if (rc == NTP_OK) {
        Serial.println("[MAIN] SUCCESS! RTC is now synchronized with internet time.");
    } else {
        Serial.print("[MAIN] NTP sync failed (error: ");
        Serial.print(rc);
        Serial.println(")");

        // Fallback: set time manually if NTP fails
        Serial.println("[MAIN] Setting manual time as fallback...");
        ntp_manual_set(rtc, 26, 7, 16, 12, 0, 0);  // 16/07/2026 12:00:00 WIB
    }

    Serial.println("\n--- RTC is now running. Reading every 1 second: ---\n");
}

void loop()
{
    // Print current RTC time every 1 second
    unsigned long now = millis();
    if (now - lastPrint >= PRINT_INTERVAL) {
        lastPrint = now;
        ntp_print_rtc_time(rtc);
    }
}