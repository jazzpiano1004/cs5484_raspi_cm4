#include "rtc.h"



int rtc_getTime(char *strTime)
{
    time_t timeStamp;
    char str[30];

    time(&timeStamp);  // get current time

    //str = ctime(&timeStamp);     // use calender format
    // copy string buf into returned string
    //strcpy(strTime, str);
    //
    sprintf(strTime, "%ld", timeStamp); // use Unix second format
    
    return 0; //if success
}

int rtc_setTime()
{    
    return 0; //if success
}

int rtc_tickStart(rtc_tick_t *s_tick)
{
    clock_gettime(CLOCK_MONOTONIC, &(s_tick->ts));
    s_tick->tick_start = (uint64_t)s_tick->ts.tv_sec * 1000000000 + (uint64_t)s_tick->ts.tv_nsec;
    return 0;
}

int rtc_tickEnd(rtc_tick_t *s_tick)
{
    clock_gettime(CLOCK_MONOTONIC, &(s_tick->ts));
    s_tick->tick_end = (uint64_t)s_tick->ts.tv_sec * 1000000000 + (uint64_t)s_tick->ts.tv_nsec;
    return 0;
}

double rtc_getPerformTime(rtc_tick_t *s_tick)
{
    double time_perform;
    time_perform = (double) (s_tick->tick_end - s_tick->tick_start) / 1000000000;

    return time_perform;
}
