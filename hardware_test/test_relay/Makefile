#make file - afe worker for smartmeter project
 
CC=gcc  #compiler
TARGET=test_relay #target file name
 
all:
	$(CC) test_relay.c ../../smartmeter_lib/relay_led.c -l wiringPi -o $(TARGET)
 
clean:
	rm $(TARGET)
