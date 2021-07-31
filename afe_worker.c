#include "cs5484_wiringpi.h"
#include "rtc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>

#define BUS_SPEED 		1000000
#define SPI_MODE_CS5484		3

#define N_METER_DATAFIELD       8 // i, v, p, q, s, pf, kwh, kVArh
const char* BACKUP_FILENAME = "backup_meter.txt";
const char* REDIS_CHANNELNAME_V = "meter_1phase.CS5484_Evalboard.v";
const char* REDIS_CHANNELNAME_I = "meter_1phase.CS5484_Evalboard.i";
const char* REDIS_CHANNELNAME_P = "meter_1phase.CS5484_Evalboard.P";
const char* REDIS_CHANNELNAME_Q = "meter_1phase.CS5484_Evalboard.Q";
const char* REDIS_CHANNELNAME_S = "meter_1phase.CS5484_Evalboard.S";
const char* REDIS_CHANNELNAME_PF = "meter_1phase.CS5484_Evalboard.PF";
const char* REDIS_CHANNELNAME_KWH = "meter_1phase.CS5484_Evalboard.energy";
const char* REDIS_CHANNELNAME_KVARH = "meter_1phase.CS5484_Evalboard.energy_kvarh";

#define REDIS_URL	"www.ismartmeter.net"
#define REDIS_PORT	16379




int main()
{  
    /*
     *  initialize wiring-Pi and SPI interface
     *
     */
    wiringPiSetup();
    pinMode(CS_PIN, OUTPUT);
    pullUpDnControl(CS_PIN, PUD_UP);
    digitalWrite(CS_PIN, 1);
    int ret;
    ret = wiringPiSPISetupMode(SPI_CHANNEL, BUS_SPEED, SPI_MODE_CS5484);
    printf("SPI init result : %d\r\n", ret);

    ret = reset(0);
    printf("Reset CS5484 : ret=%d\r\n", ret);
    delay(5000);



    /*
     *  read the previous value from backup file
     *
     */
    FILE *backup_file;
    char line[1024];
    double field[N_METER_DATAFIELD];
    
    backup_file = fopen(BACKUP_FILENAME, "r");
    if(backup_file == NULL){
        printf("backup file does not exist, exist program\n");
	return 0;
    }
    else{
        fgets(line, 1024, backup_file);
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
        fclose(backup_file);
    }
    
    uint32_t buf;
    float i;		// current
    float v;		// voltage
    float p;		// active power
    float q;              // reactive power
    float s;              // apparent power
    float pf; 	        // power factor
    double kwh;         // energy consumption (real)
    double kVArh;       // energy consumption (complex)
    
    // assign backup value to current value
    i = field[0];
    v = field[1];
    p = field[2];
    q = field[3];
    s = field[4];
    pf = field[5];
    kwh = field[6];
    kVArh = field[7];
    


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
    while(1)
    {  
	/*
	 *  Start conversion of cs5484 
	 *  and wait until sampling is ready (polling method)
	 *
	 */
	ret = start_conversion(CONVERSION_TYPE_SINGLE, 0, 3000);

	if(ret == STATUS_OK){
	    // read all param from conversion result
            i  = get_current_rms(ANALOG_INPUT_CH2, 0);
            v  = get_voltage_rms(ANALOG_INPUT_CH2, 0);
            p  = get_act_power_avg(ANALOG_INPUT_CH2, 0);
            q  = get_react_power_avg(ANALOG_INPUT_CH2, 0);
            s  = get_apparent_power_avg(ANALOG_INPUT_CH2, 0);
	    pf = get_pf(ANALOG_INPUT_CH2, 0);
            
	    // calculate energy
            kwh = kwh + (double)p/1000.0/3600.0;
	    kVArh = kVArh + (double)s/1000.0/3600.0;
	    
	    // print result
            printf("I, V, P, Q, S, PF, KWH :\t");
	    printf("%f\t%f\t%f\t%f\t%f\t%f\t%f\r\n", i, v, p, q, s, pf, kwh);
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
	    sprintf(str_buf, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%f,%f,timestamp=%s\n", i, v, p, q, s, pf, kwh, kVArh, timestamp);
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
	    	char *value;
                printf("Set tag via Redis...\n\n"); 
	        
	    	channel = (char *)REDIS_CHANNELNAME_V;
	    	sprintf(value, "%.2f", v);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	    	channel = (char *)REDIS_CHANNELNAME_I;
	    	sprintf(value, "%.2f", 1000*i);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	   
	    	channel = (char *)REDIS_CHANNELNAME_P;
	    	sprintf(value, "%.2f", p);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	        
	    	channel = (char *)REDIS_CHANNELNAME_Q;
	    	sprintf(value, "%.2f", q);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);

	    	channel = (char *)REDIS_CHANNELNAME_S;
	    	sprintf(value, "%.2f", s);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	        
		channel = (char *)REDIS_CHANNELNAME_PF;
	    	sprintf(value, "%.2f", pf);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
                
	    	channel = (char *)REDIS_CHANNELNAME_KWH;
	    	sprintf(value, "%.5f", kwh);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s tag:%s %s tagtime:%s %s", "mset", channel, value
										     , channel, timestamp);
	    	
		channel = (char *)REDIS_CHANNELNAME_KVARH;
	    	sprintf(value, "%.5f", kVArh);
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



