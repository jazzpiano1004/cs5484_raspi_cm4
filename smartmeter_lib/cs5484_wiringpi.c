#include "cs5484_wiringpi.h"






uint8_t cs5484_reg_write(cs5484_basic_config_t *s_cs5484_config,
			uint32_t data,
			uint8_t addr)
{
    uint8_t *pdata = (uint8_t *)&data;
    uint8_t buf[5];
    uint8_t csum;

    buf[0] = 0x40 | addr;       // CMD byte
    buf[1] = pdata[2];          // Data byte (MSB)
    buf[2] = pdata[1];          // Data byte
    buf[3] = pdata[0];          // Data byte (LSB)
     
    digitalWrite(s_cs5484_config->spi_cs_pin, 0);
    wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, buf, 4);             // send CMD + write data
    
    if(s_cs5484_config->csum_en != 0){
	csum = 0xFF - (pdata[0] + pdata[1] + pdata[2]);
        wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, &csum, 1);       // send checksum after write
    }
   
    digitalWrite(CS_PIN, 1);

    return STATUS_OK;
}

uint8_t cs5484_reg_read(cs5484_basic_config_t *s_cs5484_config, 
			uint32_t *data,
			uint8_t addr)
{
    uint8_t buf[5];
    uint8_t csum;
    
    buf[0] = 0x00 | addr;
    buf[1] = 0xFF;
    buf[2] = 0xFF;
    buf[3] = 0xFF;

    digitalWrite(s_cs5484_config->spi_cs_pin, 0);
    wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, buf, 1);        // send CMD

    if(s_cs5484_config->csum_en != 0){
	csum = 0xFF - addr;
        wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, &csum, 1);  // send checksum after CMD
        wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, &buf[1], 3);  // read data
        *data = (uint32_t)(buf[3] + (8 << buf[2]) + (16 << buf[1]));
	csum = 0xFF;
        wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, &csum, 1);  // read checksum after read
        digitalWrite(s_cs5484_config->spi_cs_pin, 1);
        // checksum 	
        if(csum == (uint8_t)(0xFF - (buf[3] + buf[2] + buf[1]))){
	    return STATUS_OK;
	}
	else{
	    return STATUS_FAIL;
	}
    }
    else{
        wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, &buf[1], 3);  // read data
        digitalWrite(s_cs5484_config->spi_cs_pin, 1);
        *data = (uint32_t)(buf[3] + (buf[2] << 8) + (buf[1] << 16));
	return STATUS_OK;
    }
}

uint8_t cs5484_page_select(cs5484_basic_config_t *s_cs5484_config, uint8_t page)
{
    uint8_t buf[5];
    uint8_t csum;

    buf[0] = 0x80 | page;
    digitalWrite(s_cs5484_config->spi_cs_pin, 0);
    wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, buf, 1);  // send CMD
    if(s_cs5484_config->csum_en != 0){
	csum = 0xFF - page;
        wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, &csum, 1);  // send checksum
    }
    digitalWrite(s_cs5484_config->spi_cs_pin, 1);

    return STATUS_OK;
}

uint8_t cs5484_instruction(cs5484_basic_config_t *s_cs5484_config, uint8_t instruct_code)
{
    uint8_t buf[5];
    uint8_t csum;

    buf[0] = 0xC0 | instruct_code;
    digitalWrite(s_cs5484_config->spi_cs_pin, 0);
    wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, buf, 1);  // send CMD
    if(s_cs5484_config->csum_en != 0){
	csum = 0xFF - instruct_code;
        wiringPiSPIDataRW(s_cs5484_config->pi_spi_channel, &csum, 1);  // send checksum
    }
    digitalWrite(s_cs5484_config->spi_cs_pin, 1);

    return STATUS_OK;
}

uint8_t cs5484_reset(cs5484_basic_config_t *s_cs5484_config)
{
    return cs5484_instruction(s_cs5484_config, 0x01);
}

