#include "../smartmeter_lib/cs5484_wiringpi.h"
#include "../smartmeter_lib/ct_model.h"
#include "../smartmeter_lib/rtc.h"
#include "../smartmeter_lib/relay_led.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <pthread.h>



// FLag for worker debugging
#define DEBUG_WORKER   1
#define DATALOGGING_TEMPERATURE_COMPENSATION_TEST    1
#define DATALOGGING_ENERGY_PULSE_TEST                0


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
    double kvarh;       // energy consumption (complex)
    double wh;         // energy consumption (real)
    double varh;       // energy consumption (complex)
    int relay_state;   // current state of latching relay

}meter_tag_t;



const char* BACKUP_FILENAME = "../backup_meter.txt";
const char* PULSE_LOG_FILENAME = "../pulse_log.txt";
const char* CALIBRATION_FILENAME = "../calibration/calibration_output.txt";
const char* REDIS_CHANNELNAME_V     = "smartmeter-raspi.meter-1phase-01.v";
const char* REDIS_CHANNELNAME_I     = "smartmeter-raspi.meter-1phase-01.i";
const char* REDIS_CHANNELNAME_P     = "smartmeter-raspi.meter-1phase-01.P";
const char* REDIS_CHANNELNAME_Q     = "smartmeter-raspi.meter-1phase-01.Q";
const char* REDIS_CHANNELNAME_S     = "smartmeter-raspi.meter-1phase-01.S";
const char* REDIS_CHANNELNAME_PF    = "smartmeter-raspi.meter-1phase-01.PF";
const char* REDIS_CHANNELNAME_KWH   = "smartmeter-raspi.meter-1phase-01.KWH";
const char* REDIS_CHANNELNAME_KVARH = "smartmeter-raspi.meter-1phase-01.KVARH";
const char* REDIS_CHANNELNAME_RELAY_WRITE = "smartmeter-raspi.meter-1phase-01.relay_write";

#define N_METER_DATAFIELD   9 // i, v, p, q, s, pf, kwh, kvarh, relay_state
#define REDIS_URL	    "redis"
#define REDIS_PORT	    6379
#define STATUS_READ_BACKUP_OK      0
#define STATUS_READ_BACKUP_ERROR   1

#define TEMPERATURE_LOG_FILENAME  "temperature_log.txt"

#define PULSE_PER_KWH       2000
#define PULSE_PER_KVARH     2000
#define PULSE_PER_WH        ((double)PULSE_PER_KWH/1000.0)
#define PULSE_PER_VARH      ((double)PULSE_PER_KVARH/1000.0)
#define ENERGY_DIFF_MUL     1000000
#define PULSE_LOG_FILENAME  "pulse_log.txt"

int meter_wiringpi_init(cs5484_basic_config_t *s_config, relay_led_config_t *s_relay_config);
int meter_chipmeasurement_init(cs5484_basic_config_t *s_cs5484_config,
		               cs5484_input_config_t *s_cs5484_channel_1,
			       cs5484_input_config_t *s_cs5484_channel_2);
int meter_read_backup_file(FILE **backup_file, const char *filename, double *field);
void meter_load_calibration(FILE **calibration_file, const char *filename, 
		            cs5484_input_config_t *s_cs5484_channel_1);
int meter_check_chipconfig(cs5484_basic_config_t *s_cs5484_config, uint32_t ref_config0);

void connectCallback(const redisAsyncContext *c, int status);
void disconnectCallback(const redisAsyncContext *c, int status);
void onMessage(redisAsyncContext * c, void *reply, void * privdata);
void getCallback(redisAsyncContext *c, void *r, void *privdata);



/*
 * Configuration struct for measurement HAT board
 *
 */ 
cs5484_basic_config_t   s_cs5484_config;
cs5484_input_config_t   s_cs5484_channel_1;
cs5484_input_config_t   s_cs5484_channel_2;
ct_profile_t            s_ct_profile;
relay_led_config_t      s_relay_config;
meter_tag_t             s_tag;

/*
 * Global var for meter
 *
 */
int redis_connected = 0;
int meter_run_flag = 0;
uint32_t meter_user_config0;
uint32_t meter_user_config1;
uint32_t meter_user_config2;



