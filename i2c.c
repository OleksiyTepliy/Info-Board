#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <stdint.h>
#include "i2c.h"
#include "uart.h"


#define ACK (TWCR |= (1 << TWEA))
#define NACK (TWCR &= ~(1 << TWEA))
#define START (TWCR |= ((1 << TWINT) | (1 << TWSTA)))
#define DIS_START (TWCR &= ~(1 << TWSTA))
#define STOP (TWCR |= (1 << TWSTO))
#define CLEAR_TWINT (TWCR |= (1 << TWINT))



/***********************************************
Transmission Modes
The TWI can operate in one of four major modes:
• Master Transmitter (MT)
• Master Receiver (MR)
• Slave Transmitter (ST)
• Slave Receiver (SR)
***********************************************/

/* in our case MT and MR will be used */


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
	TWCR |= (1 << TWEN) | (1 << TWIE); // eneble TWI and TWI interrupt
}

// ds1307 slave address 1101000  (if read + 0x01 if write  + 0x00)


void i2c_send(uint8_t dev_addr, uint8_t dev_mem_addr, uint8_t *data, uint16_t size)
{
	uint8_t write = (dev_addr << 1);

	START; // start condition
}

void i2c_read(uint8_t dev_addr, uint8_t dev_mem_addr, uint8_t *data, uint16_t size)
{

}

char info[20] = {'\0'};
uint8_t ds1307_addr = 0x68;


// Two-wire Serial Interface Interrupt
ISR(TWI_vect, ISR_BLOCK)
{
	uint8_t read = (ds1307_addr << 1) | 0x01;
	uint8_t write = (ds1307_addr << 1);

	static uint8_t num = 0;

	/* WRITE TO REGISTER */
	if (num == 0) {
		TWDR = write; // address + write flag
		DIS_START; // stop start 
		CLEAR_TWINT; // clear interrupt flag, immideatly will activate TWI and send data.
	}
	if (num == 1) {
		TWDR = 0x06; // register to write
		CLEAR_TWINT;
	}
	if (num == 2) {
		TWDR = 0x12; // address + read flag
		CLEAR_TWINT;
	}
	if (num == 3) {
		STOP; // stop condition
		CLEAR_TWINT;
	}
	/* WRITE TO REGISTER */ 	/********************************/


	/* READ FROM REGISTER */ 	/********************************/
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

	/* READ FROM REGISTER */
	num++;
}
