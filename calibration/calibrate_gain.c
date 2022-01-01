#include "../smartmeter_lib/cs5484_wiringpi.h"
#include "../smartmeter_lib/relay_led.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>



/*
 * Configuration struct for measurement HAT board
 *
 */ 
cs5484_basic_config_t   s_cs5484_config;
cs5484_input_config_t   s_cs5484_channel_1;
cs5484_input_config_t   s_cs5484_channel_2;
relay_led_config_t      s_relay_config;


/*
 * Function prototype in calibration program
 *
 */
int meter_wiringpi_init(cs5484_basic_config_t *s_config, relay_led_config_t *s_relay_config);
int meter_chipmeasurement_init(cs5484_basic_config_t *s_cs5484_config,
		               cs5484_input_config_t *s_cs5484_channel_1,
			       cs5484_input_config_t *s_cs5484_channel_2);
void meter_chiptest();
void meter_gain_calibration_1();
void meter_gain_calibration_2();
void meter_load_calibration(FILE **calibration_file, const char *filename);
void meter_save_calibration(FILE **calibration_file, const char *filename);

const char* CALIBRATION_FILENAME = "calibration_output.txt";

/*
 * Main calibration parameters
 *
 */
uint32_t v_gain;
uint32_t i_gain;
uint32_t i_acoffset;
uint32_t p_offset;
uint32_t q_offset;

int main()
{  
    /*
     * CS5484 Basic Configuration
     *
     */
    s_cs5484_config.pi_spi_channel = SPI_CHANNEL_0;
    s_cs5484_config.spi_mode = SPI_MODE_CS5484;
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
    s_cs5484_channel_1.channel = 1;
    s_cs5484_channel_1.filter_mode_v = FILTER_MODE_HPF;
    s_cs5484_channel_1.filter_mode_i = FILTER_MODE_HPF;
    
    /*
     * CS5484 Input Configuration of Channel_2
     *
     */
    s_cs5484_channel_2.channel = 2;
    s_cs5484_channel_2.filter_mode_v = FILTER_MODE_DISABLE;
    s_cs5484_channel_2.filter_mode_i = FILTER_MODE_DISABLE;

    /*
     *  Relay & LED Configuration
     *
     */
    s_relay_config.relay_open_pin = PIN_RELAY_OPEN; 
    s_relay_config.relay_close_pin = PIN_RELAY_CLOSE;
    s_relay_config.relay_state = 1; 
    s_relay_config.led_kwh_pin = PIN_LED_KWH;
    


    /*
     *  initialize meter
     *
     */
    FILE *calibration_file;
    meter_wiringpi_init(&s_cs5484_config, &s_relay_config);
    meter_chiptest();
    meter_load_calibration(&calibration_file, CALIBRATION_FILENAME); // must call this before chipmeasurement_init().
    printf("after load, s_cs5484_channel_1.gain_v = %d\n", s_cs5484_channel_1.gain_v);
    meter_chipmeasurement_init(&s_cs5484_config, &s_cs5484_channel_1, &s_cs5484_channel_2);
    


    /*
     *  gain calibration & update gain for channel1's config
     *
     */
    meter_gain_calibration_2();
    meter_save_calibration(&calibration_file, CALIBRATION_FILENAME);


         
    /*
     *  set filter settling time back to the default, 
     *
     */
    s_cs5484_config.t_settling = 30;
    cs5484_set_settlingtime(&s_cs5484_config);
    cs5484_page_select(&s_cs5484_config, 16);
    cs5484_reg_write(&s_cs5484_config, 4000, 51);
    


    uint32_t voltage_raw, current_raw, power_raw, pf_raw;
    double v, i, p, q, pf;
    double temp;
    uint32_t timeout;
    int ret;
    uint32_t data_buf;
    
    while(1){    
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
                printf("chip config lost! re-init chip...\n");
		meter_chipmeasurement_init(&s_cs5484_config, &s_cs5484_channel_1, &s_cs5484_channel_2);
	    }
	    else{
		temp = cs5484_get_temperature(&s_cs5484_config);
	    }
	    
            printf("AC Line Voltage, Current, Power, PF = %.3f, %.3f, %.3f, %.3f temp(C)=%.2f\n", v, i, p, pf, temp);
	}
    }
    
    return 0;
}



