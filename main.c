#include "cs5484_wiringpi.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUS_SPEED 		1000000
#define SPI_MODE_CS5484		3

#define CURRENT_FULLSCALE 150000
#define VOLT_FULLSCALE    438000
#define POWER_LINE_FULLSCALE ((long long)CURRENT_FULLSCALE*VOLT_FULLSCALE/1000000)

#define N_METER_DATAFIELD 5 // i, v, p, pf, kwh
#define BACKUP_FILENAME   "backup_meter.txt"


int main()
{  
    // initialize wiring-Pi
    wiringPiSetup();

    // intialize SPI interface
    pinMode(CS_PIN, OUTPUT);
    pullUpDnControl(CS_PIN, PUD_UP);
    digitalWrite(CS_PIN, 1);
    int ret;
    ret = wiringPiSPISetupMode(SPI_CHANNEL, BUS_SPEED, SPI_MODE_CS5484);
    printf("SPI init result : %d\r\n", ret);

    ret = reset(0);
    printf("Reset CS5484 : ret=%d\r\n", ret);
    delay(1000);

    /*
     *  read the previous history from log file
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
    uint32_t i;		// current
    uint32_t v;		// voltage
    uint32_t p;		// power (active power)
    uint32_t pf; 	// power factor
    double kwh;         // energy consumption
    
    // assign backup value to current value
    i = field[0];
    v = field[1];
    p = field[2];
    pf = field[3];
    kwh = field[4];

    // init GPIO (for testing)
    pinMode(23, OUTPUT);

    while(1)
    {  
	// start conversion and wait until completed (polling method)
	ret = start_conversion(CONVERSION_TYPE_SINGLE, 0, 10000);
        
	if(ret == STATUS_OK){
	    // read all param from conversion result
            i  = get_current_rms(ANALOG_INPUT_CH1, 0);
            v  = get_voltage_rms(ANALOG_INPUT_CH1, 0);
            p  = get_power_avg(ANALOG_INPUT_CH1, 0);
	    pf = get_pf(ANALOG_INPUT_CH1, 0);
            kwh = kwh + (double)p/1000/3600;
	    
	    // print result
            printf("I, V, P, PF, KWH :\t\t");
	    printf("%d\t\t%d\t\t%d\t\t%d\t\t%f\r\n", i, v, p, pf, kwh);

	    // write data to backup file
            backup_file = fopen(BACKUP_FILENAME, "r+");
            char str_buf[20];
	    sprintf(str_buf, "%d,%d,%d,%d,%f\n", i, v, p, pf, kwh);
	    fputs(str_buf, backup_file);
	    fclose(backup_file);
        }
        else{
	    printf("Error from conversion : %d\n", ret);
	}
    }


    return 0;
}

