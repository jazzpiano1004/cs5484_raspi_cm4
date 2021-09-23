#include "cs5484_wiringpi.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>


#define PIN_RELAY_OPEN     5
#define PIN_RELAY_CLOSE    6 

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
        printf("Turn off GPIO before exit\n");
    }
}

void relay_open();
void relay_close();

int main()
{  
    // signal handler initialize (for detect Ctrl + C exist)
    signal(SIGINT, intHandler);

    /*
     *  initialize wiring-Pi and SPI interface
     *
     */
    int ret; // for return status of function call
    printf("Init wiringPi\n");
    ret = wiringPiSetupGpio();
    printf("wiringpi setup, ret=%d\n", ret);
    pinMode(PIN_RELAY_OPEN,  OUTPUT);
    pinMode(PIN_RELAY_CLOSE, OUTPUT);
    wiringpi_setup = 1;
    
    int relay_state=0;
    int tmp;
    while(keepRunning){

	if(relay_state == 1)
	{
	    printf("Relay OFF\n");
            relay_close();
	    tmp = 0;
	}
	else if(relay_state == 0){
	    printf("Relay ON\n");
	    relay_open();
	    tmp = 1;
	}
	relay_state = tmp;
	delay(5000);
    }
    return 0;
}

void relay_close()
{
    digitalWrite(PIN_RELAY_OPEN, 0);
    digitalWrite(PIN_RELAY_CLOSE, 0);
    delay(100);
    digitalWrite(PIN_RELAY_CLOSE, 1);
    delay(100);
    digitalWrite(PIN_RELAY_CLOSE, 0);
}

void relay_open()
{
    digitalWrite(PIN_RELAY_OPEN, 0);
    digitalWrite(PIN_RELAY_CLOSE, 0);
    delay(100);
    digitalWrite(PIN_RELAY_OPEN, 1);
    delay(100);
    digitalWrite(PIN_RELAY_OPEN, 0);
}



