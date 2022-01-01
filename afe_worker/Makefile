#make file - afe worker for smartmeter project
 
CC=gcc  #compiler
#target file name (output bin)
TARGET=afe_worker
TARGET_ASYNC=afe_worker_async
 
all:
	$(CC) afe_worker.c ../smartmeter_lib/cs5484_wiringpi.c ../smartmeter_lib/rtc.c ../smartmeter_lib/relay_led.c \
		../smartmeter_lib/ct_model.c -l wiringPi -l hiredis -o $(TARGET)
	$(CC) afe_worker_async.c ../smartmeter_lib/cs5484_wiringpi.c ../smartmeter_lib/rtc.c ../smartmeter_lib/relay_led.c \
		../smartmeter_lib/ct_model.c -l wiringPi -l hiredis -l event -l pthread -o $(TARGET_ASYNC)

debug:
	$(CC) afe_worker.c ../smartmeter_lib/cs5484_wiringpi.c ../smartmeter_lib/rtc.c ../smartmeter_lib/relay_led.c \
		../smartmeter_lib/ct_model.c -g -l wiringPi -l hiredis -o $(TARGET)_debug
	$(CC) afe_worker_async.c ../smartmeter_lib/cs5484_wiringpi.c ../smartmeter_lib/rtc.c ../smartmeter_lib/relay_led.c \
		../smartmeter_lib/ct_model.c -g -l wiringPi -l hiredis -l event -l pthread -o $(TARGET_ASYNC)_debug

clean:
	rm $(TARGET) $(TARGET_ASYNC)
	rm $(TARGET)_debug $(TARGET_ASYNC)_debug
