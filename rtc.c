#include "rtc.h"



int rtc_getTime(char *strTime){
    time_t timeStamp;
    char *str;

    time(&timeStamp);
    str = ctime(&timeStamp);
    strcpy(strTime, str);

    return 0; //if success
}

int rtc_setTime(){
    
    return 0; //if success
}