void *thread_measurement(void *arg)
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
    s_tag.kvarh = field[7];
    s_tag.wh = s_tag.kwh * 1000;
    s_tag.varh = s_tag.kvarh * 1000;
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
    s_cs5484_config.conversion_type = CONVERSION_TYPE_CONTINUOUS;

    /*
     * CS5484 Input Configuration of Channel_1
     *
     */
    s_cs5484_channel_1.channel = 1;
    s_cs5484_channel_1.filter_mode_v = FILTER_MODE_HPF;
    s_cs5484_channel_1.filter_mode_i = FILTER_MODE_HPF;
    s_cs5484_channel_1.phase_error = -4.8;
    s_cs5484_channel_1.ac_offset_i = 0;
    s_cs5484_channel_1.offset_p = 0;
    s_cs5484_channel_1.offset_q = 0;
    
    /*
     * CS5484 Input Configuration of Channel_2
     *
     */
    s_cs5484_channel_2.channel = 2;
    s_cs5484_channel_2.filter_mode_v = FILTER_MODE_DISABLE;
    s_cs5484_channel_2.filter_mode_i = FILTER_MODE_DISABLE;
    s_cs5484_channel_2.phase_error = 0;
    s_cs5484_channel_2.ac_offset_i = 0;
    s_cs5484_channel_2.offset_p = 0;
    s_cs5484_channel_2.offset_q = 0;

    /*
     * CT profile
     *
     */
    s_ct_profile.model_type = CT_MODELTYPE_LINEAR_REGRESSION;
    s_ct_profile.coeffs[0] = 0.0122614;
    s_ct_profile.coeffs[1] = -0.00016122;
    s_ct_profile.offset = 4.416743;

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
     *  Initialize wiring-Pi and SPI interface, CS5484 Chip, Load calibration
     *
     */
    FILE *calibration_file;
    meter_wiringpi_init(&s_cs5484_config, &s_relay_config);
    meter_load_calibration(&calibration_file, CALIBRATION_FILENAME, &s_cs5484_channel_1); // must call this before chipmeasurement_init().
    meter_chipmeasurement_init(&s_cs5484_config, &s_cs5484_channel_1, &s_cs5484_channel_2);


    /*
     * REDIS Sync Initialize
     * This client will be used for Redis's "SET" command
     *
     */
    redisContext* c = redisConnect((char*)REDIS_URL, REDIS_PORT);
    
    // REDIS authentication
    const char* command_auth = "auth ictadmin";
    redisReply* r = (redisReply*)redisCommand(c, command_auth);

    if(!(c->err)){
        printf("redis sync connect OK\n");
    }
    else{
	printf("redis sync connect failed\n");
    }
    if(r == NULL){
    }
    if(!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str, "OK") == 0)){
        printf("redis sync, Failed to execute command[%s].\n", command_auth);
    }
    else{
        printf("redis sync, Succeed to execute command[%s].\n", command_auth);
    }
    


    /*
     * main loop
     *
     */
    int ret;
    int conversion_error_count = 0;
    int measure_first_sample_flag = 0;
    rtc_tick_t s_timer;
    double timeInterval;
    double temperature;
    int phase_compensate_tick=0;
    int set_analysetag_tick=0;

    meter_run_flag = 1;  // start measurement

    while(meter_run_flag)
    {  
	/*
	 *  Wait until config0 of cs5484 is corrected
	 */
        while(meter_check_chipconfig(&s_cs5484_config, meter_user_config0) != STATUS_OK)
	{   
            meter_chipmeasurement_init(&s_cs5484_config, &s_cs5484_channel_1, &s_cs5484_channel_2);
	}



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
            
	    // measure the time interval between previous sampling and current sampling 
	    if(measure_first_sample_flag == 0)
	    {
	       timeInterval = 1.0;
	       measure_first_sample_flag = 1;
	    }
	    else
	    {
	       rtc_tickEnd(&s_timer); // stop tick
	       timeInterval = rtc_getPerformTime(&s_timer);
	    }
	    rtc_tickStart(&s_timer); // re-start tick
            
	    // Calculate energy kWh, kVARh
	    double dwh;
	    double dvarh;
            dwh = (double)s_tag.p/3600.0 * timeInterval;
            dvarh = (double)s_tag.q/3600.0 * timeInterval;
	    s_tag.wh   = s_tag.wh + dwh;
	    s_tag.varh = s_tag.varh + dvarh; 
            s_tag.kwh  = s_tag.kwh + dwh/1000.0;
	    s_tag.kvarh = s_tag.kvarh + dvarh/1000.0;
	    
	    // Checking if temperature config fail
	    int timeout=0;
	    while(!cs5484_is_temperature_ready(&s_cs5484_config) && !(timeout > 2000)){
	        // wait for TUP, do nothing
		// It should be better if interrupt is used in this aplication. 
		timeout++;
	    }
	    if(timeout > 2000){
		// reinitialize chip
                printf("chip config lost! re-init chip...\n");
		meter_chipmeasurement_init(&s_cs5484_config, &s_cs5484_channel_1, &s_cs5484_channel_2);
	    }
	    else{
		// Read temperature if config is still OK
		temperature = cs5484_get_temperature(&s_cs5484_config);

		// Phase compensation for every 30 seconds
		phase_compensate_tick++;
		if(phase_compensate_tick >= 30)
		{
		    // Read how much CT's phase error
		    double phase_error;
                    phase_error = ct_get_phase_error(&s_ct_profile, temperature, s_tag.i);
		    printf("******************\n**************** current phase error = %lf\n", phase_error);

		    // Update phase error and set compensated value
                    s_cs5484_channel_1.phase_error = -1 * phase_error;
                    cs5484_set_phase_compensation(&s_cs5484_config, &s_cs5484_channel_1);

		    // reset tick
		    phase_compensate_tick = 0;
		}
	    }

	    // print result
            printf("meter temperature :%f\n", temperature);
            printf("I, V, P, Q, S, PF, KWH :\t");
	    printf("%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\r\n", s_tag.i, s_tag.v, s_tag.p, s_tag.q, s_tag.s, s_tag.pf, s_tag.kwh);
        }
        else{
	    printf("Error from conversion : %d\n", ret);
#if DEBUG_WORKER
            conversion_error_count++;
#endif
	}
