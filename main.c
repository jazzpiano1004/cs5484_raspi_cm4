#include "cs5484_wiringpi.h"
#include <errno.h>

#define BUS_SPEED 		1000000
#define SPI_MODE_CS5484		3

int main()
{   
    // initialize wiring-Pi
    wiringPiSetup();

    // intialize SPI interface
    pinMode(CS_PIN, OUTPUT);
    pullUpDnControl(CS_PIN, PUD_UP);
    digitalWrite(CS_PIN, 1);
    int ret;
    ret = wiringPiSPISetupMode(SPI_CHANNEL, BUS_SPEED, SPI_MODE_CS5484);
    printf("SPI init result : %d\r\n", ret);

    ret = reset(0);
    printf("Reset CS5484 : ret=%d\r\n", ret);
    delay(5000);
    
    uint32_t buf;
    uint32_t i;
    uint32_t v;
    uint32_t p;

    while(1)
    {  
	ret = start_conversion(CONVERSION_TYPE_SINGLE, 0, 10000);
	printf("start conversion, ret : %d\r\n", ret);
        v = get_voltage_rms(ANALOG_INPUT_CH1, 0);
        i = get_current_rms(ANALOG_INPUT_CH1, 0);
        p = get_power_avg(ANALOG_INPUT_CH1, 0);
        
	printf("i, v, p :\t%d\t%d\t%d\r\n", i, v, p);
    }


    return 0;
}

