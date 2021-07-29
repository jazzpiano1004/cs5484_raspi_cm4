#include "cs5484_wiringpi.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

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

uint32_t get_voltage_raw(uint8_t input_channel, uint8_t csum_en);
uint32_t get_current_raw(uint8_t input_channel, uint8_t csum_en);
uint32_t get_power_raw(uint8_t input_channel, uint8_t csum_en);
uint32_t get_pf_raw(uint8_t input_channel, uint8_t csum_en);
double get_actual_value_voltage(uint32_t raw_data);
double get_actual_value_current(uint32_t raw_data);
double get_actual_value_power(uint32_t raw_data);
double get_actual_value_pf(uint32_t raw_data);



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
        /*
	printf("Turn off GPIO of SPI before exit\n");
	pullUpDnControl(CS_PIN, PUD_OFF);
	digitalWrite(CS_PIN, 0);
	*/
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
    pullUpDnControl(CS_PIN, PUD_OFF);
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
     
    // read Config0
    reg_read(&data_buf, 0, CHECKSUM_DISABLE);
    printf("read Config0, default val = 0x400000, read value = %x\n", data_buf);
    delay(1000);
         
    // read Config1
    reg_read(&data_buf, 1, CHECKSUM_ENABLE);
    reg_read(&data_buf, 1, CHECKSUM_DISABLE);
    printf("read Config1, default val = 0x00EEEE, read value = %x\n", data_buf);
    delay(1000);

    // read UART control
    reg_read(&data_buf, 7, CHECKSUM_DISABLE);
    printf("read UART control, default val = 0x02004D, read value = %x\n\n", data_buf);
    delay(1000);
 
    uint32_t voltage_raw, current_raw, power_raw, pf_raw;
    double v, i, p, pf;

    while(keepRunning){    
        ret = start_conversion(CONVERSION_TYPE_SINGLE, CHECKSUM_DISABLE, 1000);
	if(ret == STATUS_OK){
            voltage_raw = get_voltage_raw(ANALOG_INPUT_CH2, CHECKSUM_DISABLE);
            current_raw = get_current_raw(ANALOG_INPUT_CH2, CHECKSUM_DISABLE);
            power_raw = get_power_raw(ANALOG_INPUT_CH2, CHECKSUM_DISABLE);
	    pf_raw = get_pf_raw(ANALOG_INPUT_CH2, CHECKSUM_DISABLE);
            v = get_actual_value_voltage(voltage_raw);
            i = get_actual_value_current(current_raw);
            p = get_actual_value_power(power_raw);
            pf = get_actual_value_pf(pf_raw);
	    printf("AC Line Voltage, Current, Power, PF = %.3f, %.3f, %.3f, %.3f\n", v, i, p, pf);
	    //printf("AC Line Voltage, Current, Power, PF = %.3f, %.3f, %.3f, %d\n", v, i, p, pf_raw);
	}
    }
    return 0;
}



uint32_t get_voltage_raw(uint8_t input_channel, uint8_t csum_en)
{
    uint8_t addr;
    uint32_t raw;
    int v;

    if(input_channel == ANALOG_INPUT_CH1)   	addr = 7;
    else if(input_channel == ANALOG_INPUT_CH2)	addr = 13;
    else return STATUS_FAIL;

    page_select(16, csum_en);
    reg_read(&raw, addr, csum_en);

    return raw;
}

uint32_t get_current_raw(uint8_t input_channel, uint8_t csum_en)
{
    uint8_t addr;
    uint32_t raw;
    int i;

    if(input_channel == ANALOG_INPUT_CH1)   	addr = 6;
    else if(input_channel == ANALOG_INPUT_CH2)	addr = 12;  
    else return STATUS_FAIL;

    page_select(16, csum_en);
    reg_read(&raw, addr, csum_en);
    
    return raw;
}

