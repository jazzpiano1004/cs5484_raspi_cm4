#make file - afe worker for smartmeter project
 
CC=gcc  #compiler
TARGET=test_cs5484 #target file name
 
all:
	$(CC) test_cs5484.c ../../smartmeter_lib/cs5484_wiringpi.c -l wiringPi -o $(TARGET)
 
clean:
	rm $(TARGET)
