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
        digitalWrite(CS_PIN, 0);
        digitalWrite(MOSI_PIN, 0);
        digitalWrite(MISO_PIN, 0);
        digitalWrite(SCK_PIN, 0);
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
    printf("Init wiringPi\n");
    ret = wiringPiSetupGpio();
    printf("wiringpi setup, ret=%d\n", ret);
    pinMode(CS_PIN, OUTPUT);
    pinMode(MOSI_PIN, OUTPUT);
    pinMode(MISO_PIN, OUTPUT);
    pinMode(SCK_PIN, OUTPUT);
    wiringpi_setup = 1;

    while(keepRunning){
    printf("turn off\n");
    digitalWrite(CS_PIN, 0);
    digitalWrite(MOSI_PIN, 0);
    digitalWrite(MISO_PIN, 0);
    digitalWrite(SCK_PIN, 0);
    delay(1);
    printf("turn on\n");
    digitalWrite(CS_PIN, 1);
    digitalWrite(MOSI_PIN, 1);
    digitalWrite(MISO_PIN, 1);
    digitalWrite(SCK_PIN, 1);
    delay(1);
    }
    return 0;
}



