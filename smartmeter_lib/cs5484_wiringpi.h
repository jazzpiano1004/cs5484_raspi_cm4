#ifndef CS5484_WIRINGPI_H
#define CS5484_WIRINGPI_H


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <wiringPiSPI.h>
#include <wiringPi.h>
#include <math.h>

#define DEBUG_CS5484   0

/*
 * SPI Bus Parameters
 *
 */
#define SPI_BUS_SPEED 	        1000000
#define SPI_CHANNEL_0 		0
#define CS_PIN 			8
#define MISO_PIN        	9
#define MOSI_PIN        	10
#define SCK_PIN         	11
#define SPI_MODE_CS5484		3
#define RST_PIN                 25

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
#define FILTER_MODE_DISABLE     0
#define FILTER_MODE_HPF         1
#define FILTER_MODE_PMF         2
#define T_SETTLING_2000MS       8000

#define FULLSCALE_RAWDATA_VOLTAGE        0x999999
#define FULLSCALE_RAWDATA_CURRENT        0x999999
#define FULLSCALE_RAWDATA_POWER          0x2E147B
#define FULLSCALE_INPUT_VOLTAGE          0.250
#define R_DIVIDER_GAIN_INVERSE           2477
#define CT_RATIO                         2500
#define R_BURDEN                         5.1
#define FULLSCALE_OUTPUT_VOLTAGE         0.250 / sqrt(2) * R_DIVIDER_GAIN_INVERSE  // Fullscale voltage at voltage input of CS5484 (Vrms)
#define FULLSCALE_OUTPUT_CURRENT         0.250 / sqrt(2) * CT_RATIO / R_BURDEN     // Fullscale load current of CS5484 (Arms)
#define FULLSCALE_OUTPUT_POWER           FULLSCALE_OUTPUT_VOLTAGE * FULLSCALE_OUTPUT_CURRENT

#define TEMPSENSOR_CALIBRATE_COEFF          (double)1.10893
#define TEMPSENSOR_CALIBRATE_OFFSET         (double)12.4883

/* 
 * Struct for Raspi-to-CS5484 Basic Configuration 
 * - SPI configuration
 * - GPIO mapping
 * - Checksum
 * - Conversion type
 */
typedef struct
{
    uint8_t     pi_spi_channel;
    uint8_t     spi_mode;
    uint32_t	spi_bus_speed;
    uint8_t	spi_cs_pin;
    uint8_t	spi_mosi_pin;
    uint8_t	spi_miso_pin;
    uint8_t	spi_sck_pin;
    uint8_t	chip_reset_pin;
    uint8_t	csum_en;
    uint8_t	conversion_type;
    uint32_t    t_settling;
    uint32_t    sample_count;

}cs5484_basic_config_t;

/* 
 * Struct for Raspi-to-CS5484 analog input channel configuration 
 * - I, V Channel
 * - Phase Compensatation
 * - Gain, Offset calibration
 * 
 */
typedef struct
{
    int8_t	channel;
    uint32_t	gain_i;        // range 1.0 <-> 4.0
    uint32_t    gain_v;        // range 1.0 <-> 4.0
    uint32_t    dc_offset_i;
    uint32_t    dc_offset_v;
    uint32_t    ac_offset_i;
    uint32_t    offset_p;      
    uint32_t    offset_q;
    float	phase_error;   // range -8.99 <-> 8.99
    uint8_t     filter_mode_i;
    uint8_t     filter_mode_v;

}cs5484_input_config_t;



/*
 * CS5484 basic command
 *
 */
uint8_t cs5484_reg_write(cs5484_basic_config_t *s_cs5484_config,
			uint32_t data,
			uint8_t addr);
uint8_t cs5484_reg_read(cs5484_basic_config_t *s_cs5484_config, 
			uint32_t *data,
			uint8_t addr);
uint8_t cs5484_page_select(cs5484_basic_config_t *s_cs5484_config, uint8_t page);
uint8_t cs5484_instruction(cs5484_basic_config_t *s_cs5484_config, uint8_t instruct_code);
uint8_t cs5484_reset(cs5484_basic_config_t *s_cs5484_config);

/*
 * Energy meter functions
 *
 */
