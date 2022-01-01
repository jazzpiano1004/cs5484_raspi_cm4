#ifndef RTC_H
#define RTC_H

/*
 * Include for Real-time Clock
 *
 */
#include <time.h>  // temporary RTC right now. It should be the actual RTC chip.
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define RTC_CLOCKS_PER_SEC   CLOCKS_PER_SEC

/* 
 * Struct for msec tick 
 * Used for task scheduling in energy measurement
 */
typedef struct
{
    uint64_t tick_start;
    uint64_t tick_end;
    struct timespec ts;

}rtc_tick_t;

int rtc_getTime(char *strTime);
int rtc_setTime();

int rtc_tickStart(rtc_tick_t *s_tick);
int rtc_tickEnd(rtc_tick_t *s_tick);
double rtc_getPerformTime(rtc_tick_t *s_tick);

#endif
