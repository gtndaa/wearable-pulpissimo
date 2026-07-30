/*
 * PPG PASSTHROUGH - FIRMWARE FINAL (v21)
 * ============================================================
 * Firmware pulse-oximeter wearable: AFE4400 (SPI) + STM32.
 * Alur: baca sensor -> filter -> deteksi jari -> hitung BPM -> hitung SpO2.
 * ============================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <math.h>

//=== PIN & REGISTER ===
#define AFE_CS_PIN   PB6
#define AFE_RDY_PIN  PA10
#define AFE_PDN_PIN  PA8
#define AFE_MISO_PIN PA6
#define AFE_MOSI_PIN PA7
#define AFE_SCLK_PIN PA5
#define SPI_SPEED 100000

#define REG_CONTROL0    0x00
#define REG_LED2STC     0x01
#define REG_LED2ENDC    0x02
#define REG_LED2LEDSTC  0x03
#define REG_LED2LEDENDC 0x04
#define REG_ALED2STC    0x05
#define REG_ALED2ENDC   0x06
#define REG_LED1STC     0x07
#define REG_LED1ENDC    0x08
#define REG_LED1LEDSTC  0x09
#define REG_LED1LEDENDC 0x0A
#define REG_ALED1STC    0x0B
#define REG_ALED1ENDC   0x0C
#define REG_LED2CONVST  0x0D
#define REG_LED2CONVEND 0x0E
#define REG_ALED2CONVST 0x0F
#define REG_ALED2CONVEND 0x10
#define REG_LED1CONVST  0x11
#define REG_LED1CONVEND 0x12
#define REG_ALED1CONVST 0x13
#define REG_ALED1CONVEND 0x14
#define REG_ADCRSTSTCT0 0x15
#define REG_ADCRSTENDCT0 0x16
#define REG_ADCRSTSTCT1 0x17
#define REG_ADCRSTENDCT1 0x18
#define REG_ADCRSTSTCT2 0x19
#define REG_ADCRSTENDCT2 0x1A
#define REG_ADCRSTSTCT3 0x1B
#define REG_ADCRSTENDCT3 0x1C
#define REG_PRPCOUNT    0x1D
#define REG_CONTROL1    0x1E
#define REG_TIA_AMB_GAIN 0x21
#define REG_LEDCNTRL    0x22
#define REG_CONTROL2    0x23
#define REG_LED2MALED2VAL 0x2E
#define REG_LED1MALED1VAL 0x2F

//=== KOMUNIKASI SPI DASAR ===
void afeWrite(uint8_t addr, uint32_t data) {
    SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
    digitalWrite(AFE_CS_PIN, LOW);
    delayMicroseconds(10);
    SPI.transfer(addr);
    SPI.transfer((data >> 16) & 0xFF);
    SPI.transfer((data >> 8) & 0xFF);
    SPI.transfer(data & 0xFF);
    delayMicroseconds(10);
    digitalWrite(AFE_CS_PIN, HIGH);
    SPI.endTransaction();
    delayMicroseconds(50);
}

uint32_t afeReadRaw(uint8_t addr) {
    afeWrite(REG_CONTROL0, 0x000001);
    delayMicroseconds(50);
    SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
    digitalWrite(AFE_CS_PIN, LOW);
    delayMicroseconds(10);
    SPI.transfer(addr);
    uint8_t b1 = SPI.transfer(0x00);
    uint8_t b2 = SPI.transfer(0x00);
    uint8_t b3 = SPI.transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(AFE_CS_PIN, HIGH);
    SPI.endTransaction();
    delayMicroseconds(50);
    afeWrite(REG_CONTROL0, 0x000000);
    return ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
}

uint32_t afeReadRetry(uint8_t addr, int maxRetry = 3) {
    uint32_t val = afeReadRaw(addr);
    int attempt = 1;
    while (val == 0xFFFFFF && attempt < maxRetry) {
        delayMicroseconds(200);
        val = afeReadRaw(addr);
        attempt++;
    }
    return val;
}

int32_t signExtend24(uint32_t val) {
    val &= 0xFFFFFF;
    if (val & 0x800000) val |= 0xFF000000UL;
    return (int32_t)val;
}

bool verifyRegister(const char* name, uint8_t addr, uint32_t expected) {
    uint32_t rb = afeReadRetry(addr);
    bool ok = (rb == expected);
    Serial.print(F("    "));
    Serial.print(name);
    Serial.print(F(": tulis=0x"));
    Serial.print(expected, HEX);
    Serial.print(F(" baca=0x"));
    Serial.print(rb, HEX);
    Serial.println(ok ? F(" [OK]") : F(" [BEDA]"));
    return ok;
}

//=== KONFIGURASI REGISTER AFE4400 (urutan & nilai sudah tervalidasi) ===
void afeFullConfig() {
    digitalWrite(AFE_PDN_PIN, LOW);
    delay(200);
    digitalWrite(AFE_PDN_PIN, HIGH);
    delay(1000);
    afeWrite(REG_CONTROL0, 0x000008);
    delay(200);
    afeWrite(REG_CONTROL2, 0x020100);  delay(10);
    afeWrite(REG_PRPCOUNT, 7999);      delay(2);
    afeWrite(REG_LED2STC, 6050);       delay(2);
    afeWrite(REG_LED2ENDC, 7998);      delay(2);
    afeWrite(REG_LED2LEDSTC, 6000);    delay(2);
    afeWrite(REG_LED2LEDENDC, 7999);   delay(2);
    afeWrite(REG_ALED2STC, 50);        delay(2);
    afeWrite(REG_ALED2ENDC, 1998);     delay(2);
    afeWrite(REG_LED1STC, 2050);       delay(2);
    afeWrite(REG_LED1ENDC, 3998);      delay(2);
    afeWrite(REG_LED1LEDSTC, 2000);    delay(2);
    afeWrite(REG_LED1LEDENDC, 3999);   delay(2);
    afeWrite(REG_ALED1STC, 4050);      delay(2);
    afeWrite(REG_ALED1ENDC, 5998);     delay(2);
    afeWrite(REG_LED2CONVST, 4);       delay(2);
    afeWrite(REG_LED2CONVEND, 1999);   delay(2);
    afeWrite(REG_ALED2CONVST, 2004);   delay(2);
    afeWrite(REG_ALED2CONVEND, 3999);  delay(2);
    afeWrite(REG_LED1CONVST, 4004);    delay(2);
    afeWrite(REG_LED1CONVEND, 5999);   delay(2);
    afeWrite(REG_ALED1CONVST, 6004);   delay(2);
    afeWrite(REG_ALED1CONVEND, 7999);  delay(2);
    afeWrite(REG_ADCRSTSTCT0, 0);      delay(2);
    afeWrite(REG_ADCRSTENDCT0, 3);     delay(2);
    afeWrite(REG_ADCRSTSTCT1, 2000);   delay(2);
    afeWrite(REG_ADCRSTENDCT1, 2003);  delay(2);
    afeWrite(REG_ADCRSTSTCT2, 4000);   delay(2);
    afeWrite(REG_ADCRSTENDCT2, 4003);  delay(2);
    afeWrite(REG_ADCRSTSTCT3, 6000);   delay(2);
    afeWrite(REG_ADCRSTENDCT3, 6003);  delay(2);
    afeWrite(REG_TIA_AMB_GAIN, 0x000003); delay(10);
    afeWrite(REG_LEDCNTRL, 0x012020);     delay(10);
    afeWrite(REG_CONTROL1, 0x000102);     delay(10);
}

//=== BAGIAN 1: FILTERING (EMA / High-pass / Low-pass) ===
float dcIr = 0, dcRed = 0;
bool dcInit = false;
const float DC_ALPHA_FAST = 0.99f;
const float DC_ALPHA_SLOW = 0.999f;

const float HP_ALPHA = 0.995f;
float hpPrevXIr = 0, hpPrevYIr = 0;
float hpPrevXRed = 0, hpPrevYRed = 0;
bool hpInit = false;

const float LP_ALPHA = 0.90f;
float lpYIr = 0, lpYRed = 0;
bool lpInit = false;

//=== BAGIAN 2: DETEKSI JARI ===
int fingerState = 0; // 0=IDLE, 1=CANDIDATE, 2=ACTIVE
unsigned long candidateStartMs = 0;
unsigned long lastBeatTimeMs = 0;
int candidateBeatCount = 0;

//--- tracker RAW cepat, dasar semua threshold di bawah ---
float dcFastIr = 0;
bool dcFastInit = false;
const float DC_DETECT_ALPHA = 0.99f; // time constant ~200ms

//--- baseline "tanpa jari" + ambang masuk CANDIDATE ---
float emptyBaselineIr = 0;
bool baselineCaptured = false;
unsigned long bootMs = 0;
const unsigned long BASELINE_CAPTURE_MS = 3000;
const float RAW_ON_FRAC = 0.85f; // raw < 85% baseline -> kandidat "ada jari"

//--- bukti "ada jari" harus BERTAHAN, bukan sesaat (anti klip-dibuka/noise) ---
int onEvidence = 0;
const int ON_EVIDENCE_NEEDED = 250; // ~0.5 detik @ ~500Hz
const int ON_EVIDENCE_MAX = 375;

//--- ambang "lepas" PERSONAL: dikalibrasi ke level org itu sendiri stlh ACTIVE ---
const float RAW_OFF_FRAC_FALLBACK = 0.90f; // dipakai sblm ada kalibrasi personal (msh CANDIDATE)
float confirmedOnLevel = 0;
bool confirmedOnLevelSet = false;
const float OFF_RECOVER_FRAC = 0.5f; // "lepas" jika raw pulih >=50% jarak menuju baseline
const float ON_LEVEL_ALPHA = 0.999f; // adaptasi lambat (~2 detik) selama ACTIVE

int offEvidence = 0;
const int OFF_EVIDENCE_NEEDED = 500; // ~1 detik bukti bersih @ ~500Hz
const int OFF_EVIDENCE_MAX = 750;

const unsigned long CANDIDATE_TIMEOUT_MS = 22000;
const unsigned long NO_PULSE_TIMEOUT_MS = 30000;
// Dikurangi dari 3 -> 2: makin sedikit detak yg dibutuhkan utk konfirmasi,
// makin kecil peluang kontak yg blm 100% mantap keburu terganggu sblm sempat
// terkonfirmasi (kasus jari org lain yg klipnya kurang pas).
const int BEATS_TO_CONFIRM = 2;
// SpO2 sengaja tetap nunggu SEDIKIT lebih banyak detak drpd BPM - BPM boleh
// cepat, tapi SpO2 baik jaga jarak ekstra demi akurasi (lihat BAGIAN 5).
const int SPO2_MIN_BEATS = 3;

//=== BAGIAN 3: WARM-UP (stabilitas DC) - HANYA menggerbang SpO2, bukan BPM ===
float dcSnapshotIr = 0, dcSnapshotRed = 0;
unsigned long lastSnapshotMs = 0;
int stableChecks = 0;
bool warmupDone = false;
const unsigned long SNAPSHOT_INTERVAL_MS = 1000;
const float STABILITY_THRESHOLD_PCT = 0.03f;
const int STABLE_CHECKS_NEEDED = 3;
const unsigned long MAX_WARMUP_MS = 20000;
const unsigned long MIN_WARMUP_MS = 3000;

//=== BAGIAN 4: DETEKSI DETAK JANTUNG (BPM) - mulai segera saat CANDIDATE ===
const unsigned long BEAT_DETECT_SETTLE_MS = 500; // jeda tenang stlh reset, tunggu transien filter

//--- baseline dinamis KHUSUS deteksi lintasan-nol, tahan drift DC lambat ---
float beatBaseline = 0;
bool beatBaselineInit = false;
const float BEAT_BASELINE_ALPHA = 0.995f;

bool bpWasNegative = true;
unsigned long lastBeatMs = 0;
float runningAmp = 0;
const float AMP_DECAY = 0.999f;
const unsigned long REFRACT_MS = 330;
const unsigned long MAX_INTERVAL_MS = 2000;

#define BPM_HIST_SIZE 7
float bpmHist[BPM_HIST_SIZE] = {0};
int bpmHistIdx = 0;
int bpmHistCount = 0;
float bpmEstimate = 0;

float avgBeatAmplitude = 0;
bool avgAmpInit = false;
const float AVG_AMP_ALPHA = 0.8f;

//=== BAGIAN 5: SATURASI OKSIGEN (SpO2) - menunggu warmupDone utk ketelitian ===
float peakIr = -1e6f, valleyIr = 1e6f;
float peakRed = -1e6f, valleyRed = 1e6f;

#define SPO2_HIST_SIZE 5
float spo2Hist[SPO2_HIST_SIZE] = {0};
int spo2HistIdx = 0;
int spo2HistCount = 0;
float spo2 = 0;
bool spo2Init = false;
const float SPO2_SMOOTH_ALPHA = 0.4f;

//=== VARIABEL OUTPUT/UMUM ===
unsigned long sampleCount = 0;
unsigned long startMs = 0;
const int DECIM_FACTOR = 20;
int decimCounter = 0;
float lastAcRed = 0, lastAcIr = 0;
float lastHpYRed = 0, lastHpYIr = 0;
float lastRatioR = 0;
float lastSpo2Raw = 0; // SpO2 sblm median+smoothing - telemetri pembanding
int lastRdy = LOW;

float medianOfN(float* arr, int count) {
    if (count <= 0) return 0;
    float tmp[BPM_HIST_SIZE];
    int n = (count < BPM_HIST_SIZE) ? count : BPM_HIST_SIZE;
    memcpy(tmp, arr, n * sizeof(float));
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (tmp[j] < tmp[i]) { float t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }
    return tmp[n / 2];
}

void resetBeatAndSpo2Only() {
    bpmHistIdx = 0; bpmHistCount = 0; bpmEstimate = 0;
    peakIr = -1e6f; valleyIr = 1e6f;
    peakRed = -1e6f; valleyRed = 1e6f;
    spo2HistIdx = 0; spo2HistCount = 0;
    spo2 = 0; spo2Init = false;
    candidateBeatCount = 0;
    lastBeatTimeMs = 0;
    beatBaselineInit = false;
}

void resetAlgorithmState() {
    dcInit = false;
    hpInit = false;
    lpInit = false;
    warmupDone = false;
    stableChecks = 0;
    startMs = millis();
    lastSnapshotMs = startMs;
    dcSnapshotIr = 0; dcSnapshotRed = 0;

    bpWasNegative = true;
    lastBeatMs = 0;
    runningAmp = 0;
    bpmHistIdx = 0; bpmHistCount = 0; bpmEstimate = 0;
    avgAmpInit = false; avgBeatAmplitude = 0;
    beatBaselineInit = false;

    peakIr = -1e6f; valleyIr = 1e6f;
    peakRed = -1e6f; valleyRed = 1e6f;
    spo2HistIdx = 0; spo2HistCount = 0;
    spo2 = 0; spo2Init = false;

    candidateBeatCount = 0;
    lastBeatTimeMs = 0;

    confirmedOnLevel = 0;
    confirmedOnLevelSet = false;
    offEvidence = 0;
    onEvidence = 0;
}

void setup() {
    Serial.begin(115200);

    // Tunggu terminal serial terhubung (maks 5 detik) - supaya baris log awal
    // tdk pernah "hilang" & tdk perlu tombol reset manual. Tetap lanjut
    // otomatis stlh timeout, supaya alat wearable ini tetap jalan normal
    // walau dipakai tanpa PC terhubung.
    unsigned long serialWaitMs = millis();
    while (!Serial && (millis() - serialWaitMs) < 5000) {
        // menunggu host membuka port serial
    }
    delay(300);

    pinMode(AFE_CS_PIN, OUTPUT);
    digitalWrite(AFE_CS_PIN, HIGH);
    pinMode(AFE_PDN_PIN, OUTPUT);
    pinMode(AFE_RDY_PIN, INPUT);
    pinMode(AFE_MISO_PIN, INPUT);
    SPI.setSCLK(AFE_SCLK_PIN);
    SPI.setMISO(AFE_MISO_PIN);
    SPI.setMOSI(AFE_MOSI_PIN);
    SPI.begin();
    afeFullConfig();

    Serial.println(F("[Sanity Check]"));
    verifyRegister("CONTROL2",     REG_CONTROL2, 0x020100);
    verifyRegister("PRPCOUNT",     REG_PRPCOUNT, 7999);
    verifyRegister("TIA_AMB_GAIN", REG_TIA_AMB_GAIN, 0x000003);
    verifyRegister("LEDCNTRL",     REG_LEDCNTRL, 0x012020);
    verifyRegister("CONTROL1",     REG_CONTROL1, 0x000102);
    Serial.println(F("PENTING: clip harus tertutup rapat TANPA jari 3 detik pertama"));
    Serial.println(F("(untuk kalibrasi baseline deteksi jari)."));
    Serial.println();

    startMs = millis();
    lastSnapshotMs = startMs;
    bootMs = millis();
}

void loop() {
    int rdyPin = digitalRead(AFE_RDY_PIN);
    bool sampleReady = (rdyPin == HIGH && lastRdy == LOW);
    lastRdy = rdyPin;
    if (!sampleReady) return;

    int32_t rawIr  = signExtend24(afeReadRetry(REG_LED2MALED2VAL));
    int32_t rawRed = signExtend24(afeReadRetry(REG_LED1MALED1VAL));
    sampleCount++;

    //--- BAGIAN 1: Filtering ---
    float dcAlpha = (sampleCount < 2000) ? DC_ALPHA_FAST : DC_ALPHA_SLOW;
    if (!dcInit) { dcIr = (float)rawIr; dcRed = (float)rawRed; dcInit = true; }
    dcIr  = dcAlpha * dcIr  + (1.0f - dcAlpha) * (float)rawIr;
    dcRed = dcAlpha * dcRed + (1.0f - dcAlpha) * (float)rawRed;

    if (!dcFastInit) { dcFastIr = (float)rawIr; dcFastInit = true; }
    dcFastIr = DC_DETECT_ALPHA * dcFastIr + (1.0f - DC_DETECT_ALPHA) * (float)rawIr;

    float acIr  = (float)rawIr  - dcIr;
    float acRed = (float)rawRed - dcRed;

    if (!hpInit) {
        hpPrevXIr = (float)rawIr; hpPrevXRed = (float)rawRed;
        hpPrevYIr = 0; hpPrevYRed = 0;
        hpInit = true;
    }
    float hpYIr  = HP_ALPHA * (hpPrevYIr  + (float)rawIr  - hpPrevXIr);
    float hpYRed = HP_ALPHA * (hpPrevYRed + (float)rawRed - hpPrevXRed);
    hpPrevXIr = (float)rawIr;  hpPrevYIr = hpYIr;
    hpPrevXRed = (float)rawRed; hpPrevYRed = hpYRed;

    if (!lpInit) { lpYIr = hpYIr; lpYRed = hpYRed; lpInit = true; }
    lpYIr  = LP_ALPHA * lpYIr  + (1.0f - LP_ALPHA) * hpYIr;
    lpYRed = LP_ALPHA * lpYRed + (1.0f - LP_ALPHA) * hpYRed;
    float bpIr  = lpYIr;
    float bpRed = lpYRed;

    unsigned long now = millis();

    //--- BAGIAN 2: baseline "tanpa jari" (sekali, di awal boot) ---
    if (!baselineCaptured && dcFastInit && (now - bootMs) > BASELINE_CAPTURE_MS) {
        emptyBaselineIr = dcFastIr;
        baselineCaptured = true;
        Serial.print(F("[Info] Baseline: raw_ir="));
        Serial.println(emptyBaselineIr, 0);
    }

    //--- BAGIAN 2: ambang masuk (tetap) & keluar (personal) ---
    float offThreshold = confirmedOnLevelSet
        ? confirmedOnLevel + (emptyBaselineIr - confirmedOnLevel) * OFF_RECOVER_FRAC
        : emptyBaselineIr * RAW_OFF_FRAC_FALLBACK;
    bool rawHighNow = baselineCaptured && (dcFastIr > offThreshold);
    bool rawLowNow  = baselineCaptured && (dcFastIr < emptyBaselineIr * RAW_ON_FRAC);

    // Penghitung bukti: naik lambat, turun cepat - toleran noise sesaat.
    if (rawLowNow)  onEvidence  = min(onEvidence + 1, ON_EVIDENCE_MAX);
    else            onEvidence  = max(onEvidence - 2, 0);
    if (rawHighNow) offEvidence = min(offEvidence + 1, OFF_EVIDENCE_MAX);
    else            offEvidence = max(offEvidence - 2, 0);
    bool confirmedOn  = onEvidence  >= ON_EVIDENCE_NEEDED;
    bool confirmedOff = offEvidence >= OFF_EVIDENCE_NEEDED;

    // Adaptasi lambat level "ada-jari" org ini selama ACTIVE.
    if (fingerState == 2 && confirmedOnLevelSet) {
        confirmedOnLevel = ON_LEVEL_ALPHA * confirmedOnLevel + (1.0f - ON_LEVEL_ALPHA) * dcFastIr;
    }

    //--- BAGIAN 3: Warm-up (menggerbang SpO2 saja) ---
    if (fingerState >= 1 && !warmupDone) {
        unsigned long elapsedSinceStart = now - startMs;
        if (now - lastSnapshotMs >= SNAPSHOT_INTERVAL_MS) {
            if (dcSnapshotIr != 0 && dcSnapshotRed != 0) {
                float diffIr  = fabsf(dcIr  - dcSnapshotIr)  / fabsf(dcSnapshotIr + 1);
                float diffRed = fabsf(dcRed - dcSnapshotRed) / fabsf(dcSnapshotRed + 1);
                if (diffIr < STABILITY_THRESHOLD_PCT && diffRed < STABILITY_THRESHOLD_PCT)
                    stableChecks++;
                else
                    stableChecks = 0;
            }
            dcSnapshotIr = dcIr;
            dcSnapshotRed = dcRed;
            lastSnapshotMs = now;
        }
        bool stableEnough = (stableChecks >= STABLE_CHECKS_NEEDED) && (elapsedSinceStart > MIN_WARMUP_MS);
        bool forcedByTimeout = (elapsedSinceStart > MAX_WARMUP_MS);
        if (stableEnough || forcedByTimeout) {
            warmupDone = true;
            Serial.print(F("[Info] DC stabil ("));
            Serial.print(elapsedSinceStart / 1000.0f, 1);
            Serial.println(F("s) -> SpO2 aktif."));
        }
    }

    //--- BAGIAN 4 & 5: BPM (segera stlh settle) + SpO2 (tunggu warmupDone) ---
    if (fingerState >= 1 && (now - candidateStartMs) > BEAT_DETECT_SETTLE_MS) {
        // Baseline dinamis khusus lintasan-nol - buang sisa drift DC lambat.
        if (!beatBaselineInit) { beatBaseline = bpIr; beatBaselineInit = true; }
        beatBaseline = BEAT_BASELINE_ALPHA * beatBaseline + (1.0f - BEAT_BASELINE_ALPHA) * bpIr;
        float bpBeat = bpIr - beatBaseline;

        runningAmp *= AMP_DECAY;
        float absBp = fabsf(bpBeat);
        if (absBp > runningAmp) runningAmp = absBp;
        float threshold = runningAmp * 0.25f;
        // Ambang re-arm negatif dilonggarkan (0.5 -> 0.35x) - beberapa org
        // punya bentuk gelombang yg tdk turun terlalu dalam tiap detak;
        // ambang lama beresiko melewatkan detak selang-seling (bpm jd
        // separuh nilai asli, mis. 35bpm padahal sebenarnya ~70bpm).
        float negThreshold = -threshold * 0.35f;

        if (bpWasNegative) {
            if (bpBeat > threshold) {
                unsigned long elapsed = now - lastBeatMs;
                if (elapsed > REFRACT_MS && lastBeatMs > 0 && elapsed < MAX_INTERVAL_MS) {
                    float instBpm = 60000.0f / (float)elapsed;
                    float thisBeatAmp = absBp;
                    bool artifact = avgAmpInit && (thisBeatAmp > 2.5f * avgBeatAmplitude);

                    if (instBpm > 35.0f && instBpm < 200.0f && !artifact) {
                        lastBeatTimeMs = now;
                        candidateBeatCount++;

                        // Estimasi waktu DIHITUNG dari detak nyata, bukan tebakan.
                        if (fingerState == 1 && candidateBeatCount == 1) {
                            float etaSec = (BEATS_TO_CONFIRM - 1) * (60.0f / instBpm);
                            Serial.print(F("[Info] Detak awal ~"));
                            Serial.print(instBpm, 0);
                            Serial.print(F("bpm, BPM aktif ~"));
                            Serial.print(etaSec, 1);
                            Serial.println(F("s lagi."));
                        }

                        bpmHist[bpmHistIdx] = instBpm;
                        bpmHistIdx = (bpmHistIdx + 1) % BPM_HIST_SIZE;
                        if (bpmHistCount < BPM_HIST_SIZE) bpmHistCount++;
                        bpmEstimate = medianOfN(bpmHist, bpmHistCount);

                        if (!avgAmpInit) { avgBeatAmplitude = thisBeatAmp; avgAmpInit = true; }
                        else avgBeatAmplitude = AVG_AMP_ALPHA * avgBeatAmplitude + (1 - AVG_AMP_ALPHA) * thisBeatAmp;

                        // SpO2: hanya dihitung kalau detak cukup DAN DC sudah stabil.
                        if (candidateBeatCount >= SPO2_MIN_BEATS && warmupDone) {
                            float acIrAmp  = peakIr  - valleyIr;
                            float acRedAmp = peakRed - valleyRed;
                            float absDcIr  = fabsf(dcIr);
                            float absDcRed = fabsf(dcRed);

                            float piIr  = (absDcIr  > 0) ? acIrAmp  / absDcIr  : 0;
                            float piRed = (absDcRed > 0) ? acRedAmp / absDcRed : 0;

                            // Gerbang perfusion-index: sinyal terlalu lemah -> freeze, bukan ngaco.
                            if (absDcIr > 1000.0f && absDcRed > 1000.0f &&
                                acIrAmp > 5.0f && acRedAmp > 5.0f &&
                                piIr > 0.003f && piRed > 0.003f) {
                                float ratioR = piIr / piRed;
                                lastRatioR = ratioR;
                                if (ratioR > 0.2f && ratioR < 1.5f) {
                                    // Kalibrasi 2-titik: R=0.62->98, R=1.0->85
                                    float spo2Raw = 119.0f - 34.0f * ratioR;
                                    lastSpo2Raw = spo2Raw;
                                    spo2Raw = constrain(spo2Raw, 70.0f, 100.0f);
                                    spo2Hist[spo2HistIdx] = spo2Raw;
                                    spo2HistIdx = (spo2HistIdx + 1) % SPO2_HIST_SIZE;
                                    if (spo2HistCount < SPO2_HIST_SIZE) spo2HistCount++;
                                    float spo2Med = medianOfN(spo2Hist, spo2HistCount);
                                    if (!spo2Init) { spo2 = spo2Med; spo2Init = true; }
                                    else spo2 = SPO2_SMOOTH_ALPHA * spo2 + (1.0f - SPO2_SMOOTH_ALPHA) * spo2Med;
                                }
                            }
                        }
                        peakIr = bpIr; valleyIr = bpIr;
                        peakRed = bpRed; valleyRed = bpRed;
                    }
                }
                if (elapsed > REFRACT_MS || lastBeatMs == 0) lastBeatMs = now;
                bpWasNegative = false;
            }
        } else {
            if (bpBeat < negThreshold) bpWasNegative = true;
        }

        if (bpIr > peakIr) peakIr = bpIr;
        if (bpIr < valleyIr) valleyIr = bpIr;
        if (bpRed > peakRed) peakRed = bpRed;
        if (bpRed < valleyRed) valleyRed = bpRed;
    }

    //--- BAGIAN 2 (lanjutan): State Machine Deteksi Jari ---
    switch (fingerState) {
        case 0: // IDLE
            if (baselineCaptured && confirmedOn && sampleCount > 2000) {
                fingerState = 1;
                candidateStartMs = now;
                resetAlgorithmState();
                // Personalisasi ambang "lepas" LANGSUNG di sini (bukan nunggu BPM
                // terkonfirmasi) - kontak yg msh menyetel diri tdk keburu kena
                // ambang generik yg lebih ketat.
                confirmedOnLevel = dcFastIr;
                confirmedOnLevelSet = true;
                Serial.print(F("[Info] Jari terdeteksi (kedalaman "));
                Serial.print((dcFastIr / emptyBaselineIr) * 100.0f, 0);
                Serial.println(F("%)."));
            }
            break;

        case 1: // CANDIDATE
            if (confirmedOff) {
                fingerState = 0;
                Serial.println(F("[Info] Sinyal hilang -> idle."));
            } else if (candidateBeatCount >= BEATS_TO_CONFIRM) {
                fingerState = 2;
                Serial.print(F("[Info] BPM aktif: "));
                Serial.print(bpmEstimate, 0);
                Serial.println(F("bpm (SpO2 ~10-15s lagi)."));
            } else if (now - candidateStartMs > CANDIDATE_TIMEOUT_MS) {
                fingerState = 0;
                Serial.println(F("[Info] Timeout -> idle."));
            }
            break;

        case 2: // ACTIVE
            if (confirmedOff) {
                fingerState = 0;
                resetAlgorithmState();
                Serial.println(F("[Info] Jari lepas."));
            } else if (lastBeatTimeMs > 0 && (now - lastBeatTimeMs) > NO_PULSE_TIMEOUT_MS) {
                fingerState = 1;
                candidateStartMs = now;
                resetBeatAndSpo2Only();
                Serial.println(F("[Info] Detak hilang -> ulang deteksi."));
            }
            break;
    }

    lastAcRed = acRed;
    lastAcIr  = acIr;
    lastHpYRed = hpYRed;
    lastHpYIr  = hpYIr;

    //--- OUTPUT (di-decimasi ~25 Hz) ---
    decimCounter++;
    if (decimCounter >= DECIM_FACTOR) {
        decimCounter = 0;
        bool measuring = (fingerState >= 1);
        bool bpmReady  = (measuring && bpmHistCount >= BEATS_TO_CONFIRM);
        bool active = bpmReady;
        float bpmOut  = bpmReady ? bpmEstimate : 0;
        float spo2Out = (bpmReady && spo2Init) ? spo2 : 0;

        Serial.print(F(">finger_on:"));   Serial.println(active ? 1 : 0);
        Serial.print(F(">bpm:"));         Serial.println(bpmOut, 0);
        Serial.print(F(">spo2:"));        Serial.println(spo2Out, 1);
        Serial.print(F(">spo2_raw:"));    Serial.println(bpmReady ? lastSpo2Raw : 0, 1);
        Serial.print(F(">ac_red:"));      Serial.println(lastAcRed, 1);
        Serial.print(F(">ac_ir:"));       Serial.println(lastAcIr, 1);
        Serial.print(F(">ac_red_filt:")); Serial.println(lastHpYRed, 1);
        Serial.print(F(">ac_ir_filt:"));  Serial.println(lastHpYIr, 1);
        Serial.print(F(">dc_red:"));      Serial.println(dcRed, 1);
        Serial.print(F(">dc_ir:"));       Serial.println(dcIr, 1);
        Serial.print(F(">raw_red:"));     Serial.println(rawRed);
        Serial.print(F(">raw_ir:"));      Serial.println(rawIr);
    }
}
