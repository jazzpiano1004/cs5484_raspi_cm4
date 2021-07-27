#include "cs5484_wiringpi.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

#define R_DIVIDER_GAIN_INVERSE   2477
#define FULLSCALE_RAWDATA        0x999999
#define FULLSCALE_INPUT_VOLTAGE  0.250
#define CT_RATIO                 2500
#define R_BURDEN                 10
int get_voltage_raw(uint8_t input_channel, uint8_t csum_en);
int get_current_raw(uint8_t input_channel, uint8_t csum_en);
double get_actual_value_voltage(uint32_t raw_data);
double get_actual_value_current(uint32_t raw_data);

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
 
    uint32_t voltage_raw, current_raw;
    double v, i;

    while(keepRunning){    
        ret = start_conversion(CONVERSION_TYPE_SINGLE, CHECKSUM_DISABLE, 1000);
	if(ret == STATUS_OK){
            voltage_raw = get_voltage_raw(ANALOG_INPUT_CH1, CHECKSUM_DISABLE);
            current_raw = get_current_raw(ANALOG_INPUT_CH1, CHECKSUM_DISABLE);
            v = get_actual_value_voltage(voltage_raw);
            i = get_actual_value_current(current_raw);
	    printf("AC Line Voltage, Current = %f, %f\n", v, i);
	}
    }
    return 0;
}



int get_voltage_raw(uint8_t input_channel, uint8_t csum_en)
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

int get_current_raw(uint8_t input_channel, uint8_t csum_en)
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

double get_actual_value_voltage(uint32_t raw_data){
    double vrms;       // voltage at input of CS5484
    double vrms_line;  // AC Line voltage
    
    // calculate rms voltage at input of CS5484
    vrms = (double)raw_data / FULLSCALE_RAWDATA * FULLSCALE_INPUT_VOLTAGE / sqrt(2);
    
    // calculate AC line voltage
    vrms_line = vrms * R_DIVIDER_GAIN_INVERSE;

    return vrms_line;
}

double get_actual_value_current(uint32_t raw_data){
    double vrms;       // voltage at input of CS5484
    double irms_line;  // AC Line voltage
    
    // calculate rms voltage at input of CS5484
    vrms = (double)raw_data / FULLSCALE_RAWDATA * FULLSCALE_INPUT_VOLTAGE / sqrt(2);
    
    // calculate Load current
    irms_line = CT_RATIO * vrms / R_BURDEN;

    return irms_line;
}
