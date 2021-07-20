#include "cs5484_wiringpi.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

volatile int keepRunning = 1;
volatile int wiringpi_setup = 0;
void intHandler(int dummy){
    keepRunning = 0;
    printf("keyboard interrupt, send exit program...\n");

    /*
     * Disable all GPIOs before exist program
     *
     */
    if (wiringpi_setup){
        printf("Turn off GPIO of SPI before exit\n");
	pullUpDnControl(CS_PIN, PUD_OFF);
	digitalWrite(CS_PIN, 0);
    }
}

int main()
{  
    // signal handler initialize (for detect Ctrl + C exist)
    signal(SIGINT, intHandler);

    /*
     *  initialize wiring-Pi and SPI interface
     *
     */
    int ret; // for return status of function call
    wiringPiSetupGpio();
    wiringpi_setup = 1;
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, 0);
    delay(1000);
    pullUpDnControl(CS_PIN, PUD_UP);
    digitalWrite(CS_PIN, 1);
    ret = wiringPiSPISetupMode(SPI_CHANNEL, BUS_SPEED, SPI_MODE_CS5484);
    printf("SPI init result : %d\r\n", ret);
    delay(1000);

    /*
     * CS5484 : Reset CMD
     *
     */
    ret = reset(CHECKSUM_DISABLE);
    printf("Reset CS5484 : ret=%d\r\n", ret);
    delay(5000);
    
    ret = page_select(0, CHECKSUM_DISABLE);
    printf("change page, ret=%d\n", ret);
    uint32_t data_buf;

    /*
     * Read checksum enable and reconfig at SerialCtrl
     * 
     */
    /*
    page_select(0, CHECKSUM_DISABLE);
    int set_csum=0;
    while(!set_csum){
        ret = reg_read(&data_buf, 7, CHECKSUM_DISABLE);
        printf("read SerialCtrl, default val = 0x02004D, read value = %x, ret=%d\n", data_buf, ret);
        delay(1);
        
	if(data_buf == 0x02004D){
            ret = reg_write(0x00004D, 7, CHECKSUM_DISABLE);
            printf("read SerialCtrl after enable CSUM, read value = %x, ret=%d\n", data_buf, ret);
            ret = reg_read(&data_buf, 7, CHECKSUM_ENABLE);
            printf("read SerialCtrl, read value = %x, ret=%d\n", data_buf, ret);
	    if(data_buf == 0x00004D){
                set_csum = 1;
	    }
	}
	delay(1);
    }
    */
     
    while(keepRunning){    
        // read Config0
	//reg_read(&data_buf, 0, CHECKSUM_ENABLE);
        reg_write(0x400000, 0, CHECKSUM_DISABLE);
        printf("read Config0, default val = 0x400000, read value = %x\n", data_buf);
        delay(1);
        /* 
        // read Config1
        reg_read(&data_buf, 1, CHECKSUM_ENABLE);
        reg_read(&data_buf, 1, CHECKSUM_DISABLE);
        printf("read Config1, default val = 0x00EEEE, read value = %x\n", data_buf);
	delay(1);

	// read UART control
        reg_read(&data_buf, 7, CHECKSUM_DISABLE);
        printf("read UART control, default val = 0x02004D, read value = %x\n\n", data_buf);
	delay(1);

        //start_conversion(CONVERSION_TYPE_SINGLE, 0, 10000);
	delay(1);
	*/
	
    }
    return 0;
}



