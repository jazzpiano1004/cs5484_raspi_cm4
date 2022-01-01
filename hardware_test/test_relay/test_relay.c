#include "../../smartmeter_lib/cs5484_wiringpi.h"
#include "../../smartmeter_lib/relay_led.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>



relay_led_config_t   s_relay_config;
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
        led_kwh_off(&s_relay_config);
        led_kvarh_off(&s_relay_config);
    }
}

int main()
{  
    // signal handler initialize (for detect Ctrl + C exist)
    signal(SIGINT, intHandler);



    /*
     *  initialize wiring-Pi for GPIO
     *
     */
    int ret; // for return status of function call
    printf("Init wiringPi\n");
    ret = wiringPiSetupGpio();
    printf("wiringpi setup, ret=%d\n", ret);

    // initialize struct for Relay & LED
    s_relay_config.relay_open_pin = PIN_RELAY_OPEN; 
    s_relay_config.relay_close_pin = PIN_RELAY_CLOSE;
    s_relay_config.relay_state = RELAY_STATE_DISCONNECT; 
    s_relay_config.led_kwh_pin = PIN_LED_KWH; 
    s_relay_config.led_kvarh_pin = PIN_LED_KVARH;
    
    relay_led_gpio_init(&s_relay_config);
    wiringpi_setup = 1;
    
    // turn off 
    led_kwh_off(&s_relay_config);
    led_kvarh_off(&s_relay_config);


    // toggle loop test 
    while(keepRunning){

	if(s_relay_config.relay_state == RELAY_STATE_CONNECT)
	{
	    printf("Disconnect Latching Relay\n");
            relay_disconnect(&s_relay_config);
	    led_kwh_pulse(&s_relay_config);
	}
	else if(s_relay_config.relay_state == RELAY_STATE_DISCONNECT){
	    printf("Connect Latching Relay\n");
	    relay_connect(&s_relay_config);
	    led_kvarh_pulse(&s_relay_config);
	}
	delay(5000);
    }
    return 0;
}