/////////////////////////////////////////////// Initialized functions in smartmeter /////////////////////////////////////////////////////

int meter_wiringpi_init(cs5484_basic_config_t *s_config, relay_led_config_t *s_relay_config)
{
    // WiringPi setup for GPIO and SPI bus
    wiringPiSetupGpio();
    wiringPiSPISetupMode(s_config->pi_spi_channel, s_config->spi_bus_speed, s_config->spi_mode);
    
    // Config GPIO for SPI bus
    pinMode(s_config->spi_cs_pin, OUTPUT);
    pinMode(s_config->chip_reset_pin, OUTPUT);
    pullUpDnControl(s_config->spi_cs_pin, PUD_OFF);
    pullUpDnControl(s_config->chip_reset_pin, PUD_UP);
    digitalWrite(s_config->chip_reset_pin, 1);
    digitalWrite(s_config->spi_cs_pin, 1);

    // Config GPIO for Relay & Pulse LED
    relay_led_gpio_init(s_relay_config);
    led_kwh_off(s_relay_config);
    led_kvarh_off(s_relay_config);

    // Hard Reset CS5484
    digitalWrite(s_config->chip_reset_pin, 0);
    delay(1000);
    digitalWrite(s_config->chip_reset_pin, 1);
    delay(1000);

    // Software reset
    cs5484_reset(s_config);
    delay(10000);

    return STATUS_OK;
}



int meter_chipmeasurement_init(cs5484_basic_config_t *s_cs5484_config,
		               cs5484_input_config_t *s_cs5484_channel_1,
			       cs5484_input_config_t *s_cs5484_channel_2)
{    
    // set AC offset filter
    cs5484_input_filter_init(s_cs5484_config, s_cs5484_channel_1);
    
    // enable temperature sensor
    cs5484_temperature_enable(s_cs5484_config, 1);
    
    // set I1gain, V1gain
    cs5484_set_gain_voltage(s_cs5484_config, s_cs5484_channel_1);
    cs5484_set_gain_current(s_cs5484_config, s_cs5484_channel_1);

    // set Iacoff
    cs5484_set_offset_current(s_cs5484_config, s_cs5484_channel_1);

    // set phase compensation 
    //cs5484_set_phase_compensation(s_cs5484_config, s_cs5484_channel_1);
    //
    return STATUS_OK;
}


void meter_chiptest()
{
    /*
     * CS5484 : Test Write and Read back (Echo)
     *
     */
    uint32_t data_buf;
     
    cs5484_page_select(&s_cs5484_config, 0);

    // read Config0
    cs5484_reg_read(&s_cs5484_config, &data_buf, 0);
    printf("read Config0, default val = 0x400000, read value = %x\n", data_buf);
    delay(1000);
         
    // read Config1
    cs5484_reg_read(&s_cs5484_config, &data_buf, 1);
    printf("read Config1, default val = 0x00EEEE, read value = %x\n", data_buf);
    delay(1000);

    // read UART control
    cs5484_reg_read(&s_cs5484_config, &data_buf, 7);
    printf("read UART control, default val = 0x02004D, read value = %x\n", data_buf);
    delay(1000);
}



//////////////////////////////////////   Calibration Procudures ////////////////////////////////////
void meter_load_calibration(FILE **calibration_file, const char *filename)
{
    char line[1024];
    uint32_t field[5];

    *calibration_file = fopen(filename, "r");
    if(*calibration_file == NULL){
        printf("calibration file does not exist, exit read function\n");
    }
    else{
        fgets(line, 1024, *calibration_file);
        printf("calibration param : %s\n", line);
    	for(int i=0; i<5; i++){
            char *token;
            char tmp[1024];
            int len;
            int len_token;
       
	    // copy line to tmp buffer. because strtok() change input string
	    memcpy(tmp, line, sizeof(line));

	    // extract token 
            token = strtok(tmp, ",");

	    // convert string to floating-point value
	    field[i] = atof(token);

	    // update line by remove the previous token from the front of line string
	    len = strlen(line);
	    len_token = strlen(token);
            memcpy(tmp, line + len_token + 1, sizeof(line) - sizeof(len_token) - sizeof(char));
            memcpy(line, tmp, sizeof(tmp));
        }
        fclose(*calibration_file);
    }

    s_cs5484_channel_1.gain_v = field[0];
    s_cs5484_channel_1.gain_i = field[1];
    s_cs5484_channel_1.ac_offset_i = field[2];
    s_cs5484_channel_1.offset_p = field[3];
    s_cs5484_channel_1.offset_q = field[4];
}


