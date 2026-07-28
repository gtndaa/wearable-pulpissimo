/**
 * ntp_sync.c - NTP Time Synchronization for DS1307 RTC
 *
 * 3 core functions:
 *   1. ntp_network_connect()  - Open connection to NTP server
 *   2. ntp_fetch_timestamp()  - Fetch timestamp from server
 *   3. time_sync_to_rtc()     - Convert + write to RTC
 *
 * Switch: -DTEST_DI_LAPTOP (Wi-Fi) / no flag (Ethernet board)
 */

#include "ntp_sync.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef TEST_DI_LAPTOP
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
#else
    // TODO: Board ethernet headers (e.g. lwip/sockets.h)
#endif

static int ntp_sockfd = -1;

// ===================================================
// FUNCTION 1: Open connection to NTP server
// ===================================================
static int ntp_network_connect(void)
{
#ifdef TEST_DI_LAPTOP
    struct sockaddr_in serv_addr;
    struct hostent *server;

    ntp_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ntp_sockfd < 0) {
        printf("[NTP ERR -1] Failed to open socket\n");
        return NTP_ERR_SOCKET;
    }

    server = gethostbyname(NTP_SERVER);
    if (server == NULL) {
        printf("[NTP ERR -2] DNS resolve failed: %s\n", NTP_SERVER);
        close(ntp_sockfd);
        ntp_sockfd = -1;
        return NTP_ERR_DNS;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(NTP_PORT);

    // 5s timeout to prevent hang
    struct timeval tv = { .tv_sec = NTP_TIMEOUT_SEC, .tv_usec = 0 };
    setsockopt(ntp_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(ntp_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[NTP ERR -3] Connection failed: %s\n", NTP_SERVER);
        close(ntp_sockfd);
        ntp_sockfd = -1;
        return NTP_ERR_CONNECT;
    }

    printf("[NTP] Connected to %s\n", NTP_SERVER);
    return NTP_OK;

#else
    // TODO: Board ethernet (lwip_socket, lwip_connect)
    printf("[NTP ERR -1] Board ethernet not implemented\n");
    return NTP_ERR_SOCKET;
#endif
}

// ===================================================
// FUNCTION 2: Fetch NTP timestamp from server
// ===================================================
static uint32_t ntp_fetch_timestamp(int *err)
{
#ifdef TEST_DI_LAPTOP
    unsigned char packet[NTP_PACKET_SIZE];
    int n;

    // NTP Client request: 0x1B
    memset(packet, 0, NTP_PACKET_SIZE);
    packet[0] = 0x1B;

    n = write(ntp_sockfd, packet, NTP_PACKET_SIZE);
    if (n < 0) {
        printf("[NTP ERR -4] Failed to send packet\n");
        *err = NTP_ERR_SEND;
        return 0;
    }

    n = read(ntp_sockfd, packet, NTP_PACKET_SIZE);
    if (n < 0) {
        printf("[NTP ERR -5] Timeout: no response in %ds\n", NTP_TIMEOUT_SEC);
        *err = NTP_ERR_TIMEOUT;
        return 0;
    }

    close(ntp_sockfd);
    ntp_sockfd = -1;

    // Extract Transmit Timestamp (bytes 40-43)
    uint32_t ts = (uint32_t)packet[40] << 24 |
                  (uint32_t)packet[41] << 16 |
                  (uint32_t)packet[42] << 8  |
                  (uint32_t)packet[43];

    printf("[NTP] Timestamp: %u\n", ts);
    *err = NTP_OK;
    return ts;

#else
    printf("[NTP ERR -4] Board ethernet not implemented\n");
    *err = NTP_ERR_SEND;
    return 0;
#endif
}

// ===================================================
// FUNCTION 3: Convert + set RTC (NEVER CHANGES)
// ===================================================
static int time_sync_to_rtc(i2c_t *rtc_handle, uint32_t ntp_ts)
{
    #define NTP_UNIX_DELTA 2208988800UL

    if (ntp_ts < 3913056000UL) {
        printf("[NTP ERR -7] Invalid timestamp\n");
        return NTP_ERR_INVALID;
    }

    time_t unix_time = (time_t)(ntp_ts - NTP_UNIX_DELTA);
    unix_time += NTP_UTC_OFFSET_WIB;  // UTC → WIB (+7h)

    struct tm *t = gmtime(&unix_time);

    rtc_time_t rtc = {
        .year    = t->tm_year - 100,
        .month   = t->tm_mon + 1,
        .date    = t->tm_mday,
        .day     = t->tm_wday + 1,
        .hours   = t->tm_hour,
        .minutes = t->tm_min,
        .seconds = t->tm_sec
    };

    printf("[NTP] Time (WIB): %02d/%02d/20%02d %02d:%02d:%02d\n",
           rtc.date, rtc.month, rtc.year,
           rtc.hours, rtc.minutes, rtc.seconds);

#ifdef TEST_DI_LAPTOP
    printf("[NTP] rtc_set_time() simulated (no hardware)\n");
    (void)rtc_handle;
    return NTP_OK;
#else
    int rc = rtc_set_time(rtc_handle, &rtc);
    if (rc != RTC_OK) {
        printf("[NTP ERR -6] RTC write failed (code: %d)\n", rc);
        return NTP_ERR_RTC_SET;
    }
    printf("[NTP] RTC synchronized!\n");
    return NTP_OK;
#endif
}

// ===================================================
// PUBLIC: ntp_sync_now() - Sync once from internet
// ===================================================
int ntp_sync_now(i2c_t *rtc_handle)
{
    int rc;

    printf("\n=== NTP SYNC START ===\n");

    rc = ntp_network_connect();
    if (rc != NTP_OK) return rc;

    int fetch_err = NTP_OK;
    uint32_t ts = ntp_fetch_timestamp(&fetch_err);
    if (fetch_err != NTP_OK) return fetch_err;

    rc = time_sync_to_rtc(rtc_handle, ts);

    printf("=== NTP SYNC %s ===\n\n", rc == NTP_OK ? "OK" : "FAILED");
    return rc;
}

// ===================================================
// PUBLIC: ntp_manual_set() - Set RTC without internet
// ===================================================
int ntp_manual_set(i2c_t *rtc_handle, rtc_time_t *waktu)
{
    if (waktu == NULL) return NTP_ERR_INVALID;

    printf("[NTP MANUAL] Set: %02d/%02d/20%02d %02d:%02d:%02d\n",
           waktu->date, waktu->month, waktu->year,
           waktu->hours, waktu->minutes, waktu->seconds);

#ifdef TEST_DI_LAPTOP
    printf("[NTP] rtc_set_time() simulated\n");
    (void)rtc_handle;
    return NTP_OK;
#else
    return rtc_set_time(rtc_handle, waktu);
#endif
}