#if DEBUG_WORKER
	printf("error count : %d\n", conversion_error_count);
#endif

	/*
	 *  Read timestamp
	 *
	 */
        char timestamp[30];
	rtc_getTime(timestamp);
        printf("timestamp=%s\n", timestamp);

	/*
	 *  Write data to backup file
	 *
	 */
        backup_file = fopen(BACKUP_FILENAME, "r+");
	if(backup_file != NULL){
            char str_buf[200];
	    sprintf(str_buf, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%f,%f,%.1f,timestamp=%s\n", \
	            s_tag.i, s_tag.v, s_tag.p, s_tag.q, s_tag.s, s_tag.pf, s_tag.kwh, s_tag.kvarh, (float)s_tag.relay_state, timestamp);

	    fputs(str_buf, backup_file);
	    fclose(backup_file);
	}
	
	
#if DATALOGGING_TEMPERATURE_COMPENSATION_TEST
        FILE *temperature_log_file;
	/*
	 *  Stamp blinking to log file
	 *
	 */
        temperature_log_file = fopen(TEMPERATURE_LOG_FILENAME, "a");
	if(temperature_log_file != NULL){
           char str_buf[200];
	   sprintf(str_buf, "%s,%.3f,%.3f,%.3f\n", \
		   timestamp, temperature, s_tag.i, s_tag.pf);

	   fputs(str_buf, temperature_log_file);
	   fclose(temperature_log_file);
        }
#endif

	
	/*
	 *  REDIS Set value with a CS5484's sampling
	 *
	 */
        redisFree(c);
        c = redisConnect((char*)REDIS_URL, REDIS_PORT);
	/*
	 *  REDIS Set value with a CS5484's sampling
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
	    	sprintf(value, "%.3f", s_tag.v);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	    	channel = (char *)REDIS_CHANNELNAME_I;
	    	sprintf(value, "%.3f", s_tag.i);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	   
	    	channel = (char *)REDIS_CHANNELNAME_P;
	    	sprintf(value, "%.3f", s_tag.p/1000);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	        
	    	channel = (char *)REDIS_CHANNELNAME_Q;
	    	sprintf(value, "%.3f", s_tag.q/1000);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);

	    	channel = (char *)REDIS_CHANNELNAME_S;
	    	sprintf(value, "%.3f", s_tag.s/1000);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	        
		channel = (char *)REDIS_CHANNELNAME_PF;
	    	sprintf(value, "%.2f", s_tag.pf);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
                if(set_analysetag_tick >= 10)
		{	
	    	    channel = (char *)REDIS_CHANNELNAME_KWH;
	    	    sprintf(value, "%.3f", s_tag.kwh);
		    freeReplyObject(r);
            	    r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	    	
		    channel = (char *)REDIS_CHANNELNAME_KVARH;
	    	    sprintf(value, "%.3f", s_tag.kvarh);
		    freeReplyObject(r);
            	    r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
		
		    channel = (char *)REDIS_CHANNELNAME_RELAY_WRITE;
	     	    sprintf(value, "%d", (int)s_tag.relay_state);
	 	    freeReplyObject(r);
            	    r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
		   								     , channel, timestamp);
		    set_analysetag_tick = 0;
		}
		else{
		    set_analysetag_tick++;
		}
	    }
	    else{
		printf("Cannot auth to redis\n");
	    }
        }
	else{
            printf("Cannot connect to redis server\n");
	}
    }
}

void *thread_redis_subscribe(void *arg)
{
    /*
     * REDIS Async Initialize
     *
     */  
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();

    // Create Redis connect configuration
    redisOptions options = {0};
    REDIS_OPTIONS_SET_TCP(&options, "127.0.0.1", 6379);
    struct timeval tv = {0};
    tv.tv_sec = 1;
    options.connect_timeout = &tv;
    
    /*
     *  REDIS Async Connect
     *
     */
    redisAsyncContext *c = redisAsyncConnectWithOptions(&options);
    if (c->err) {
        /* Let *c leak for now... */
        printf("Error: %s\n", c->errstr);
        return (void *)1;
    }
    redisLibeventAttach(c,base);
    
    // REDIS Connect, Disconnect Callback
    redisAsyncSetConnectCallback(c, connectCallback);
    redisAsyncSetDisconnectCallback(c, disconnectCallback);

    /*
     *  REDIS Authentication
     *
     */
    printf("Send Authentication CMD\n");
    redisAsyncCommand(c, getCallback, NULL, "auth ictadmin");

    /*
     *  REDIS Subscribe
     *  This will go on forever
     */
    char *channel = (char *)REDIS_CHANNELNAME_RELAY_WRITE;
    printf("Send Subscribe for relay tag\n");
    redisAsyncCommand(c, onMessage, NULL, "SUBSCRIBE tag:%s", channel);
    event_base_dispatch(base);
}