uint32_t get_power_raw(uint8_t input_channel, uint8_t csum_en)
{
    uint8_t addr;
    uint32_t raw;
    int p;

    if(input_channel == ANALOG_INPUT_CH1)   	addr = 5;
    else if(input_channel == ANALOG_INPUT_CH2)	addr = 11;
    else return STATUS_FAIL;

    page_select(16, csum_en);
    reg_read(&raw, addr, csum_en);

    return raw;
}

uint32_t get_pf_raw(uint8_t input_channel, uint8_t csum_en)
{
    uint8_t addr;
    uint32_t raw;
    int pf;

    if(input_channel == ANALOG_INPUT_CH1)   	addr = 21;
    else if(input_channel == ANALOG_INPUT_CH2)	addr = 25;
    else return STATUS_FAIL;

    page_select(16, csum_en);
    reg_read(&raw, addr, csum_en);

    return raw;
}

double get_actual_value_voltage(uint32_t raw_data){
    double vrms;       // voltage at voltage input of CS5484
    double vrms_line;  // AC Line voltage
    
    // calculate rms voltage at input of CS5484
    vrms = (double)raw_data / FULLSCALE_RAWDATA_VOLTAGE * FULLSCALE_INPUT_VOLTAGE / sqrt(2);
    
    // calculate AC line voltage
    vrms_line = vrms * R_DIVIDER_GAIN_INVERSE;

    return vrms_line;
}

double get_actual_value_current(uint32_t raw_data){
    double vrms;       // voltage at current input of CS5484
    double irms_line;  // AC Line voltage
    
    // calculate rms voltage at input of CS5484
    vrms = (double)raw_data / FULLSCALE_RAWDATA_CURRENT * FULLSCALE_INPUT_VOLTAGE / sqrt(2);
    
    // calculate Load current
    irms_line = CT_RATIO * vrms / R_BURDEN;

    return irms_line;
}

double get_actual_value_power(uint32_t raw_data){
    double p;       // power
    long rawdata_4byte;
    int8_t  sign_neg=0;
    
    // Detect sign bit of 24-bit 2nd compliment number
    /*
    if(raw_data & 0x800000){
	// negative sign detected flag
	sign_neg = 1;

	// 1st complement (inverse bitwise)
	raw_data = ~raw_data;

	// add 1
	raw_data++;
    }

    // Crop only 24-bit 2nd complement
    raw_data &= 0xFFFFFF;

    // show result
    if(sign_neg != 0)   rawdata_4byte = -raw_data;
    else                rawdata_4byte = raw_data;
    */

    if(raw_data & 0x800000){
        rawdata_4byte = (long)(raw_data - 0x1000000);	
    }
    else{
	rawdata_4byte = (long)raw_data;
    }
    // calculate power at cs5484 input
    p = (double)rawdata_4byte / FULLSCALE_RAWDATA_POWER * FULLSCALE_OUTPUT_POWER;
    
    return p;
}

double get_actual_value_pf(uint32_t raw_data){
    double pf;       // power
    long rawdata_4byte;
    int8_t  sign_neg=0;
    
    // Detect sign bit of 24-bit 2nd compliment number
    /*
    if(raw_data & 0x800000){
	// negative sign detected flag
	sign_neg = 1;

	// 1st complement (inverse bitwise)
	raw_data = ~raw_data;

	// add 1
	raw_data++;
    }

    // Crop only 24-bit 2nd complement
    raw_data &= 0xFFFFFF;

    // show result
    if(sign_neg != 0)   rawdata_4byte = -raw_data;
    else                rawdata_4byte = raw_data;
    */

    if(raw_data & 0x800000){
        rawdata_4byte = (long)(raw_data - 0x1000000);	
    }
    else{
	rawdata_4byte = (long)raw_data;
    }
    // calculate power at cs5484 input
    pf = ((double)(rawdata_4byte) / 0x7FFFFF) * 100;
    
    return pf;
}
