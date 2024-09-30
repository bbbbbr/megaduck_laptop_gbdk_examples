#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>

#include <megaduck_laptop_io.h>

#include "megaduck_rtc.h"

// These BCD conversions are for simplicity, in
// actual programs for performance it may be
// optimal to use the Game Boy / GBDK BCD features.
uint8_t bcd_to_u8(uint8_t i)
{
    return (i & 0xFu) + ((i >> 4) * 10u);
}

uint8_t u8_to_bcd(uint8_t i)
{
    return (i % 10u) + ((i / 10u) << 4);
}


uint16_t megaduck_rtc_year;
uint8_t  megaduck_rtc_mon;
uint8_t  megaduck_rtc_day;
uint8_t  megaduck_rtc_weekday;

uint8_t  megaduck_rtc_ampm;
uint8_t  megaduck_rtc_hour;
uint8_t  megaduck_rtc_min;
uint8_t  megaduck_rtc_sec;


// Get RTC command reply (Peripheral -> Duck)
//     All values are in BCD format
//         Ex: Month = December = 12th month = 0x12 (NOT 0x0C)
// [0]: Length of reply (always 4)
// [1..8] RTC Data
// [9]: checksum
//
// RTC Data:
//    if (tm.tm_year > (2000 - 1900))
//        [1] = int_to_bcd(tm.tm_year - (2000 - 1900));
//    else
//        [1] = int_to_bcd(tm.tm_year); // Years in BCD since 1900 (tm_year is already in since 1900 format)
//    [2] = int_to_bcd(tm.tm_mon + 1);
//    [3] = int_to_bcd(tm.tm_mday);
//    [4] = int_to_bcd(tm.tm_wday); // DOW
//
//    [5] = int_to_bcd( (tm.tm_hour < 12) ? 0 : 1 ); // AMPM
//    [6] = int_to_bcd(tm.tm_hour % 12);
//    [7] = int_to_bcd(tm.tm_min);
//    [8] = int_to_bcd(tm.tm_sec);

// Power-on defaults for the spanish laptop (oddly don't match system ROM hardcoded defaults)
void megaduck_laptop_set_default_rtc_values(void) {

        megaduck_rtc_year    = 1993u;
        megaduck_rtc_mon     = 6u; // June
        megaduck_rtc_day     = 1u; // 1st
        megaduck_rtc_weekday = 2u; // Tuesday

        megaduck_rtc_ampm    = 0u; // AM
        megaduck_rtc_hour    = 0u;
        megaduck_rtc_min     = 0u;
        megaduck_rtc_sec     = 0u;
}

// Send RTC data and handle the response
//
// Returns success or failure
bool megaduck_send_rtc(void) {


// Rewrite sending to just set all value in buffer and precalc checksum/etc with a wrapper function
// Then change the actual send code to just a single big loop, ay easier to read

    // For this example set the power-on defaults for the spanish laptop
    megaduck_laptop_set_default_rtc_values();

    uint8_t year_to_send;
    // Calculate as number of years since either 1900 or 2000
    // Strip year off, handle
    if (megaduck_rtc_year < 2000u) year_to_send = megaduck_rtc_year - 1900u;
    else                           year_to_send = megaduck_rtc_year - 2000u;

    megaduck_serial_tx_buf[0] = u8_to_bcd(year_to_send);
    megaduck_serial_tx_buf[1] = u8_to_bcd(megaduck_rtc_mon);
    megaduck_serial_tx_buf[2] = u8_to_bcd(megaduck_rtc_day);
    megaduck_serial_tx_buf[3] = u8_to_bcd(megaduck_rtc_weekday);

    megaduck_serial_tx_buf[4] = megaduck_rtc_ampm;
    megaduck_serial_tx_buf[5] = megaduck_rtc_hour;
    megaduck_serial_tx_buf[6] = megaduck_rtc_min;
    megaduck_serial_tx_buf[7] = megaduck_rtc_sec;

    megaduck_serial_tx_buf_len = RTC_SEND_LEN;

    if (serial_io_send_command_and_buffer(SYS_CMD_RTC_SET_DATE_AND_TIME)) {
        return true;
    } else {
        return false;
    }
}



// Request RTC data and handle the response
//
// Returns success or failure, raw rtc data in BCD format is loaded into vars
bool megaduck_poll_rtc(void) {

    if (serial_io_send_command_and_receive_buffer(SYS_CMD_RTC_GET_DATE_AND_TIME)) {
        if (megaduck_serial_rx_buf_len == RTC_REPLY_LEN) {
            megaduck_rtc_year     = megaduck_serial_rx_buf[0];
            megaduck_rtc_mon      = megaduck_serial_rx_buf[1];
            megaduck_rtc_day      = megaduck_serial_rx_buf[2];
            megaduck_rtc_weekday  = megaduck_serial_rx_buf[3];

            megaduck_rtc_ampm     = megaduck_serial_rx_buf[4];
            megaduck_rtc_hour     = megaduck_serial_rx_buf[5];
            megaduck_rtc_min      = megaduck_serial_rx_buf[6];
            megaduck_rtc_sec      = megaduck_serial_rx_buf[7];
            return true;
        }
    }
    return false;
}


// Translates raw RTC data in BCD format to decimal
//
// The 1992 wraparound is optional, but it's how
// the Super Junior SameDuck emulation does it in order
// to remain compatible with the MegaDuck Laptop System ROM
// which defaults to 1993 on startup (so BCD 93 for year)
// and supports years as early as 1992 (BCD 92) within an
// 8 bit bcd number (max being 99 years).
void megaduck_keyboard_process_rtc(void) {

    megaduck_rtc_year = bcd_to_u8(megaduck_rtc_year);
    if (megaduck_rtc_year >= 92) megaduck_rtc_year += 1900u;
    else                         megaduck_rtc_year += 2000u;

    megaduck_rtc_mon     = bcd_to_u8(megaduck_rtc_mon);
    megaduck_rtc_day     = bcd_to_u8(megaduck_rtc_day);
    megaduck_rtc_mon     = bcd_to_u8(megaduck_rtc_mon);
    megaduck_rtc_weekday = bcd_to_u8(megaduck_rtc_weekday);

    megaduck_rtc_ampm = bcd_to_u8(megaduck_rtc_ampm);
    megaduck_rtc_hour = bcd_to_u8(megaduck_rtc_hour);
    megaduck_rtc_min = bcd_to_u8(megaduck_rtc_min);
    megaduck_rtc_sec = bcd_to_u8(megaduck_rtc_sec);
}