void *thread_watthour_pulse(void *arg)
{
    double wh_previous;
    double varh_previous;
    int diff_wh;
    int diff_varh;
    int pulse_count_kwh=0;
    int pulse_count_kvarh=0;
    FILE *pulse_log_file;

    while(!meter_run_flag){
	// wait until meter start to run and start conversion 
    }

    // initialize previous energy value
    wh_previous   = s_tag.wh;
    varh_previous = s_tag.varh;

    while(meter_run_flag){
	diff_wh   = (int)((s_tag.wh - wh_previous) * ENERGY_DIFF_MUL);     // difference wh multiply by constant 
	diff_varh = (int)((s_tag.varh - varh_previous) * ENERGY_DIFF_MUL); // difference varh multiply by constant
        	
	// check pulse for kwh
	if(diff_wh  >= (int)(ENERGY_DIFF_MUL / PULSE_PER_WH)){
#if DEBUG_WORKER
            pulse_count_kwh++;
	    printf("blink kWh!, pulse cnt (kWh)=%d\n", pulse_count_kwh);
#endif
	    wh_previous = s_tag.wh;
	    led_kwh_pulse(&s_relay_config);

#if DATALOGGING_ENERGY_PULSE_TEST
	    /*
	     *  Stamp blinking to log file
	     *
	     */
            char timestamp[30];
	    rtc_getTime(timestamp);
            pulse_log_file = fopen(PULSE_LOG_FILENAME, "a");
	    if(pulse_log_file != NULL){
               char str_buf[200];
	       sprintf(str_buf, "blink kWh!. w=%f, pulse_kwh=%d, pulse_kvarh=%d,timestamp=%s\n", 
		       s_tag.p, pulse_count_kwh, pulse_count_kvarh, timestamp);

	       fputs(str_buf, pulse_log_file);
	       fclose(pulse_log_file);
	   }
#endif
        }

	// check pulse for kvarh
	if(diff_varh  >= (int)(ENERGY_DIFF_MUL / PULSE_PER_VARH)){
#if DEBUG_WORKER
            pulse_count_kvarh++;
	    printf("blink kVARh!, pulse cnt (kVARh)=%d\n", pulse_count_kvarh);
#endif
	    varh_previous = s_tag.varh;
            led_kvarh_pulse(&s_relay_config);
	    
#if DATALOGGING_ENERGY_PULSE_TEST
	    /*
	     *  Stamp blinking to log file
	     *
	     */
            char timestamp[30];
	    rtc_getTime(timestamp);
            pulse_log_file = fopen(PULSE_LOG_FILENAME, "a");
	    if(pulse_log_file != NULL){
               char str_buf[200];
	       sprintf(str_buf, "blink kVARh!. pulse_kwh=%d, pulse_kvarh=%d,timestamp=%s\n", pulse_count_kwh, pulse_count_kvarh, timestamp);
	       fputs(str_buf, pulse_log_file);
	       fclose(pulse_log_file);
	    }
#endif
	}
	delay(1);
    }
}	



