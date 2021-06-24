#include "cs5484_wiringpi.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>

#define BUS_SPEED 		1000000
#define SPI_MODE_CS5484		3

#define CURRENT_FULLSCALE 150000*25
#define VOLT_FULLSCALE    438000
#define POWER_LINE_FULLSCALE ((long long)CURRENT_FULLSCALE*VOLT_FULLSCALE/1000000)

#define N_METER_DATAFIELD 5 // i, v, p, pf, kwh
const char* BACKUP_FILENAME = "backup_meter.txt";
const char* REDIS_CHANNELNAME_V = "tag:meter_1phase.CS5484_Evalboard.v";
const char* REDIS_CHANNELNAME_I = "tag:meter_1phase.CS5484_Evalboard.i";
const char* REDIS_CHANNELNAME_P = "tag:meter_1phase.CS5484_Evalboard.P";
const char* REDIS_CHANNELNAME_PF = "tag:meter_1phase.CS5484_Evalboard.PF";
const char* REDIS_CHANNELNAME_E = "tag:meter_1phase.CS5484_Evalboard.energy";



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
        memcpy(tmp, line+len_token+1, sizeof(line) - sizeof(len_token) - sizeof(char));
        memcpy(line, tmp, sizeof(tmp));
    }
    fclose(backup_file);
    
    uint32_t buf;
    int i;		// current
    int v;		// voltage
    int p;		// power (active power)
    int pf; 	        // power factor
    double kwh;         // energy consumption
    
    // assign backup value to current value
    i = field[0];
    v = field[1];
    p = field[2];
    pf = field[3];
    kwh = field[4];



    /*
     * REDIS initialize
     *
     */
    
    //redisContext* c = redisConnect((char*)"www.intelligentscada.com", 6379);
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
        return 0;
    }
    if(!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str, "OK") == 0)){
        printf("Failed to execute command[%s].\n", command_auth);
    }
    else{
        freeReplyObject(r);
        printf("Succeed to execute command[%s].\n", command_auth);
    }



    /*
     * main loop
     *
     */
    while(1)
    {  
	// start conversion and wait until completed (polling method)
	ret = start_conversion(CONVERSION_TYPE_SINGLE, 0, 2000);
        
	if(ret == STATUS_OK){
	    // read all param from conversion result
            i  = get_current_rms(ANALOG_INPUT_CH1, 0);
            v  = get_voltage_rms(ANALOG_INPUT_CH1, 0);
            p  = get_power_avg(ANALOG_INPUT_CH1, 0);
	    pf = get_pf(ANALOG_INPUT_CH1, 0);
            // CT of remoted evalboard is in inverse direction right now \_--_/
	    p = -p;
            kwh = kwh + (double)p/1000.0/3600.0;
	    
	    // print result
            printf("I, V, P, PF, KWH :\t\t");
	    printf("%d\t\t%d\t\t%d\t\t%d\t\t%f\r\n", i, v, p, pf, kwh);
            
	    
	    // write data to backup file
            backup_file = fopen(BACKUP_FILENAME, "r+");
            char str_buf[20];
	    sprintf(str_buf, "%d,%d,%d,%d,%f\n", i, v, p, pf, kwh);
	    fputs(str_buf, backup_file);
	    fclose(backup_file);
            
	    // set tag to REDIS db
	    if(!(c->err)){
	    	char *channel;
	    	char *value;
	    	channel = (char *)REDIS_CHANNELNAME_V;
	    	sprintf(value, "%.2f", (float)v/1000);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    	
	    	channel = (char *)REDIS_CHANNELNAME_I;
	    	sprintf(value, "%d", i);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    
	    	channel = (char *)REDIS_CHANNELNAME_P;
	    	sprintf(value, "%d", p);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    
	    	channel = (char *)REDIS_CHANNELNAME_PF;
	    	sprintf(value, "%d", pf);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
	    
	   	 channel = (char *)REDIS_CHANNELNAME_E;
	    	sprintf(value, "%f", kwh);
            	r = (redisReply*)redisCommand(c, "%s %s %s", "set", channel, value);
            
	    	if(!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str, "OK") == 0)){
                    printf("Failed to execute set command");
                    freeReplyObject(r);
            	}
	    }
        }
        else{
	    printf("Error from conversion : %d\n", ret);
	}
    }

    //redisFree(c);
    return 0;
}



