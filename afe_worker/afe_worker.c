#include "../smartmeter_lib/cs5484_wiringpi.h"
#include "../smartmeter_lib/rtc.h"
#include "../smartmeter_lib/relay_led.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>


#define DEBUG_MODE 1



/* 
 * Struct for Smartmeter's Tags 
 * 
 */
typedef struct
{
    float i;		// current
    float v;		// voltage
    float p;		// active power
    float q;            // reactive power
    float s;            // apparent power
    float pf; 	        // power factor
    double kwh;         // energy consumption (real)
    double kVArh;       // energy consumption (complex)
    int relay_state;    // current state of latching relay

}meter_tag_t;



const char* BACKUP_FILENAME = "../backup_meter.txt";
const char* REDIS_CHANNELNAME_V     = "smartmeter.meter-1phase-01.v";
const char* REDIS_CHANNELNAME_I     = "smartmeter.meter-1phase-01.i";
const char* REDIS_CHANNELNAME_P     = "smartmeter.meter-1phase-01.P";
const char* REDIS_CHANNELNAME_Q     = "smartmeter.meter-1phase-01.Q";
const char* REDIS_CHANNELNAME_S     = "smartmeter.meter-1phase-01.S";
const char* REDIS_CHANNELNAME_PF    = "smartmeter.meter-1phase-01.PF";
const char* REDIS_CHANNELNAME_KWH   = "smartmeter.meter-1phase-01.KWH";
const char* REDIS_CHANNELNAME_KVARH = "smartmeter.meter-1phase-01.KVARH";
const char* REDIS_CHANNELNAME_RELAY_STATE = "smartmeter.meter-1phase-01.relay_state";

#define N_METER_DATAFIELD   9 // i, v, p, q, s, pf, kwh, kVArh, relay_state
#define REDIS_URL	    "127.0.0.1"
#define REDIS_PORT	    6379
#define STATUS_READ_BACKUP_OK      0
#define STATUS_READ_BACKUP_ERROR   1



int meter_wiringpi_init(cs5484_basic_config_t *s_config, relay_led_config_t *s_relay_config);
int meter_read_backup_file(FILE **backup_file, const char *filename, double *field);



/*
 * Configuration struct for measurement HAT board
 *
 */ 
cs5484_basic_config_t   s_cs5484_config;
cs5484_input_config_t   s_cs5484_channel_1;
cs5484_input_config_t   s_cs5484_channel_2;
relay_led_config_t      s_relay_config;
meter_tag_t             s_tag;