int main(){
    /*
     *  Create threads
     *  - Thread for measurement loop
     *  - Thread for Redis subscribe callback in Relay Control & Other write method by client
     *  - Thread for meter pulse 
     */
    pthread_t t_measure;
    pthread_t t_redis_sub;
    pthread_t t_pulse;
    pthread_create(&t_measure,   NULL, &thread_measurement,     NULL);
    pthread_create(&t_redis_sub, NULL, &thread_redis_subscribe, NULL);
    pthread_create(&t_pulse,     NULL, &thread_watthour_pulse,  NULL);

    // join all thread
    pthread_join(t_measure,   NULL);
    pthread_join(t_redis_sub, NULL);
    pthread_join(t_pulse, NULL);

    return 0;
}



////////////////////////////////////////////////// User Function ////////////////////////////////////////////////////
int meter_wiringpi_init(cs5484_basic_config_t *s_config, relay_led_config_t *s_relay_config)
{
    // WiringPi setup
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
    delay(100);
    digitalWrite(s_config->chip_reset_pin, 1);

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
    cs5484_set_phase_compensation(s_cs5484_config, s_cs5484_channel_1);
    
    // record user config for config0, config1, config2 for chip config loss diagnose
    cs5484_page_select(s_cs5484_config, 0);
    cs5484_reg_read(s_cs5484_config, &meter_user_config0, 0);

    return 0;
}

int meter_check_chipconfig(cs5484_basic_config_t *s_cs5484_config, uint32_t ref_config0)
{
    uint32_t previous_config0;

    // Read a previous config value of Config0
    cs5484_page_select(s_cs5484_config, 0);
    cs5484_reg_read(s_cs5484_config, &previous_config0, 0);
    
    if(previous_config0 != ref_config0) return STATUS_FAIL;
    else return STATUS_OK;
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

void meter_load_calibration(FILE **calibration_file, const char *filename, 
		            cs5484_input_config_t *s_cs5484_channel_1)
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

    s_cs5484_channel_1->gain_v = field[0];
    s_cs5484_channel_1->gain_i = field[1];
    s_cs5484_channel_1->ac_offset_i = field[2];
    s_cs5484_channel_1->offset_p = field[3];
    s_cs5484_channel_1->offset_q = field[4];
}



/*
 * Redis Callback Functions
 *
 */

void connectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("Error: %s\n", c->errstr);
        return;
    }
    printf("REDIS async Connected...\n");
}

void disconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("Error: %s\n", c->errstr);
        return;
    }
    printf("REDIS async Disconnected...\n");
}

void onMessage(redisAsyncContext * c, void *reply, void * privdata) {
    int j;
    redisReply * r = reply;
    if (reply == NULL) return;

    printf("got a message of type: %i\n", r->type);
    if (r->type == REDIS_REPLY_ARRAY) {
        for (j = 0; j < r->elements; j++) {
            printf("%u) %s\n", j, r->element[j]->str);
          
	    // Extract write value of relay control from client
            int write_val = 0;
	    if((j == r->elements - 1) && (r->element[j]->str != NULL)){
                write_val = atoi(r->element[j]->str);
                if(write_val == RELAY_STATE_DISCONNECT){
	            printf("Disconnect latching relay\n");
                    relay_disconnect(&s_relay_config);
		    s_tag.relay_state = RELAY_STATE_DISCONNECT;
                }
                else{
	            printf("Connect latching relay\n");
                    relay_connect(&s_relay_config);
		    s_tag.relay_state = RELAY_STATE_CONNECT;
                }
	    }
        }
    }
}

void getCallback(redisAsyncContext *c, void *r, void *privdata) {
    redisReply *reply = r;
    if (reply == NULL) return;
    printf("argv[%s]: %s\n", (char*)privdata, reply->str);
}
