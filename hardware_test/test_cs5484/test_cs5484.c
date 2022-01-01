#include "../../smartmeter_lib/cs5484_wiringpi.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>

/*
#define FULLSCALE_RAWDATA_VOLTAGE        0x999999
#define FULLSCALE_RAWDATA_CURRENT        0x999999
#define FULLSCALE_RAWDATA_POWER          0x2E147B
#define FULLSCALE_INPUT_VOLTAGE          0.250
#define R_DIVIDER_GAIN_INVERSE           2477
#define CT_RATIO                         2500
#define R_BURDEN                         5.1
#define FULLSCALE_OUTPUT_VOLTAGE         0.250 / sqrt(2) * R_DIVIDER_GAIN_INVERSE
#define FULLSCALE_OUTPUT_CURRENT         0.250 / sqrt(2) * CT_RATIO / R_BURDEN
#define FULLSCALE_OUTPUT_POWER           FULLSCALE_OUTPUT_VOLTAGE * FULLSCALE_OUTPUT_VOLTAGE
*/




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
     * CS5484 Basic Configuration
     *
     */
    cs5484_basic_config_t   s_cs5484_config;

    s_cs5484_config.spi_bus_speed = 100000;
    s_cs5484_config.spi_cs_pin = CS_PIN;
    s_cs5484_config.spi_mosi_pin = MOSI_PIN;
    s_cs5484_config.spi_miso_pin = MISO_PIN;
    s_cs5484_config.spi_sck_pin = SCK_PIN;
    s_cs5484_config.chip_reset_pin = RST_PIN;
    s_cs5484_config.csum_en = 0;
    s_cs5484_config.conversion_type = CONVERSION_TYPE_CONTINUOUS;
    
    

    /*
     * CS5484 Input Configuration of Channel_1
     *
     */
    cs5484_input_config_t   s_cs5484_channel_1;
    s_cs5484_channel_1.channel = 1;
    s_cs5484_channel_1.filter_mode_v = FILTER_MODE_HPF;
    s_cs5484_channel_1.filter_mode_i = FILTER_MODE_HPF;
    s_cs5484_channel_1.phase_error = 0;
    s_cs5484_channel_1.ac_offset_i = 0;
    s_cs5484_channel_1.offset_p = 0;
    s_cs5484_channel_1.offset_q = 0;
    
    
    /*
     * CS5484 Input Configuration of Channel_2
     *
     */
    cs5484_input_config_t   s_cs5484_channel_2;
    s_cs5484_channel_2.channel = 2;
    s_cs5484_channel_2.filter_mode_v = FILTER_MODE_DISABLE;
    s_cs5484_channel_2.filter_mode_i = FILTER_MODE_DISABLE;
    s_cs5484_channel_2.phase_error = 0;
    s_cs5484_channel_2.ac_offset_i = 0;
    s_cs5484_channel_2.offset_p = 0;
    s_cs5484_channel_2.offset_q = 0;



    /*
     *  initialize wiring-Pi and SPI interface
     *
     */
    int ret; // for return status of function call
    wiringPiSetupGpio();
    wiringpi_setup = 1;
    pinMode(CS_PIN, OUTPUT);
    pinMode(RST_PIN, OUTPUT);
    pullUpDnControl(CS_PIN, PUD_OFF);
    pullUpDnControl(RST_PIN, PUD_UP);
    digitalWrite(CS_PIN, 1);
    digitalWrite(RST_PIN, 1);
    ret = wiringPiSPISetupMode(SPI_CHANNEL_0, SPI_BUS_SPEED, SPI_MODE_CS5484);
    printf("SPI init result : %d\r\n", ret);
    delay(1000);
    


    /*
     * CS5484 : Reset CMD
     *
     */
    digitalWrite(RST_PIN, 0);
    delay(1000);
    digitalWrite(RST_PIN, 1);
    delay(1000);
    cs5484_reset(&s_cs5484_config);
    printf("Reset CS5484 : ret=%d\r\n", ret);
    delay(10000);
    
    cs5484_page_select(&s_cs5484_config, 0);
    printf("change page, ret=%d\n", ret);
    
    
    
    /*
     * CS5484 : Test Write and Read back (Echo)
     *
     */
    uint32_t data_buf;
     
    // read Config0
    cs5484_reg_read(&s_cs5484_config, &data_buf, 0);
    printf("read Config0, default val = 0x400000, read value = %x\n", data_buf);
    delay(1000);
         
    // read Config1
    cs5484_reg_read(&s_cs5484_config, &data_buf, 1);
    cs5484_reg_read(&s_cs5484_config, &data_buf, 1);
    printf("read Config1, default val = 0x00EEEE, read value = %x\n", data_buf);
    delay(1000);

    // read UART control
    cs5484_reg_read(&s_cs5484_config, &data_buf, 7);
    printf("read UART control, default val = 0x02004D, read value = %x\n", data_buf);
    delay(1000);

    /* 
     * Phase compensation testing
     *
     */
    // read phase compensation 
    cs5484_reg_read(&s_cs5484_config, &data_buf, 5);
    printf("read Phase compensation reg (before), read value = %x\n", data_buf);
    delay(1000);
    // phase compensation test
    cs5484_set_phase_compensation(&s_cs5484_config, &s_cs5484_channel_1);
    // read phase compensation 
    cs5484_reg_read(&s_cs5484_config, &data_buf, 5);
    printf("After Phase compensation, read value = %x\n", data_buf);
    delay(1000);

    // enable temp sensor in cs5484 
    cs5484_temperature_enable(&s_cs5484_config, 1);
    cs5484_page_select(&s_cs5484_config, 0);
    cs5484_reg_read(&s_cs5484_config, &data_buf, 0);
    printf("Enable temp sensor, config0 is now %x\n\n", data_buf);
    
    // enable high-pass filter for DC removal 
    cs5484_input_filter_init(&s_cs5484_config, &s_cs5484_channel_1);
    
    // offset setting
    cs5484_page_select(&s_cs5484_config, 16);
    cs5484_reg_read(&s_cs5484_config, &data_buf, 37);
    printf("read offset current : %d\n", data_buf);
    cs5484_set_offset(&s_cs5484_config, &s_cs5484_channel_1);
    cs5484_page_select(&s_cs5484_config, 16);
    cs5484_reg_read(&s_cs5484_config, &data_buf, 37);
    printf("after set offset current : %d\n", data_buf);

    uint32_t voltage_raw, current_raw, power_raw, pf_raw;
    double v, i, p, q, pf;
    double temp;
    uint32_t timeout;

    while(keepRunning){    
        cs5484_start_conversion(&s_cs5484_config);
        ret = cs5484_wait_for_conversion(&s_cs5484_config, 200);
	if(ret == STATUS_OK){
            i  = cs5484_get_current_rms(&s_cs5484_config, &s_cs5484_channel_1);
            v  = cs5484_get_voltage_rms(&s_cs5484_config, &s_cs5484_channel_1);
            p  = cs5484_get_act_power_avg(&s_cs5484_config, &s_cs5484_channel_1);
            q  = cs5484_get_react_power_avg(&s_cs5484_config, &s_cs5484_channel_1);
	    pf = cs5484_get_pf(&s_cs5484_config, &s_cs5484_channel_1);
	    
	    
	    timeout=0;
	    while(!cs5484_is_temperature_ready(&s_cs5484_config) && !(timeout > 2000)){
	        // wait for TUP, do nothing
		// It should be better if interrupt is used in this aplication. 
		timeout++;
	    }
	    if(timeout > 2000){
                cs5484_page_select(&s_cs5484_config, 0);
                cs5484_reg_read(&s_cs5484_config, &data_buf, 0);
    		printf("temperature timeout. checking config0, is now %x\n", data_buf);
                cs5484_temperature_enable(&s_cs5484_config, 1);
    		printf("temperature timeout reconfig... \n");
	    }
	    else{
		temp = cs5484_get_temperature(&s_cs5484_config);
	    }
	    
            cs5484_page_select(&s_cs5484_config, 16);
            cs5484_reg_read(&s_cs5484_config, &data_buf, 37);
            printf("after set offset current : %d\n", data_buf);
            printf("AC Line Voltage, Current, Power, PF = %.3f, %.3f, %.3f, %.3f temp(C)=%.2f\n", v, i, p, pf, temp);
            //printf("AC Line Voltage, Current, P, Q, PF = %.3f, %.3f, %.3f, %.3f, %.3f\n", v, i, p, q, pf);
	}
    }
    return 0;
}



