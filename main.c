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
    
    ret = page_select(0, 0);
    printf("Setpage : ret=%d\r\n", ret);

    uint32_t buf;
    while(1)
    {
	ret = reg_write(0x500000, 0, 0);
        ret = reg_read(&buf, 0, 0);
        printf("read : ret=%d, read data=%x\r\n", ret, buf);
        delay(100);	
	ret = reg_write(0x400000, 0, 0);
        ret = reg_read(&buf, 0, 0);
        printf("read : ret=%d, read data=%x\r\n", ret, buf);
        delay(100);	
    }


    return 0;
}

