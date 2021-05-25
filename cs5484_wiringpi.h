#ifndef CS5484_WIRINGPI_H
#define CS5484_WIRINGPI_H


#include <stdio.h>
#include <stdint.h>
#include <wiringPiSPI.h>
#include <wiringPi.h>

#define SPI_CHANNEL 	0
#define CS_PIN 		5

#define STATUS_OK	0
#define STATUS_FAIL 	1
#define STATUS_TIMEOUT 	2

#define CONVERSION_TYPE_SINGLE 	   1
#define CONVERSION_TYPE_CONTINUOUS 2

#define ANALOG_INPUT_CH1 1
#define ANALOG_INPUT_CH2 2


uint8_t reg_write(uint32_t data, uint8_t addr, uint8_t csum_en);
uint8_t reg_read(uint32_t *data, uint8_t addr, uint8_t csum_en);
uint8_t page_select(uint8_t page, uint8_t csum_en);
uint8_t instruction(uint8_t instruct_code, uint8_t csum_en);
uint8_t reset(uint8_t csum_en);
uint8_t start_conversion(uint8_t conversion_type, uint8_t csum_en, int timeout);
uint32_t get_voltage_peak(uint8_t input_channel, uint8_t csum_en);
uint32_t get_voltage_rms(uint8_t input_channel, uint8_t csum_en);
uint32_t get_current_peak(uint8_t input_channel, uint8_t csum_en);
uint32_t get_current_rms(uint8_t input_channel, uint8_t csum_en);
uint32_t get_power_avg(uint8_t input_channel, uint8_t csum_en);



#endif
