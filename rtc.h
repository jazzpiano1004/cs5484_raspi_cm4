#ifndef RTC_H
#define RTC_H

/*
 * Include for Real-time Clock
 *
 */
#include <time.h>  // temporary RTC right now. It should be the actual RTC chip.
#include <stdio.h>
#include <string.h>

int rtc_getTime(char *strTime);
int rtc_setTime();

#endif
