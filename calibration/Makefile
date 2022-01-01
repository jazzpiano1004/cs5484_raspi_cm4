#make file - calibration for smartmeter project
 
CC=gcc  #compiler
TARGET_GAIN=calibrate_gain     #target file name
TARGET_OFFSET=calibrate_offset #target file name
TARGET_NOLOAD=calibrate_noload #target file name

 
all:
	$(CC) calibrate_gain.c ../smartmeter_lib/cs5484_wiringpi.c ../smartmeter_lib/relay_led.c -l wiringPi -o $(TARGET_GAIN)
	$(CC) calibrate_offset.c ../smartmeter_lib/cs5484_wiringpi.c ../smartmeter_lib/relay_led.c -l wiringPi -o $(TARGET_OFFSET)
	$(CC) calibrate_noload.c ../smartmeter_lib/cs5484_wiringpi.c ../smartmeter_lib/relay_led.c -l wiringPi -o $(TARGET_NOLOAD)
 
clean:
	rm $(TARGET_GAIN)
	rm $(TARGET_OFFSET)
	rm $(TARGET_NOLOAD)
