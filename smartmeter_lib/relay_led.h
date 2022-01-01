#ifndef RELAY_LED_H
#define RELAY_LED_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <wiringPi.h>

#define PIN_RELAY_OPEN     6
#define PIN_RELAY_CLOSE    5 
#define PIN_LED_KWH        13
#define PIN_LED_KVARH      26
#define RELAY_STATE_DISCONNECT   0
#define RELAY_STATE_CONNECT      1
/*
 * Struct for Relay & Energy LED control
 *
 */
typedef struct
{
    uint8_t	relay_open_pin;
    uint8_t	relay_close_pin;
    uint8_t     relay_state;
    uint8_t	led_kwh_pin;
    uint8_t	led_kvarh_pin;

}relay_led_config_t;



void relay_led_gpio_init(relay_led_config_t *s_config);
void relay_connect(relay_led_config_t *s_config);
void relay_disconnect(relay_led_config_t *s_config);

void led_kwh_on(relay_led_config_t *s_config);
void led_kwh_off(relay_led_config_t *s_config);
void led_kvarh_on(relay_led_config_t *s_config);
void led_kvarh_off(relay_led_config_t *s_config);
void led_kwh_pulse(relay_led_config_t *s_config);
void led_kvarh_pulse(relay_led_config_t *s_config);


#endif