uint8_t cs5484_start_conversion(cs5484_basic_config_t *s_cs5484_config)
{   
    cs5484_page_select(s_cs5484_config, 0);          // set to page 0
    cs5484_reg_write(s_cs5484_config, 0x800000, 23); // clear DRDY in Status0
    
    if(s_cs5484_config->conversion_type == CONVERSION_TYPE_SINGLE){
        cs5484_instruction(s_cs5484_config, 0x14); // set single conversion
    } 
    else if(s_cs5484_config->conversion_type == CONVERSION_TYPE_CONTINUOUS){
        cs5484_instruction(s_cs5484_config, 0x15); // set continuous conversion
    }
    else return STATUS_FAIL;

    return STATUS_OK;
}

uint8_t cs5484_stop_conversion(cs5484_basic_config_t *s_cs5484_config)
{   
    cs5484_instruction(s_cs5484_config, 0x18); // set stop conversion
    cs5484_page_select(s_cs5484_config, 0);          // set to page 0
    cs5484_reg_write(s_cs5484_config, 0x800000, 23); // clear DRDY in Status0

    return STATUS_OK;
}

uint8_t cs5484_wait_for_conversion(cs5484_basic_config_t *s_cs5484_config, int timeout)
{
    uint32_t status0;
    uint8_t ret;
    int timeout_cnt=timeout;
    int ready=0;
    
    while(!ready)
    {
	ret = cs5484_reg_read(s_cs5484_config, &status0, 23);
	//printf("read status0 : %x\r\n", status0);
	if(ret == STATUS_OK)
	{
            // data ready and conversion ready flag in status0
	    if(((status0 & 0xC00000) == 0xC00000) && (status0 != 0xFFFFFF))
	    {   
	        printf("read status0 : %x\r\n", status0);
		printf("conversion data ready!\n");
                cs5484_page_select(s_cs5484_config, 0);
		cs5484_reg_write(s_cs5484_config, 0x800000, 23);
		ready = 1;		
	    }
	    else{
	        timeout_cnt--;
	    }
	}
	else{
	    timeout_cnt--;
	}

	// check timeout
	if(timeout_cnt <= 0){
	    return STATUS_TIMEOUT;
	}
	delay(10);
    }
    return STATUS_OK;
}

uint32_t cs5484_get_voltage_peak(cs5484_basic_config_t *s_cs5484_config,
				 cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t v;

    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)	addr = 36;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 38;
    else return STATUS_FAIL;

    cs5484_page_select(s_cs5484_config, 0);
    cs5484_reg_read(s_cs5484_config, &v, addr);

    return v;
}

uint32_t cs5484_get_current_peak(cs5484_basic_config_t *s_cs5484_config,
				 cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t i;

    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 37;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 39;
    else return STATUS_FAIL;

    cs5484_page_select(s_cs5484_config, 0);
    cs5484_reg_read(s_cs5484_config, &i, addr);

    return i;
}

double cs5484_get_voltage_rms(cs5484_basic_config_t *s_cs5484_config,
			      cs5484_input_config_t *s_cs5484_input_config)
{
    /*
     * Section : Read raw data from register.
     *
     */
    uint8_t addr;
    uint32_t raw;

    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 7;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 13;
    else return STATUS_FAIL;
    
    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);



    /*
     * Section : Convert raw data to actual voltage
     *
     */
    double vrms;       // voltage at voltage input of CS5484
    double vrms_line;  // AC Line voltage

    // Calculate rms voltage at input of CS5484
    //vrms  = CalFullScale(438000,0x999999,(uint32_t)raw);
    vrms = (double)raw / FULLSCALE_RAWDATA_VOLTAGE * FULLSCALE_INPUT_VOLTAGE / sqrt(2);

    // Calculate AC line voltage
    vrms_line = vrms * R_DIVIDER_GAIN_INVERSE;

    return vrms_line;
}

double cs5484_get_current_rms(cs5484_basic_config_t *s_cs5484_config,
			      cs5484_input_config_t *s_cs5484_input_config)
{
    /*
     * Section : Read raw data from register.
     *
     */
    uint8_t addr;
    uint32_t raw;

    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 6;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 12;
    else return STATUS_FAIL;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);

