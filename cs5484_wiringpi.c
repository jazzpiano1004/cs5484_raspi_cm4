#include "cs5484_wiringpi.h"






uint8_t write(uint32_t data, uint8_t addr, uint8_t csum_en)
{
    uint8_t *pdata = (uint8_t *)&data;
    uint8_t buf[5];
    uint8_t csum;

    buf[0] = 0x40 | addr;       // CMD byte
    buf[1] = pdata[2];          // Data byte (MSB)
    buf[2] = pdata[1];          // Data byte
    buf[3] = pdata[0];          // Data byte (LSB)
     
    digitalWrite(CS_PIN, 0);
    wiringPiSPIDataRW(SPI_CHANNEL, buf, 4);             // send CMD + write data
    if(csum_en != 0){
	csum = 0xFF - (pdata[0] + pdata[1] + pdata[2]);
        wiringPiSPIDataRW(SPI_CHANNEL, &csum, 1);       // send checksum after write
    }
    digitalWrite(CS_PIN, 1);

    return STATUS_OK;
}

uint8_t read(uint32_t *data, uint8_t addr, uint8_t csum_en)
{
    uint8_t buf[5];
    uint8_t csum;
    
    buf[0] = 0x00 | addr;
    buf[1] = 0xFF;
    buf[2] = 0xFF;
    buf[3] = 0xFF;

    digitalWrite(CS_PIN, 0);
    wiringPiSPIDataRW(SPI_CHANNEL, buf, 1);        // send CMD
    if(csum_en != 0){
	csum = 0xFF - addr;
        wiringPiSPIDataRW(SPI_CHANNEL, &csum, 1);  // send checksum after CMD
        wiringPiSPIDataRW(SPI_CHANNEL, &buf[1], 3);  // read data
        *data = (uint32_t)(buf[3] + (8 << buf[2]) + (16 << buf[1]));
	csum = 0xFF;
        wiringPiSPIDataRW(SPI_CHANNEL, &csum, 1);  // read checksum after read
        digitalWrite(CS_PIN, 1);
        // checksum 	
        if(csum == (uint8_t)(0xFF - (buf[3] + buf[2] + buf[1]))){
	    return STATUS_OK;
	}
	else{
	    return STATUS_FAIL;
	}
    }
    else{
        wiringPiSPIDataRW(SPI_CHANNEL, &buf[1], 3);  // read data
        digitalWrite(CS_PIN, 1);
        *data = (uint32_t)(buf[3] + (buf[2] << 8) + (buf[1] << 16));
	return STATUS_OK;
    }
}

uint8_t setpage(uint8_t page, uint8_t csum_en)
{
    uint8_t buf[5];
    uint8_t csum;

    buf[0] = 0x80 | page;
    digitalWrite(CS_PIN, 0);
    wiringPiSPIDataRW(SPI_CHANNEL, buf, 1);  // send CMD
    if(csum_en != 0){
	csum = 0xFF - page;
        wiringPiSPIDataRW(SPI_CHANNEL, &csum, 1);  // send checksum
    }
    digitalWrite(CS_PIN, 1);

    return STATUS_OK;
}

uint8_t reset(uint8_t csum_en)
{
    uint8_t buf[5];
    uint8_t csum;
    uint8_t ctrl_code;

    buf[0] = 0xC0 | ctrl_code;
    digitalWrite(CS_PIN, 0);
    wiringPiSPIDataRW(SPI_CHANNEL, buf, 1);  // send CMD
    if(csum_en != 0){
	csum = 0xFF - ctrl_code;
        wiringPiSPIDataRW(SPI_CHANNEL, &csum, 1);  // send checksum
    }
    digitalWrite(CS_PIN, 1);

    return STATUS_OK;
}