int main()
{  
    /*
     *  Read the previous value from backup file
     *
     */
    FILE *backup_file;
    double field[N_METER_DATAFIELD];
    
    meter_read_backup_file(&backup_file, BACKUP_FILENAME, field);

    uint32_t buf;
    
    // assign backup value to current value
    s_tag.i = field[0];
    s_tag.v = field[1];
    s_tag.p = field[2];
    s_tag.q = field[3];
    s_tag.s = field[4];
    s_tag.pf = field[5];
    s_tag.kwh = field[6];
    s_tag.kVArh = field[7];
    s_tag.relay_state = (int)field[8];
#if DEBUG_MODE
    printf("Read backup value, I, V, P, Q, S, PF, KWH, Relay_State :\t");
    printf("%f\t%f\t%f\t%f\t%f\t%f\t%f\t%d\r\n", s_tag.i, s_tag.v, s_tag.p, s_tag.q, s_tag.s, s_tag.pf, s_tag.kwh, s_tag.relay_state);
#endif


    /*
     * CS5484 Basic Configuration
     * Raspi
     */
    s_cs5484_config.pi_spi_channel = SPI_CHANNEL_0;
    s_cs5484_config.spi_bus_speed  = SPI_BUS_SPEED;
    s_cs5484_config.spi_mode       = SPI_MODE_CS5484;
    s_cs5484_config.spi_cs_pin     = CS_PIN;
    s_cs5484_config.spi_mosi_pin   = MOSI_PIN;
    s_cs5484_config.spi_miso_pin   = MISO_PIN;
    s_cs5484_config.spi_sck_pin    = SCK_PIN;
    s_cs5484_config.chip_reset_pin = RST_PIN;
    s_cs5484_config.csum_en = 0;
    s_cs5484_config.conversion_type = CONVERSION_TYPE_SINGLE;

    /*
     * CS5484 Input Configuration of Channel_1
     *
     */
    s_cs5484_channel_1.channel = 1;
    s_cs5484_channel_1.phase_error = 0;
    
    /*
     * CS5484 Input Configuration of Channel_2
     *
     */
    s_cs5484_channel_2.channel = 2;
    s_cs5484_channel_2.phase_error = 0;

    /*
     *  Relay & LED Configuration
     *
     */
    s_relay_config.relay_open_pin = PIN_RELAY_OPEN; 
    s_relay_config.relay_close_pin = PIN_RELAY_CLOSE;
    s_relay_config.relay_state = s_tag.relay_state; 
    s_relay_config.led_kwh_pin = PIN_LED_KWH; 
    s_relay_config.led_kvarh_pin = PIN_LED_KVARH;
    


    /*
     *  Initialize wiring-Pi and SPI interface
     *
     */
    meter_wiringpi_init(&s_cs5484_config, &s_relay_config);



    /*
     * REDIS initialize
     *
     */
    
    redisContext* c = redisConnect((char*)REDIS_URL, REDIS_PORT);
    
    // REDIS authentication
    const char* command_auth = "auth ictadmin";
    redisReply* r = (redisReply*)redisCommand(c, command_auth);

    if(!(c->err)){
        printf("redis connect OK\n");
    }
    else{
	printf("redis connect failed\n");
    }

    if(r == NULL){
    }
    if(!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str, "OK") == 0)){
        printf("Failed to execute command[%s].\n", command_auth);
    }
    else{
        printf("Succeed to execute command[%s].\n", command_auth);
    }
    


    /*
     * main loop
     *
     */
    int ret;
    while(1)
    {  
	/*
	 *  Start conversion of cs5484 
	 *  and wait until sampling is ready (polling method)
	 *
	 */
	cs5484_start_conversion(&s_cs5484_config);
        ret = cs5484_wait_for_conversion(&s_cs5484_config, 200);

	if(ret == STATUS_OK){
	    // read all param from conversion result
            s_tag.i  = cs5484_get_current_rms(&s_cs5484_config, &s_cs5484_channel_1);
            s_tag.v  = cs5484_get_voltage_rms(&s_cs5484_config, &s_cs5484_channel_1);
            s_tag.p  = cs5484_get_act_power_avg(&s_cs5484_config, &s_cs5484_channel_1);
            s_tag.q  = cs5484_get_react_power_avg(&s_cs5484_config, &s_cs5484_channel_1);
            s_tag.s  = cs5484_get_apparent_power_avg(&s_cs5484_config, &s_cs5484_channel_1);
	    s_tag.pf = cs5484_get_pf(&s_cs5484_config, &s_cs5484_channel_1);
            
	    // calculate energy
            s_tag.kwh = s_tag.kwh + (double)s_tag.p/1000.0/3600.0;
	    s_tag.kVArh = s_tag.kVArh + (double)s_tag.s/1000.0/3600.0;
	    
	    // print result
            printf("I, V, P, Q, S, PF, KWH :\t");
	    printf("%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\r\n", s_tag.i, s_tag.v, s_tag.p, s_tag.q, s_tag.s, s_tag.pf, s_tag.kwh);
        }
        else{
	    printf("Error from conversion : %d\n", ret);
	}



	/*
	 *  Read timestamp
	 *
	 */
        char timestamp[30];
	rtc_getTime(timestamp);
        printf("timestamp=%s", timestamp);


	/*
	 *  Write data to backup file
	 *
	 */
        backup_file = fopen(BACKUP_FILENAME, "r+");
	if(backup_file != NULL){
            char str_buf[200];
	    sprintf(str_buf, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%f,%f,%.1f,timestamp=%s\n", \
	            s_tag.i, s_tag.v, s_tag.p, s_tag.q, s_tag.s, s_tag.pf, s_tag.kwh, s_tag.kVArh, (float)s_tag.relay_state, timestamp);

	    fputs(str_buf, backup_file);
	    fclose(backup_file);
	}



	/*
	 *  Connect to REDIS and Set value with a CS5484's sampling
	 *
	 */
	
        redisFree(c);
        c = redisConnect((char*)REDIS_URL, REDIS_PORT);
	if(!(c->err)){
	    freeReplyObject(r);
            r = (redisReply*)redisCommand(c, command_auth);
	    if((r->type == REDIS_REPLY_STATUS) && (strcasecmp(r->str, "OK") == 0)){
	        char *channel;
	    	char value[20];
                printf("Set tag via Redis...\n\n"); 
	        
	    	channel = (char *)REDIS_CHANNELNAME_V;
	    	sprintf(value, "%.2f", s_tag.v);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	    	channel = (char *)REDIS_CHANNELNAME_I;
	    	sprintf(value, "%.2f", s_tag.i);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	   
	    	channel = (char *)REDIS_CHANNELNAME_P;
	    	sprintf(value, "%.2f", s_tag.p);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	        
	    	channel = (char *)REDIS_CHANNELNAME_Q;
	    	sprintf(value, "%.2f", s_tag.q);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);

	    	channel = (char *)REDIS_CHANNELNAME_S;
	    	sprintf(value, "%.2f", s_tag.s);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	        
		channel = (char *)REDIS_CHANNELNAME_PF;
	    	sprintf(value, "%.2f", s_tag.pf);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
                
	    	channel = (char *)REDIS_CHANNELNAME_KWH;
	    	sprintf(value, "%.5f", s_tag.kwh);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	    	
		channel = (char *)REDIS_CHANNELNAME_KVARH;
	    	sprintf(value, "%.5f", s_tag.kVArh);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
		
		channel = (char *)REDIS_CHANNELNAME_RELAY_STATE;
	    	sprintf(value, "%d", (int)s_tag.relay_state);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	    }
	    else{
		printf("Cannot auth to redis\n");
	    }
        }
	else{
            printf("Cannot connect to redis server\n");
	}
	
    }

    return 0;
}



