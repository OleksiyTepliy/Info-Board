#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <stdint.h>
#include "i2c.h"
#include "uart.h"

uint8_t dev_adress;
uint8_t dev_register;
const uint8_t *data_buff;
uint16_t data_len;
enum i2c_mode I2C_MODE;
volatile uint16_t data_idx = 0;

void i2c_init()
{
//     TWBR // TWI Bit Rate Register
//     TWSR // TWI Status Register
//     TWAR // TWI (Slave) Address Register
//     TWDR // TWI Data Register
    
//     TWCR // TWI Control Register 
//     Bit 6 – TWEA TWI Enable Acknowledge
//     Bit 5 – TWSTA TWI START Condition
//     Bit 4 – TWSTO TWI STOP Condition
//     Bit 2 – TWEN TWI Enable
//     Bit 0 – TWIE TWI Interrupt Enable
	power_twi_enable();
	/* SCLK = 16Mhz / (16 + 2 * (TWBR) * (Prescaler)) */
	TWBR = 0x0C; // SCL - 400kHz
	TWCR |= (1 << TWEN) | (1 << TWIE); // enable TWI module and TWI interrupts
}

void i2c_send(uint8_t dev_addr, uint8_t dev_mem_addr, const uint8_t *data, uint16_t size)
{
	dev_adress = dev_addr << 1;
	dev_register = dev_mem_addr;
	data_buff = data;
	data_len = size;
	data_idx = 0;
	I2C_MODE = MT;
	START;
}

void i2c_read(uint8_t dev_addr, uint8_t dev_mem_addr, const uint8_t *data, uint16_t size)
{
	dev_adress = dev_addr << 1 | 0x01;
	dev_register = dev_mem_addr;
	data_buff = data;
	data_len = size;
	data_idx = 0;
	I2C_MODE = MR;
	START;
}

//uint8_t ds1307_addr = 0x68;

// Two-wire Serial Interface Interrupt
ISR(TWI_vect, ISR_BLOCK)
{
	switch(TWSR) {
		case START_TRANSMITTED: {
			TWDR = dev_adress;
			DIS_START;
			CLEAR_TWINT;
		} break;
		case MT_SLA_W_TRANSMITTED_RECEIVED_ACK: {
			TWDR = dev_register;
			CLEAR_TWINT;
		} break;
		case MR_SLA_R_TRANSMITTED_RECEIVED_ACK: {
			START;
		} break;
		case REPEATED_START_TRANSMITTED: {
			TWDR = dev_register; // register to read
			DIS_START; // stop start
			CLEAR_TWINT;
		} break;
		case MR_DATA_RECIVED_RECEIVED_ACK: {
			uint8_t info = TWDR;
			ACK; // ACK
			CLEAR_TWINT;
		} break;
		case MT_DATA_TRANSMITTED_RECEIVED_ACK: {
			if (data_idx < data_len) {
				TWDR = data_buff[data_idx++];
				CLEAR_TWINT;
			}
			else {
				STOP;
				CLEAR_TWINT;
			}
		} break;
		default: {
			//CLEAR_TWINT;
		}
	}

/*
	// READ FROM REGISTER
	if (num == 4) {
		TWDR = write; // address + write flag
		DIS_START; // stop start 
		CLEAR_TWINT; // clear interrupt flag, immideatly will activate TWI and send data.
	}
	if (num == 5) {
		TWDR = 0x06; // register to read
		CLEAR_TWINT;
	}
	if (num == 6) {
		START;

	}
	if (num == 7) {
		TWDR = read;
		DIS_START; // stop start
		CLEAR_TWINT;
	}
	if (num == 8) {
		//info[0] = TWDR; // succsess  read cmd transmit ACK or NACK says slave
		ACK; // ACK
		CLEAR_TWINT;
	}
	if (num == 9) {
		info[0] = TWDR; // wait for first data byte
		//NACK;
		CLEAR_TWINT;
	}
	if (num == 10) {
		info[1] = TWDR; // wait for second data byte
		NACK;
		STOP;
		CLEAR_TWINT;
	}
	if (num == 11) {
		info[2] = '\0';
		//STOP; // stop condition
		CLEAR_TWINT;
	}


	if (num == 11) {
		uart_send(info);
		num = 0;
	}

	// READ FROM REGISTER 
*/
	//num++;
}
