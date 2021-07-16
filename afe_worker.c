#include "cs5484_wiringpi.h"
#include "rtc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <hiredis/hiredis.h>

#define BUS_SPEED 		1000000
#define SPI_MODE_CS5484		3

#define CURRENT_FULLSCALE 150000*25
#define VOLT_FULLSCALE    438000
#define POWER_LINE_FULLSCALE ((long long)CURRENT_FULLSCALE*VOLT_FULLSCALE/1000000)

#define N_METER_DATAFIELD       8 // i, v, p, q, s, pf, kwh, kVArh

const char* BACKUP_FILENAME = "backup_meter.txt";
const char* REDIS_CHANNELNAME_V = "tag:meter_1phase.CS5484_Evalboard.v";
const char* REDIS_CHANNELNAME_I = "tag:meter_1phase.CS5484_Evalboard.i";
const char* REDIS_CHANNELNAME_P = "tag:meter_1phase.CS5484_Evalboard.P";
const char* REDIS_CHANNELNAME_Q = "tag:meter_1phase.CS5484_Evalboard.Q";
const char* REDIS_CHANNELNAME_S = "tag:meter_1phase.CS5484_Evalboard.S";
const char* REDIS_CHANNELNAME_PF = "tag:meter_1phase.CS5484_Evalboard.PF";
const char* REDIS_CHANNELNAME_KWH = "tag:meter_1phase.CS5484_Evalboard.energy";
const char* REDIS_CHANNELNAME_KVARH = "tag:meter_1phase.CS5484_Evalboard.energy_kvarh";



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
    int i;		// current
    int v;		// voltage
    int p;		// active power
    int q;              // reactive power
    int s;              // apparent power
    int pf; 	        // power factor
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
    /*
    redisContext* c = redisConnect((char*)"192.168.4.209", 6379);
    
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
    */


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
	ret = start_conversion(CONVERSION_TYPE_SINGLE, 0, 1000);

	if(ret == STATUS_OK){
	    // read all param from conversion result
            i  = get_current_rms(ANALOG_INPUT_CH2, 0);
            v  = get_voltage_rms(ANALOG_INPUT_CH2, 0);
            p  = get_act_power_avg(ANALOG_INPUT_CH2, 0);
            q  = get_react_power_avg(ANALOG_INPUT_CH2, 0);
            s  = get_apparent_power_avg(ANALOG_INPUT_CH2, 0);
	    pf = get_pf(ANALOG_INPUT_CH2, 0);
	   
	     
	    // offset and gain correction (should be include in calibration process)
	    v = v - 100*1000;
	    p = p + 133;
            
	    // calculate energy
            kwh = kwh + (double)p/1000.0/3600.0;
	    kVArh = kVArh + (double)s/1000.0/3600.0;
	    
	    // print result
            printf("I, V, P, Q, S, PF, KWH :\t");
	    printf("%d\t%d\t%d\t%d\t%d\t%d\t%f\r\n", i, v, p, q, s, pf, kwh);
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
        printf("timestamp=%s\n", timestamp);


	/*
	 *  Write data to backup file
	 *
	 */
        backup_file = fopen(BACKUP_FILENAME, "r+");
	if(backup_file != NULL){
            char str_buf[100];
	    sprintf(str_buf, "%d,%d,%d,%d,%d,%d,%f,%f,timestamp=%s\n", i, v, p, q, s, pf, kwh, kVArh, timestamp);
	    fputs(str_buf, backup_file);
	    fclose(backup_file);
	}



	/*
	 *  Connect to REDIS and Set value with a CS5484's sampling
	 *
	 */
	/*
        redisFree(c);
        c = redisConnect((char*)"192.168.4.209", 6379);
	if(!(c->err)){
	    freeReplyObject(r);
            r = (redisReply*)redisCommand(c, command_auth);
	    if((r->type == REDIS_REPLY_STATUS) && (strcasecmp(r->str, "OK") == 0)){
	        char *channel;
	    	char *value;

	    	channel = (char *)REDIS_CHANNELNAME_V;
	    	sprintf(value, "%.2f", (float)v/1000);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    
	    	channel = (char *)REDIS_CHANNELNAME_I;
	    	sprintf(value, "%d", i);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	   
	    	channel = (char *)REDIS_CHANNELNAME_P;
	    	sprintf(value, "%d", p);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    
	    	channel = (char *)REDIS_CHANNELNAME_Q;
	    	sprintf(value, "%d", q);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);

	    	channel = (char *)REDIS_CHANNELNAME_S;
	    	sprintf(value, "%d", s);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	        
		channel = (char *)REDIS_CHANNELNAME_PF;
	    	sprintf(value, "%d", pf);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    
	    	channel = (char *)REDIS_CHANNELNAME_KWH;
	    	sprintf(value, "%f", kwh);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    	
		channel = (char *)REDIS_CHANNELNAME_KVARH;
	    	sprintf(value, "%f", kVArh);
		freeReplyObject(r);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    }
	    else{
		printf("Cannot auth to redis\n");
	    }
        }
	else{
            printf("Cannot connect to redis server\n");
	}
	*/
    }

    //redisFree(c);
    return 0;
}



