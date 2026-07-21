#include "pulp.h"
#include "rtc.h"


#define DS1307_ADDR        0x68  // 7-bit address DS1307 (datasheet: binary 1101000, write=0xD0, read=0xD1)
#define DS1307_REG_SECONDS 0x00 

// Decimal to BCD
uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// BCD to Decimal
uint8_t bcd_to_dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

int rtc_init(i2c_dev_t *dev_conf, i2c_t **i2c_handle) {
    if (dev_conf == NULL || i2c_handle == NULL) {
        return RTC_ERR_NULL_PTR;
    }

    i2c_dev_init(dev_conf);
    dev_conf->id = 0; 
    dev_conf->cs = DS1307_ADDR << 1; // Shift ke 8-bit karena driver I2C PULP butuh format 8-bit
    dev_conf->max_baudrate = 100000; 

    *i2c_handle = i2c_open(dev_conf);
    if (*i2c_handle == NULL) {
        return RTC_ERR_I2C_OPEN;
    }
    return RTC_OK;
}

int rtc_set_time(i2c_t *i2c_handle, rtc_time_t *time) {
    if (i2c_handle == NULL || time == NULL) {
        return RTC_ERR_NULL_PTR;
    }

    // Validasi range input sesuai datasheet DS1307
    if (time->seconds > 59 || time->minutes > 59 || time->hours > 23 ||
        time->day < 1 || time->day > 7 ||
        time->date < 1 || time->date > 31 ||
        time->month < 1 || time->month > 12 ||
        time->year > 99) {
        return RTC_ERR_VALIDATION;
    }

    // FIX: Buffer hanya berisi register pointer + 7 byte data (total 8 byte)
    // Address TIDAK dimasukkan ke buffer — library i2c.c kirim address otomatis via dev->cs
    unsigned char tx_data[8];

    tx_data[0] = DS1307_REG_SECONDS;  // Register pointer: mulai dari 0x00

    // REF DATASHEET: Register 00H (Seconds), Bit ke-7 adalah CH (Clock Halt).
    // Jika CH = 1, osilator mati. Jika CH = 0, osilator jalan.
    // Operator & 0x7F (01111111) digunakan untuk memaksa Bit-7 menjadi 0, 
    // sehingga jam dipastikan mulai berdetak setelah waktu diset.
    tx_data[1] = dec_to_bcd(time->seconds) & 0x7F; 
    
    tx_data[2] = dec_to_bcd(time->minutes);
    
    // REF DATASHEET: Register 02H (Hours), Bit ke-6 adalah flag 12/24 Mode.
    // 0 = Mode 24 jam, 1 = Mode 12 jam.
    // Operator & 0x3F (00111111) digunakan untuk memaksa Bit-6 dan Bit-7 menjadi 0, 
    // untuk memastikan sensor selalu dikunci pada format 24jam
    tx_data[3] = dec_to_bcd(time->hours) & 0x3F;
    
    tx_data[4] = dec_to_bcd(time->day);
    tx_data[5] = dec_to_bcd(time->date);
    tx_data[6] = dec_to_bcd(time->month);
    tx_data[7] = dec_to_bcd(time->year);

    // Kirim data ke DS1307 lewat I2C
    // Library akan otomatis: [START][addr_write][tx_data[0..7]][STOP]
    if (i2c_write(i2c_handle, tx_data, 8, 1) != 0) {
        return RTC_ERR_I2C_WRITE_SET;
    }
    return RTC_OK;
}

int rtc_get_time(i2c_t *i2c_handle, rtc_time_t *time) {
    if (i2c_handle == NULL || time == NULL) {
        return RTC_ERR_NULL_PTR;
    }

    unsigned char tx_data[1];
    unsigned char rx_data[7];

    // Tahap 1: Set pointer register ke 0x00 (Register Seconds)
    // Library akan otomatis: [START][addr_write][0x00]  (tanpa STOP → repeated start)
    // FIX: Hanya kirim 1 byte (register pointer), BUKAN address + pointer
    tx_data[0] = DS1307_REG_SECONDS; 
    if (i2c_write(i2c_handle, tx_data, 1, 0) != 0) {
        return RTC_ERR_I2C_WRITE_PTR;
    }

    // Tahap 2: Burst read 7 byte dari register 0x00-0x06
    // Library i2c_read() akan otomatis: [START][addr_read][RD_ACK x6][RD_NACK][STOP]
    //
    // FIX: i2c_read() return value convention:
    //   - Sukses  = return jumlah byte yang dibaca (7)
    //   - Timeout = return 0
    // Parameter ke-4 'pending': 0 = kirim STOP setelah read (transaksi selesai)
    int bytes_read = i2c_read(i2c_handle, rx_data, 7, 0);
    if (bytes_read != 7) {
        return RTC_ERR_I2C_READ;
    }

    // Tahap 3: Konversi hasil bacaan BCD ke Desimal
    // Potong bit CH (Clock Halt) di register detik (Bit ke-7) 
    time->seconds = bcd_to_dec(rx_data[0] & 0x7F); 
    
    time->minutes = bcd_to_dec(rx_data[1]);
    
    // Potong bit 12/24 mode (Bit 6) di register jam
    time->hours   = bcd_to_dec(rx_data[2] & 0x3F); 
    
    time->day     = bcd_to_dec(rx_data[3]);
    time->date    = bcd_to_dec(rx_data[4]);
    time->month   = bcd_to_dec(rx_data[5]);
    time->year    = bcd_to_dec(rx_data[6]);

    return RTC_OK;
}

void day_tracker_init(day_tracker_t *dt)
{
    dt->last_date  = 0;
    dt->last_month = 0;
    dt->last_year  = 0;
    dt->valid      = 0;
}

int day_tracker_check_rollover(day_tracker_t *dt, i2c_t *i2c_handle)
{
    if (dt == NULL || i2c_handle == NULL) return RTC_ERR_NULL_PTR;

    rtc_time_t t;
    int ret = rtc_get_time(i2c_handle, &t);
    if (ret != RTC_OK) {
        return ret; // failed reading time from RTC
    }

    if (!dt->valid) {
        dt->last_date  = t.date;
        dt->last_month = t.month;
        dt->last_year  = t.year;
        dt->valid      = 1;
        return 0;
    }

    if (t.date != dt->last_date || t.month != dt->last_month || t.year != dt->last_year) {
        dt->last_date  = t.date;
        dt->last_month = t.month;
        dt->last_year  = t.year;
        return 1; // day changed
    }

    return 0;
}