#if DEBUG_CS5484
    printf("current (raw) : %d\n", raw);
#endif

    /*
     * Section : Convert raw data to actual current
     *
     */
    double vrms;       // voltage at current input of CS5484
    double irms_line;  // AC Line voltage
    
    // Calculate rms voltage at input of CS5484
    //i  = CalFullScale(150000,0x999999,(uint32_t)raw);
    vrms = (double)raw / FULLSCALE_RAWDATA_CURRENT * FULLSCALE_INPUT_VOLTAGE / sqrt(2);
    
    // Calculate Load current
    irms_line = CT_RATIO * vrms / R_BURDEN;

    return irms_line;
}

double cs5484_get_act_power_avg(cs5484_basic_config_t *s_cs5484_config,
				cs5484_input_config_t *s_cs5484_input_config)
{
    /*
     * Section : Read raw data from register.
     *
     */
    uint8_t addr;
    uint32_t raw;

    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 5;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 11;
    else return STATUS_FAIL;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);
    
#if DEBUG_CS5484
    printf("active power (raw) : %d\n", raw);
#endif



    /*
     * Section : Convert raw data to actual power
     *
     */
    double p;         // real power
    long raw_4byte;   // buffer for 2s complement conversion
    
    // Detect sign bit of 2s compliment value
    //p  = CalPow(convert3byteto4byte(raw));
    if(raw & 0x800000){
        raw_4byte = (long)(raw - 0x1000000);	
    }
    else{
	raw_4byte = (long)raw;
    }

    // Calculate power at output terminal
    p = ((double)(raw_4byte) * FULLSCALE_OUTPUT_POWER) / FULLSCALE_RAWDATA_POWER;

    return p;
}

double cs5484_get_react_power_avg(cs5484_basic_config_t *s_cs5484_config,
				  cs5484_input_config_t *s_cs5484_input_config)
{
    /*
     * Section : Read raw data from register.
     *
     */
    uint8_t addr;
    uint32_t raw;

    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 14;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 16;
    else return STATUS_FAIL;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);

#if DEBUG_CS5484
    printf("reactive power (raw) : %d\n", raw);
#endif

    /*
     * Section : Convert raw data to actual power
     *
     */
    double q;         // reactive power
    long raw_4byte;   // buffer for 2s complement conversion
    
    // Detect sign bit of 2s compliment value
    //q = CalPow(convert3byteto4byte(raw));
    if(raw & 0x800000){
        raw_4byte = (long)(raw - 0x1000000);	
    }
    else{
	raw_4byte = (long)raw;
    }

    // Calculate power at output terminal
    q = ((double)(raw_4byte) * FULLSCALE_OUTPUT_POWER) / FULLSCALE_RAWDATA_POWER;

    return q;
}

double cs5484_get_apparent_power_avg(cs5484_basic_config_t *s_cs5484_config,
				     cs5484_input_config_t *s_cs5484_input_config)
{
    /*
     * Section : Read raw data from register.
     *
     */
    uint8_t addr;
    uint32_t raw;

    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 20;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 24;
    else return STATUS_FAIL;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);



    /*
     * Section : Convert raw data to actual power
     *
     */
    double s;    // apparent power (real + complex)

    //s = CalPow(convert3byteto4byte(raw));
    // Calculate power at output terminal
    s = ((double)(raw) * FULLSCALE_OUTPUT_POWER) / FULLSCALE_RAWDATA_POWER;

    return s;
}

double cs5484_get_pf(cs5484_basic_config_t *s_cs5484_config,
		     cs5484_input_config_t *s_cs5484_input_config)
{ 
    /*
     * Section : Read raw data from register.
     *
     */
    uint8_t addr;
    uint32_t raw;

    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 21;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 25;
    else return STATUS_FAIL;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);



    /*
     * Section : Convert raw data to actual power factor
     *
     */
    double pf;       // power
    long raw_4byte;

    // Detect sign bit of 2s compliment value
    //pf = CalPF(convert3byteto4byte(raw));
    if(raw & 0x800000){
        raw_4byte = (long)(raw - 0x1000000);	
    }
    else{
	raw_4byte = (long)raw;
    }
    
    // calculate power factor at output terminal
    pf = (double)(((uint64_t)abs(raw_4byte)*10000) / 0x7FFFFF) / 100;
    
