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

#define N_METER_DATAFIELD 5



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

    // read the previous history from log file
    FILE *backup_file;
    backup_file = fopen("backup_meter.txt", "r+");

    char line[1024];
    fgets(line, 1024, backup_file);
    printf("read file : %s\n", line);
    double field[N_METER_DATAFIELD];
    for(int i=0; i<N_METER_DATAFIELD; i++){
        char *token;
        token = strtok(line, ",");
	field[i] = atof(token);
	printf("%f\n", field[i]);
	token = strtok(NULL, "\t\n");
    }
    fclose(backup_file);

    
    uint32_t buf;
    uint32_t i;		// current
    uint32_t v;		// voltage
    uint32_t p;		// power (active power)
    uint32_t pf; 	// power factor
    double kwh = 0;     // energy consumption

    // init GPIO (for testing)
    pinMode(23, OUTPUT);

    while(1)
    {  
	// start conversion and wait until completed (polling method)
	ret = start_conversion(CONVERSION_TYPE_SINGLE, 0, 10000);
        
	if(ret == STATUS_OK){
	    // read all param from conversion result
            v  = get_voltage_rms(ANALOG_INPUT_CH1, 0);
            i  = get_current_rms(ANALOG_INPUT_CH1, 0);
            p  = get_power_avg(ANALOG_INPUT_CH1, 0);
	    pf = get_pf(ANALOG_INPUT_CH1, 0);
            kwh = kwh + (double)p/1000/3600;
	    
	    // print result
            printf("I, V, P, PF, KWH :\t\t");
	    printf("%d\t\t%d\t\t%d\t\t%d\t\t%f\r\n", i, v, p, pf, kwh);
        }
        else{
	    printf("Error from conversion : %d\n", ret);
	}
    }


    return 0;
}

