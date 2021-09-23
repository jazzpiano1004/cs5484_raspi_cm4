#ifndef CS5484_WIRINGPI_H
#define CS5484_WIRINGPI_H


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <wiringPiSPI.h>
#include <wiringPi.h>
#include <math.h>

/*
 * SPI Bus Parameters
 *
 */
#define BUS_SPEED 	        1000000
#define SPI_CHANNEL 		0
#define CS_PIN 			8
#define MISO_PIN        	9
#define MOSI_PIN        	10
#define SCK_PIN         	11
#define SPI_MODE_CS5484		3

/*
 * CS5484 Parameters
 *
 */
#define CHECKSUM_DISABLE        0
#define CHECKSUM_ENABLE         1
#define STATUS_OK		0
#define STATUS_FAIL 		1
#define STATUS_TIMEOUT 		2
#define CONVERSION_TYPE_SINGLE 	   1
#define CONVERSION_TYPE_CONTINUOUS 2
#define ANALOG_INPUT_CH1 	1
#define ANALOG_INPUT_CH2 	2
#define CURRENT_FULLSCALE 	150000*25
#define VOLT_FULLSCALE    	438000
#define POWER_LINE_FULLSCALE	((long long)CURRENT_FULLSCALE*VOLT_FULLSCALE/1000000)
#define FULLSCALE_RAWDATA_VOLTAGE        0x999999
#define FULLSCALE_RAWDATA_CURRENT        0x999999
#define FULLSCALE_RAWDATA_POWER          0x2E147B
#define FULLSCALE_INPUT_VOLTAGE          0.250
#define R_DIVIDER_GAIN_INVERSE           2477
#define CT_RATIO                         2500
#define R_BURDEN                         10
#define FULLSCALE_OUTPUT_VOLTAGE         0.250 / sqrt(2) * R_DIVIDER_GAIN_INVERSE
#define FULLSCALE_OUTPUT_CURRENT         0.250 / sqrt(2) * CT_RATIO / R_BURDEN
#define FULLSCALE_OUTPUT_POWER           FULLSCALE_OUTPUT_VOLTAGE * FULLSCALE_OUTPUT_VOLTAGE

/*
 * CS5484 basic command
 *
 */
uint8_t reg_write(uint32_t data, uint8_t addr, uint8_t csum_en);
uint8_t reg_read(uint32_t *data, uint8_t addr, uint8_t csum_en);
uint8_t page_select(uint8_t page, uint8_t csum_en);
uint8_t instruction(uint8_t instruct_code, uint8_t csum_en);
uint8_t reset(uint8_t csum_en);

/*
 * Energy meter functions
 *
 */
uint8_t start_conversion(uint8_t conversion_type, uint8_t csum_en, int timeout);
uint32_t get_voltage_peak(uint8_t input_channel, uint8_t csum_en);
uint32_t get_current_peak(uint8_t input_channel, uint8_t csum_en);
double get_voltage_rms(uint8_t input_channel, uint8_t csum_en);
double get_current_rms(uint8_t input_channel, uint8_t csum_en);
double get_act_power_avg(uint8_t input_channel, uint8_t csum_en);
double get_react_power_avg(uint8_t input_channel, uint8_t csum_en);
double get_apparent_power_avg(uint8_t input_channel, uint8_t csum_en);
double get_pf(uint8_t input_channel, uint8_t csum_en);

/*
 * Function from old version of smartmeter
 *
 */
/*
int CalFullScale(uint32_t fullscale,uint32_t full_reg, uint32_t raw);
long convert3byteto4byte(unsigned long raw);
int CalPow(int raw);
int CalPF (int raw);
*/

#endif