#if DEBUG_CS5484
    printf("PF (raw) : %x\n", raw);
#endif

    return pf;
}

uint8_t cs5484_temperature_enable(cs5484_basic_config_t * s_cs5484_config, int enable_flag)
{
    /*
     * Section : Read previous configuration from Config0 register.
     *
     */
    uint32_t config_previous;
    uint8_t ret;
    cs5484_page_select(s_cs5484_config, 0);
    ret = cs5484_reg_read(s_cs5484_config, &config_previous, 0);
    if(ret != STATUS_OK) return STATUS_FAIL;


    /*
     * Section : Enable temperature sensor with Config0
     *
     */
    uint32_t config_update;

    if(enable_flag != 0){
        config_update = (config_previous & ~(0x806000)) | 0x802000;
    }
    else{
        config_update = (config_previous & ~(0x806000));
    }
    
    cs5484_page_select(s_cs5484_config, 0);
    cs5484_reg_write(s_cs5484_config, config_update, 0);
    
    return STATUS_OK;
}

int8_t cs5484_is_temperature_ready(cs5484_basic_config_t *s_cs5484_config)
{
    /*
     * Section : Read temperature status from Status0 register.
     *
     */
    uint32_t status0;
    uint8_t tup_bit=5;

    cs5484_page_select(s_cs5484_config, 0);
    cs5484_reg_read(s_cs5484_config, &status0, 23);

    if((status0 & (1 << tup_bit)) != 0)   return 1;
    else   return 0;
}

double cs5484_get_temperature(cs5484_basic_config_t *s_cs5484_config)
{
    /*
     * Section : Read raw data from register.
     *
     */
    uint32_t raw;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, 27);


    /*
     * Conversion of temperature value (fixed-point) into Celsius unit
     *
     */
    double temperature;
    temperature = (double)((int32_t)raw) / 65536;

    /*
     * Correct value by using pre-calibrated profile
     * temperature (self-correct) = temperature
     */
    double m=TEMPSENSOR_CALIBRATE_COEFF;
    double b=TEMPSENSOR_CALIBRATE_OFFSET;
    temperature = (temperature/m) - (b/m); 

    return temperature;
}

uint8_t cs5484_set_phase_compensation(cs5484_basic_config_t *s_cs5484_config,
				      cs5484_input_config_t *s_cs5484_input_config)
{
    /* 
     * Convert adjust angle to value of CPCC, FPCC register value
     *
     */
    uint16_t CPCC = 0;
    uint16_t FPCC = 0;
    float phase_error;
    int error;

    phase_error = s_cs5484_input_config->phase_error;
    error = (int)(phase_error * 100);

    // Check if phase error is in the range of be able to compensated
    if(error > 899 || error < -899) return STATUS_FAIL;

    // Before compensate, i lags v (phase error +)
    if(error > 0){
        // Range between 0 to 4.5
	if(error < 450){
	    CPCC = 2;
	    FPCC = 0x1FF & (uint16_t)abs((int)((4.5 - phase_error)/0.008789));
	}
        // Range between 4.5 to 8.99
	else{
	    CPCC = 3;
	    FPCC = 0x1FF & (uint16_t)abs((int)((8.99 - phase_error)/0.008789));
	}
    }

    // Before compensate, i leads the v (phase error -)
    else{
        // Range between 0 to -4.5
        if(error > -450){
	    CPCC = 0;
	    FPCC = 0x1FF & (uint16_t)abs((int)(phase_error/0.008789));
	}
        // Range between -4.5 to -8.99
	else{
	    CPCC = 1;
	    FPCC = 0x1FF & (uint16_t)abs((int)((phase_error + 4.5)/0.008789));
	}
    }
    printf("CPCC=%d, FPCC=%d\n", CPCC, FPCC);



    /*
     * Section : Write compensate configuration to register.
     *
     */
    uint32_t raw;
    uint32_t previous_config=0;
    
    // Read a previous config value
    cs5484_page_select(s_cs5484_config, 0);
    cs5484_reg_read(s_cs5484_config, &previous_config, 5);
    
    // Check which channel will be configurated
    if(s_cs5484_input_config->channel == 1){
        raw = (previous_config & 0xCFFE00) | (CPCC << 20) | FPCC;
    }
    else if(s_cs5484_input_config->channel == 2){
        raw = (previous_config & 0x3C01FF) | (CPCC << 22) | (FPCC << 9);
    }

    cs5484_page_select(s_cs5484_config, 0);
    cs5484_reg_write(s_cs5484_config, raw, 5);

    return STATUS_OK;
}



