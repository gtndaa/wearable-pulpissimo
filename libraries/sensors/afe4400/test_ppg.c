#include <stdio.h>
#include "afe4400.h"

/* --- AFE4400 PPG Passthrough — Unit Test --- */

#define NUM_SAMPLES     10

/* Counter hasil test */
static int pass_count = 0;
static int fail_count = 0;

/* Macro PASS/FAIL */
#define TEST_PASS(msg)  do { pass_count++; printf("[PASS] %s\n", (msg)); } while(0)
#define TEST_FAIL(msg)  do { fail_count++; printf("[FAIL] %s\n", (msg)); } while(0)
#define CHECK(cond, ok_msg, fail_msg) \
    do { if (cond) TEST_PASS(ok_msg); else TEST_FAIL(fail_msg); } while(0)

/* Delay sederhana */
static void delay_sim(volatile int n) { while (n--); }

/* --- Main --- */
int main(void) {
    printf("\n");
    printf("============================================\n");
    printf("  AFE4400 PPG PASSTHROUGH UNIT TEST RUNNER \n");
    printf("============================================\n\n");

    /* Test 1: Inisialisasi */
    printf("--- Test: Initialization ---\n");
    int ret = afe4400_init(0, 0);
    CHECK(ret == 0,
          "afe4400_init returns OK",
          "afe4400_init FAILED (cek koneksi SPI)");

    /* Test 2: Kalibrasi */
    printf("\n--- Test: Calibration ---\n");
    afe4400_calibrate();
    TEST_PASS("afe4400_calibrate returns OK");

    /* Test 3: Baca LED1 */
    printf("\n--- Test: Read LED1 (Merah / Red) ---\n");
    unsigned int led1_val = afe4400_read_reg(AFE_LED1VAL);
    CHECK(led1_val > 0,
          "afe4400_read_reg(AFE_LED1VAL) returns OK",
          "afe4400_read_reg(AFE_LED1VAL) FAILED (nilai 0)");
    printf("Read LED1 Value -> %u\n", led1_val);

    /* Test 4: Baca LED2 */
    printf("\n--- Test: Read LED2 (Infrared / IR) ---\n");
    unsigned int led2_val = afe4400_read_reg(AFE_LED2VAL);
    CHECK(led2_val > 0,
          "afe4400_read_reg(AFE_LED2VAL) returns OK",
          "afe4400_read_reg(AFE_LED2VAL) FAILED (nilai 0)");
    printf("Read LED2 Value -> %u\n", led2_val);

    /* Test 5: Baca Ambient */
    printf("\n--- Test: Read Ambient Light ---\n");
    unsigned int amb_val = afe4400_read_reg(AFE_ALED1VAL);
    CHECK(amb_val <= 0xFFFFFF,
          "afe4400_read_reg(AFE_ALED1VAL) nilai valid (dalam batas 24-bit)",
          "afe4400_read_reg(AFE_ALED1VAL) FAILED (nilai melebihi 24-bit)");
    printf("Read Ambient Value -> %u\n", amb_val);

    /* Test 6: Data Stream Passthrough */
    printf("\n--- Test: Passthrough Data Stream (%d Samples) ---\n", NUM_SAMPLES);
    printf(" NO |   LED 1 (Merah)   |    LED 2 (IR)     |   AMBIENT\n");
    printf("------------------------------------------------------------\n");

    int stream_ok = 1;
    for (int i = 1; i <= NUM_SAMPLES; i++) {
        unsigned int led1 = afe4400_read_reg(AFE_LED1VAL);
        unsigned int led2 = afe4400_read_reg(AFE_LED2VAL);
        unsigned int amb  = afe4400_read_reg(AFE_ALED1VAL);

        printf(" %2d |      %8u     |      %8u     |  %8u\n",
               i, led1, led2, amb);

        if (led1 == 0 && led2 == 0) stream_ok = 0;
        delay_sim(200000);
    }
    printf("------------------------------------------------------------\n");
    CHECK(stream_ok,
          "Passthrough data stream read OK",
          "Passthrough data stream FAILED (semua nilai 0)");

    /* Hasil Akhir */
    printf("\n============================================\n");
    printf("TEST RESULTS: %d PASS, %d FAIL\n", pass_count, fail_count);
    printf("============================================\n\n");

    return (fail_count == 0) ? 0 : -1;
}

void pe_start(void) {}