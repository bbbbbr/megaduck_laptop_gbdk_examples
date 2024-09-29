#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef _MEGADUCK_RTC_H
#define _MEGADUCK_RTC_H


// RTC data
extern uint16_t megaduck_rtc_year;
extern uint8_t  megaduck_rtc_mon;
extern uint8_t  megaduck_rtc_day;
extern uint8_t  megaduck_rtc_weekday;

extern uint8_t  megaduck_rtc_ampm;
extern uint8_t  megaduck_rtc_hour;
extern uint8_t  megaduck_rtc_min;
extern uint8_t  megaduck_rtc_sec;


bool megaduck_send_rtc(void);
bool megaduck_poll_rtc(void);
void megaduck_keyboard_process_rtc(void);


#endif // _MEGADUCK_RTC_H