void meter_save_calibration(FILE **calibration_file, const char *filename)
{
    /*
     *  Write calibration param to output file
     *
     */
    uint32_t field[5];
    field[0] = s_cs5484_channel_1.gain_v;
    field[1] = s_cs5484_channel_1.gain_i;
    field[2] = s_cs5484_channel_1.ac_offset_i;
    field[3] = s_cs5484_channel_1.offset_p;
    field[4] = s_cs5484_channel_1.offset_q;

    *calibration_file = fopen(filename, "r+");
    if(*calibration_file != NULL){
        char str_buf[200];
	sprintf(str_buf, "%d,%d,%d,%d,%d\n", \
	        field[0], field[1], field[2], field[3], field[4]);

	fputs(str_buf, *calibration_file);
	fclose(*calibration_file);
    }
}
void meter_gain_calibration_1()
{
    /* 
     * Gain Calibration Procedure
     *
     * 1) Apply reference line voltage and load current
     * 2) Set Vgain to Vmax/Vref*0x400000 , Set scale register to Iref/Imax*0.6*0x800000 when don't use full-load
     * 3) Set settling time to 2000ms, SampleCount=16000
     * 4) Perform continuous conversion (cmd=0xD8) for 2 seconds
     * 5) Stop conversion (0xD8)
     * 6) Clear DRDY status bit
     * 7) Wait for DRDY is set
     * 8) Set settling time, Samplecount back to the default.
     * 10) Start Continuous conversion for verification
     * 11) Read Gain, Offset Register and save on calibration file
     *
     */

    float iref;
    float vref;
    
    printf("Connect the reference load & AC Input to meter\n");
    printf("mte:reference voltage (V)=");
    scanf("%f", &vref);
    printf("mte:reference current(A)=");
    scanf("%f", &iref);

    /*
     *  Set Vgain, Iscale before calibration
     *  Vmax = 430Vrms
     *  Vref = 230Vrms
     *  Imax = FULLSCALE_OUTPUT_CURRENT
     */
    uint32_t i_raw;
    uint32_t v_raw;
    uint32_t iscale;
    uint32_t gain_v;
    uint32_t gain_i;
    //s_cs5484_channel_1.gain_v = (uint32_t)(FULLSCALE_OUTPUT_VOLTAGE / vref * 0x400000);
    //s_cs5484_channel_1.gain_i = (uint32_t)(FULLSCALE_OUTPUT_CURRENT / iref * 0x400000);
    cs5484_set_gain_voltage(&s_cs5484_config, &s_cs5484_channel_1);
    cs5484_set_gain_current(&s_cs5484_config, &s_cs5484_channel_1);
    iscale = iref/FULLSCALE_OUTPUT_CURRENT * 0.6 * 0x800000;
    cs5484_page_select(&s_cs5484_config, 18);
    cs5484_reg_write(&s_cs5484_config, iscale, 63);
    printf("Iscale=0x%x\n", iscale);
    gain_v = cs5484_get_gain_voltage(&s_cs5484_config, &s_cs5484_channel_1);
    gain_i = cs5484_get_gain_current(&s_cs5484_config, &s_cs5484_channel_1);
    printf("voltage gain (before calibration) = %x\n", gain_v);
    printf("current gain (before calibration) = %x\n", gain_i);

    /*
     * Stop conversion before calibration
     *
     */
    cs5484_stop_conversion(&s_cs5484_config);



    /*
     * Set settling time to 2000ms & SampleCount to 16000
     *
     */
    uint32_t settling_time;
    uint32_t sample_count;
    // SampleCount
    cs5484_page_select(&s_cs5484_config, 16);
    cs5484_reg_write(&s_cs5484_config, 16000, 51);
    cs5484_reg_read(&s_cs5484_config, &sample_count, 51);
    printf("sample count = %d, should be %d\n", sample_count, 16000);
    // Settling time
    s_cs5484_config.t_settling = T_SETTLING_2000MS;
    cs5484_set_settlingtime(&s_cs5484_config);
    cs5484_page_select(&s_cs5484_config, 16);
    cs5484_reg_read(&s_cs5484_config, &settling_time, 57);
    printf("settling time = %d, should be %d\n", settling_time, T_SETTLING_2000MS);
    


    /*
     * Send calibration CMD
     *
     */
    // clear DRDY, send calibration CMD
    cs5484_page_select(&s_cs5484_config, 0);          // set to page 0
    cs5484_reg_write(&s_cs5484_config, 0x800000, 23); // clear DRDY in Status0 by write 1 to it
    printf("send gain calibration CMD...\n");
    cs5484_send_calibration_cmd_gain(&s_cs5484_config);
    while(cs5484_wait_for_conversion(&s_cs5484_config, 400) != STATUS_OK); // wait for DRDY



    /*
     *  Read gain after calibration
     *
     */
    gain_v = cs5484_get_gain_voltage(&s_cs5484_config, &s_cs5484_channel_1);
    gain_i = cs5484_get_gain_current(&s_cs5484_config, &s_cs5484_channel_1);
    printf("voltage gain (after calibration) = %x\n", gain_v);
    printf("current gain (after calibration) = %x\n", gain_i);
    delay(1000);
}