uint8_t cs5484_set_offset_act_power(cs5484_basic_config_t *s_cs5484_config, 
		                    cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set offset for Pavg
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 36;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 43;
    else return STATUS_FAIL;
    
    raw = s_cs5484_input_config->offset_p;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_write(s_cs5484_config, raw, addr);

    return STATUS_OK;
}

uint32_t cs5484_get_offset_act_power(cs5484_basic_config_t *s_cs5484_config, 
	 	                     cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set offset for Pavg
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 36;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 43;
    else return STATUS_FAIL;
    
    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);

    return raw;
}


uint8_t cs5484_set_offset_react_power(cs5484_basic_config_t *s_cs5484_config, 
		                      cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set offset for Qavg
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 38;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 45;
    else return STATUS_FAIL;
    
    raw = s_cs5484_input_config->offset_q;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_write(s_cs5484_config, raw, addr);

    return STATUS_OK;
}

uint32_t cs5484_get_offset_react_power(cs5484_basic_config_t *s_cs5484_config, 
		                       cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set offset for Pavg
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 38;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 45;
    else return STATUS_FAIL;
    
    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);

    return raw;
}

uint8_t cs5484_set_offset_current(cs5484_basic_config_t *s_cs5484_config, 
		                  cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set AC offset for current (I_ACoffset)
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 37;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 44;
    else return STATUS_FAIL;
    
    raw = s_cs5484_input_config->ac_offset_i;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_write(s_cs5484_config, raw, addr);

    return STATUS_OK;
}

uint32_t cs5484_get_offset_current(cs5484_basic_config_t *s_cs5484_config, 
	   	                    cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set AC offset for current (I_ACoffset)
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 37;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 44;
    else return STATUS_FAIL;
    
    raw = s_cs5484_input_config->ac_offset_i;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);

    return raw;
}

uint8_t cs5484_set_gain_voltage(cs5484_basic_config_t *s_cs5484_config, 
		                cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set gain for voltage (V_gain)
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 35;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 42;
    else return STATUS_FAIL;
    
    //raw = (uint32_t)((s_cs5484_input_config->gain_v) * 0x400000);
    raw = s_cs5484_input_config->gain_v;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_write(s_cs5484_config, raw, addr);
    
    return STATUS_OK;
}

uint32_t cs5484_get_gain_voltage(cs5484_basic_config_t *s_cs5484_config, 
		                cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set gain for voltage (V_gain)
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 35;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 42;
    else return STATUS_FAIL;
    
    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);
    
    //return (double)(raw) / 0x400000;
    return raw;
}

uint8_t cs5484_set_gain_current(cs5484_basic_config_t *s_cs5484_config, 
		                cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set gain for current (I_gain)
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 33;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 40;
    else return STATUS_FAIL;
    
    //raw = (uint32_t)((s_cs5484_input_config->gain_i) * 0x400000);
    raw = s_cs5484_input_config->gain_i;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_write(s_cs5484_config, raw, addr);

    return STATUS_OK;
}