uint8_t  cs5484_start_conversion(cs5484_basic_config_t *s_cs5484_config);
uint8_t  cs5484_stop_conversion(cs5484_basic_config_t *s_cs5484_config);
uint8_t  cs5484_wait_for_conversion(cs5484_basic_config_t *s_cs5484_config, int timeout);

uint32_t cs5484_get_voltage_peak(cs5484_basic_config_t *s_cs5484_config,
				 cs5484_input_config_t *s_cs5484_input_config);

uint32_t cs5484_get_current_peak(cs5484_basic_config_t *s_cs5484_config,
				 cs5484_input_config_t *s_cs5484_input_config);

double cs5484_get_voltage_rms(cs5484_basic_config_t *s_cs5484_config,
			      cs5484_input_config_t *s_cs5484_input_config);

double cs5484_get_current_rms(cs5484_basic_config_t *s_cs5484_config,
			      cs5484_input_config_t *s_cs5484_input_config);

double cs5484_get_act_power_avg(cs5484_basic_config_t *s_cs5484_config,
				cs5484_input_config_t *s_cs5484_input_config);

double cs5484_get_react_power_avg(cs5484_basic_config_t *s_cs5484_config,
				  cs5484_input_config_t *s_cs5484_input_config);

double cs5484_get_apparent_power_avg(cs5484_basic_config_t *s_cs5484_config,
				     cs5484_input_config_t *s_cs5484_input_config);

double cs5484_get_pf(cs5484_basic_config_t *s_cs5484_config,
		     cs5484_input_config_t *s_cs5484_input_config);

/* 
 * For calibration
 *
 */

uint8_t cs5484_temperature_enable(cs5484_basic_config_t * s_cs5484_config, int enable_flag);
int8_t  cs5484_is_temperature_ready(cs5484_basic_config_t *s_cs5484_config);
double  cs5484_get_temperature(cs5484_basic_config_t *s_cs5484_config);

uint8_t cs5484_set_phase_compensation(cs5484_basic_config_t *s_cs5484_config,
				      cs5484_input_config_t *s_cs5484_input_config);

uint8_t cs5484_input_filter_init(cs5484_basic_config_t *s_cs5484_config,
		                 cs5484_input_config_t *s_cs5484_input_config);
uint8_t cs5484_set_settlingtime(cs5484_basic_config_t *s_cs5484_config);

uint8_t  cs5484_set_offset_act_power(cs5484_basic_config_t *s_cs5484_config, 
		                    cs5484_input_config_t *s_cs5484_input_config);
uint32_t cs5484_get_offset_act_power(cs5484_basic_config_t *s_cs5484_config, 
		                    cs5484_input_config_t *s_cs5484_input_config);
uint8_t  cs5484_set_offset_react_power(cs5484_basic_config_t *s_cs5484_config, 
		                      cs5484_input_config_t *s_cs5484_input_config);
uint32_t cs5484_get_offset_react_power(cs5484_basic_config_t *s_cs5484_config, 
		                      cs5484_input_config_t *s_cs5484_input_config);

uint8_t  cs5484_set_offset_current(cs5484_basic_config_t *s_cs5484_config, 
		                  cs5484_input_config_t *s_cs5484_input_config);
uint32_t cs5484_get_offset_current(cs5484_basic_config_t *s_cs5484_config, 
	   	                    cs5484_input_config_t *s_cs5484_input_config);

uint8_t  cs5484_set_gain_current(cs5484_basic_config_t *s_cs5484_config, 
		                cs5484_input_config_t *s_cs5484_input_config);
uint32_t cs5484_get_gain_current(cs5484_basic_config_t *s_cs5484_config, 
		                cs5484_input_config_t *s_cs5484_input_config);
uint8_t  cs5484_set_gain_voltage(cs5484_basic_config_t *s_cs5484_config, 
		                cs5484_input_config_t *s_cs5484_input_config);
uint32_t cs5484_get_gain_voltage(cs5484_basic_config_t *s_cs5484_config, 
		                cs5484_input_config_t *s_cs5484_input_config);

uint8_t cs5484_send_calibration_cmd_gain(cs5484_basic_config_t *s_cs5484_config);
uint8_t cs5484_send_calibration_cmd_offset(cs5484_basic_config_t *s_cs5484_config);
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