int meter_wiringpi_init(cs5484_basic_config_t *s_config, relay_led_config_t *s_relay_config)
{
    wiringPiSetup();
    wiringPiSPISetupMode(s_config->pi_spi_channel, s_config->spi_bus_speed, s_config->spi_mode);
    
    pinMode(s_config->spi_cs_pin, OUTPUT);
    pinMode(s_config->chip_reset_pin, OUTPUT);
    pullUpDnControl(s_config->spi_cs_pin, PUD_OFF);
    pullUpDnControl(s_config->chip_reset_pin, PUD_OFF);
    digitalWrite(s_config->chip_reset_pin, 1);
    digitalWrite(s_config->spi_cs_pin, 1);
    
    relay_led_gpio_init(s_relay_config);
    led_kwh_off(s_relay_config);
    led_kvarh_off(s_relay_config);

    delay(5000);
}



int meter_read_backup_file(FILE **backup_file, const char *filename, double *field)
{
    char line[1024];
    
    *backup_file = fopen(filename, "r");
    if(*backup_file == NULL){
        printf("backup file does not exist, exist program\n");
	return STATUS_READ_BACKUP_ERROR;
    }
    else{
        fgets(line, 1024, *backup_file);
        printf("read file : %s\n", line);
    	for(int i=0; i<N_METER_DATAFIELD; i++){
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
        fclose(*backup_file);
    }

    return STATUS_READ_BACKUP_OK;
}