uint32_t cs5484_get_gain_current(cs5484_basic_config_t *s_cs5484_config, 
		                cs5484_input_config_t *s_cs5484_input_config)
{
    uint8_t addr;
    uint32_t raw;

    /*
     * Section : Set gain for current (I_gain)
     *
     */
    if(s_cs5484_input_config->channel == ANALOG_INPUT_CH1)   	addr = 33;
    else if(s_cs5484_input_config->channel == ANALOG_INPUT_CH2)	addr = 40;
    else return STATUS_FAIL;
    
    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &raw, addr);

    //return (double)(raw) / 0x400000;
    return raw;
}

uint8_t cs5484_input_filter_init(cs5484_basic_config_t *s_cs5484_config,
		                 cs5484_input_config_t *s_cs5484_input_config)
{
    uint32_t raw;
    uint32_t previous_config=0;
    uint32_t bitmask=0;
    uint8_t  shift=0;

    // Read a previous config value of Config2
    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_read(s_cs5484_config, &previous_config, 0);
    
    // check channel
    if(s_cs5484_input_config->channel == 1){
        bitmask = ~(0x01E);
	shift = 0;
    }
    else if(s_cs5484_input_config->channel == 2){
        bitmask = ~(0x1E0);
	shift = 4;
    }
    else{
	return STATUS_FAIL;
    }
    
    // check filter mode, create byte for configuration
    uint8_t filter_bit=0;

    if(s_cs5484_input_config->filter_mode_v == FILTER_MODE_HPF){
        filter_bit = filter_bit | 1;
    }
    if(s_cs5484_input_config->filter_mode_i == FILTER_MODE_HPF){
        filter_bit = filter_bit | 3;
    }
    if(s_cs5484_input_config->filter_mode_v == FILTER_MODE_PMF){
        filter_bit = filter_bit | 2;
    }
    if(s_cs5484_input_config->filter_mode_v == FILTER_MODE_PMF){
        filter_bit = filter_bit | 10;
    }
    
    // write filter config
    raw = (previous_config & bitmask) | (filter_bit << shift);
    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_write(s_cs5484_config, raw, 0);

    return STATUS_OK;
}

uint8_t cs5484_set_settlingtime(cs5484_basic_config_t *s_cs5484_config)
{
    uint32_t raw;
    
    raw = s_cs5484_config->t_settling;

    cs5484_page_select(s_cs5484_config, 16);
    cs5484_reg_write(s_cs5484_config, raw, 57);

    return STATUS_OK;
}

uint8_t cs5484_send_calibration_cmd_gain(cs5484_basic_config_t *s_cs5484_config)
{   
    uint8_t calibration_cmd;
    calibration_cmd = (7 << 3) | 6;
    cs5484_instruction(s_cs5484_config, calibration_cmd);

    return STATUS_OK;
}

uint8_t cs5484_send_calibration_cmd_offset(cs5484_basic_config_t *s_cs5484_config)
{   
    uint8_t calibration_cmd;
    calibration_cmd = (6 << 3) | 1;
    cs5484_instruction(s_cs5484_config, calibration_cmd);

    return STATUS_OK;
}

/* 
 * Calibration Ref. from ver2015
 */
/*

//mte_rms fix 2 point 23000 = 230.00
static u32 NewGain(int gain,int mte_rms, int meter_reg_rms,int fullscale)
{
	//correcting factor
	return	(u32)(gain *(mte_rms * 1.0/fullscale * 0x999999/meter_reg_rms));
}

static void Calibrate2(afe_context_t* afe,u32 avg, u32 mte_rms, u32 fullscale, u32 gain_addr)
{
	afe_interface_context_t* inf = &afe->interface;
	u32 gain,new_gain;

	afe_inf_read(inf , 16 , gain_addr , (unsigned char*)&gain, 1);//Volt
	new_gain = NewGain( gain, mte_rms , avg, fullscale);
	afe_inf_write(inf, 16 , gain_addr , new_gain, 1);

}

usage :
v1 is from V1rms register value
Calibrate2(&meter.afe0, v1, volt, VOLTAGE1_FULLSCALE, 35);
Calibrate2(&meter.afe0, i1, amp,  CURRENT1_FULLSCALE, 33);
*/