void meter_gain_calibration_2()
{
    
    float v_mte, i_mte;
    printf("Connect the reference load & AC Input to meter\n");
    printf("mte:reference voltage(V)=");
    scanf("%f", &v_mte);
    printf("mte:reference current(A)=");
    scanf("%f", &i_mte);
    printf("v_mte=%f, i_mte=%f\n", v_mte, i_mte);
    /*
     *  Read gain before calibration
     *
     */
    uint32_t gain_v, gain_i;
    gain_v = cs5484_get_gain_voltage(&s_cs5484_config, &s_cs5484_channel_1);
    gain_i = cs5484_get_gain_current(&s_cs5484_config, &s_cs5484_channel_1);
    printf("voltage gain (after calibration) = %x\n", gain_v);
    printf("current gain (after calibration) = %x\n", gain_i);
    delay(1000);

    /*
     * Start continuous conversion & measure I, V with average method
     *
     */
    uint32_t i_raw=0, v_raw=0, tmp=0;

    for(int i=0; i<5; i++){
        // start conversion
	cs5484_start_conversion(&s_cs5484_config);
        while(cs5484_wait_for_conversion(&s_cs5484_config, 200) != STATUS_OK);

	// read raw value of Irms, Vrms
        cs5484_page_select(&s_cs5484_config, 16);
        cs5484_reg_read(&s_cs5484_config, &tmp, 6);  // read I1rms
	i_raw += tmp;
        cs5484_page_select(&s_cs5484_config, 16);
        cs5484_reg_read(&s_cs5484_config, &tmp, 7);  // read V1rms
	v_raw += tmp;
        printf("i_raw=%d, v_raw=%d\n", i_raw, v_raw);
    }

    i_raw = i_raw/5;
    v_raw = v_raw/5;
    printf("finally, i_raw=%d, v_raw=%d\n", i_raw, v_raw);
    cs5484_stop_conversion(&s_cs5484_config);


    /*
     * Calculate Vgain, Igain from measured raw data
     *
     */
    s_cs5484_channel_1.gain_v = (uint32_t)(gain_v * v_mte * 1.0 / (double)(FULLSCALE_OUTPUT_VOLTAGE) * 0x999999 / (double)v_raw);
    s_cs5484_channel_1.gain_i = (uint32_t)(gain_i * i_mte * 1.0 / (double)(FULLSCALE_OUTPUT_CURRENT) * 0x999999 / (double)i_raw);
    cs5484_set_gain_voltage(&s_cs5484_config, &s_cs5484_channel_1);
    cs5484_set_gain_current(&s_cs5484_config, &s_cs5484_channel_1);

    /* 
     * read gain after calibration
     *
     */
    gain_v = cs5484_get_gain_voltage(&s_cs5484_config, &s_cs5484_channel_1);
    gain_i = cs5484_get_gain_current(&s_cs5484_config, &s_cs5484_channel_1);
    printf("voltage gain (after calibration) = %x\n", gain_v);
    printf("current gain (after calibration) = %x\n", gain_i);
    delay(1000);
}
