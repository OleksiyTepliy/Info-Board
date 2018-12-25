#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include "i2c.h"
#include "uart.h"

uint8_t dev_adress;
uint8_t dev_register;
const uint8_t *transmit_buff;
uint8_t *receive_buff;
uint16_t data_len;
enum i2c_mode I2C_MODE;
volatile uint16_t data_idx = 0;
volatile bool busy_bus = false;

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
	TWBR = 0x48; // SCL - 100kHz
	TWCR |= (1 << TWEN) | (1 << TWIE); // enable TWI module and TWI interrupts
}

bool i2c_send(uint8_t dev_addr, uint8_t dev_mem_addr, uint8_t transmitted_data[], uint16_t write_size)
{
	while (busy_bus);
	dev_adress = dev_addr << 1;
	dev_register = dev_mem_addr;
	transmit_buff = transmitted_data;
	data_len = write_size;
	data_idx = 0;
	I2C_MODE = MT;
 	busy_bus = true;
	START;
	while(busy_bus);
	return true;
}

bool i2c_read(uint8_t dev_addr, uint8_t dev_mem_addr, uint8_t received_data[], uint16_t read_size)
{
	while (busy_bus);
	dev_adress = dev_addr << 1 | 0x01;
	dev_register = dev_mem_addr;
	receive_buff = received_data;
	data_len = read_size;
	data_idx = 0;
	I2C_MODE = MR;
 	busy_bus = true;
	START;
	while (busy_bus);
	return true;
}

// Two-wire Serial Interface Interrupt
ISR(TWI_vect, ISR_BLOCK)
{
	switch(TWSR) {
		case START_TRANSMITTED: {
			TWDR = dev_adress & ~0x01;
			DIS_START;
			CLEAR_TWINT;
		} break;
		case MT_SLA_W_TRANSMITTED_RECEIVED_ACK: {
			TWDR = dev_register;
			CLEAR_TWINT;
		} break;
		case MR_SLA_R_TRANSMITTED_RECEIVED_ACK: {
			CLEAR_TWINT;
		} break;
		case REPEATED_START_TRANSMITTED: {
				TWDR = dev_adress;
				DIS_START;
				CLEAR_TWINT;
		} break;
		case MT_DATA_TRANSMITTED_RECEIVED_ACK: {
			if (I2C_MODE == MR) {
				START;
			} else if (I2C_MODE == MT) {
				if (data_idx < data_len) {
					TWDR = transmit_buff[data_idx++];
					CLEAR_TWINT;
				} 
				else {
					STOP;
					busy_bus = false;
					CLEAR_TWINT;
				}
			}
		} break;
		case MR_DATA_RECIVED: {
			if (data_idx < data_len) {
				receive_buff[data_idx++] = TWDR;
				ACK;
			}
			else {
				NACK;
			}
			CLEAR_TWINT;
		} break;
		case MR_DATA_RECIVED_TRANSMITTED_NACK: {
			receive_buff[data_idx++] = TWDR;
			STOP;
			busy_bus = false;
			CLEAR_TWINT;
		} break;
		default: {
			busy_bus = false;
			CLEAR_TWINT;
		}
	}
}
