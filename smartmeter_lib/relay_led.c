#include "relay_led.h"






/*
 * Use to initialize GPIO of raspi for relay and LED control
 *
 */
void relay_led_gpio_init(relay_led_config_t *s_config)
{
    //ret = wiringPiSetupGpio();
    printf("GPIO setup for Relay LED \n");
    pinMode(s_config->relay_open_pin,  OUTPUT);
    pinMode(s_config->relay_close_pin, OUTPUT);
    pinMode(s_config->led_kwh_pin,  OUTPUT);
    pinMode(s_config->led_kvarh_pin,OUTPUT);
}

/*
 * Connect the output
 *
 */
void relay_connect(relay_led_config_t *s_config)
{
    // reset both IO pin of relay
    digitalWrite(s_config->relay_open_pin, 0);
    digitalWrite(s_config->relay_close_pin,0);
    
    // switch relay
    digitalWrite(s_config->relay_close_pin, 1);
    delay(100);
    digitalWrite(s_config->relay_close_pin, 0);

    // update state
    s_config->relay_state = RELAY_STATE_CONNECT;
}

/*
 * Disconnect the output
 *
 */
void relay_disconnect(relay_led_config_t *s_config)
{
    // reset both IO pin of relay
    digitalWrite(s_config->relay_open_pin, 0);
    digitalWrite(s_config->relay_close_pin,0);
    
    // switch relay
    digitalWrite(s_config->relay_open_pin, 1);
    delay(100);
    digitalWrite(s_config->relay_open_pin, 0);
    
    // update state
    s_config->relay_state = RELAY_STATE_DISCONNECT;
}

void led_kwh_on(relay_led_config_t *s_config)
{  
    digitalWrite(s_config->led_kwh_pin, 1);
}

void led_kwh_off(relay_led_config_t *s_config)
{  
    digitalWrite(s_config->led_kwh_pin, 0);
}

void led_kvarh_on(relay_led_config_t *s_config)
{  
    digitalWrite(s_config->led_kvarh_pin, 1);
}

void led_kvarh_off(relay_led_config_t *s_config)
{  
    digitalWrite(s_config->led_kvarh_pin, 0);
}

void led_kwh_pulse(relay_led_config_t *s_config)
{
    led_kwh_on(s_config);
    delay(100);
    led_kwh_off(s_config);
}

void led_kvarh_pulse(relay_led_config_t *s_config)
{
    led_kvarh_on(s_config);
    delay(100);
    led_kvarh_off(s_config);
}
