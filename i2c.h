#define ACK (TWCR |= (1 << TWEA))
#define NACK (TWCR &= ~(1 << TWEA))
#define START (TWCR |= (1 << TWINT) | (1 << TWSTA))
#define DIS_START (TWCR &= ~(1 << TWSTA))
#define STOP (TWCR |= (1 << TWSTO))
#define CLEAR_TWINT (TWCR |= (1 << TWINT))

/***********************************************
Transmission Modes.
The TWI can operate in one of four major modes:
• Master Transmitter (MT)
• Master Receiver (MR)
• Slave Transmitter (ST)
• Slave Receiver (SR)
***********************************************/

/* in our case MT and MR modes will be used */
enum i2c_mode {
	MT = 0,
	MR = 1,
};

enum i2c_bus_state {
	IDLE,
	SEND_START,
	SEND_SECOND_START,
	SEND_ADDRESS,
	SEND_DATA,
	SEND_STOP,
};

enum i2c_master_common_status_codes {
	START_TRANSMITTED = 0x08,
	REPEATED_START_TRANSMITTED = 0x10,
	ARBITRATION_LOST = 0x38,
};

enum i2c_master_transmitter_status_codes {
	MT_SLA_W_TRANSMITTED_RECEIVED_ACK = 0x18,
	MT_SLA_W_TRANSMITTED_RECEIVED_NACK = 0x20,
	MT_DATA_TRANSMITTED_RECEIVED_ACK = 0x28,
	MT_DATA_TRANSMITTED_RECEIVED_NACK = 0x30,
};

enum i2c_master_receiver_status_codes {
	MR_SLA_R_TRANSMITTED_RECEIVED_ACK = 0x40,
	MR_SLA_R_TRANSMITTED_RECEIVED_NACK = 0x48,
	MR_DATA_RECIVED_RECEIVED_ACK = 0x50,
	MR_DATA_RECIVED_RECEIVED_NACK = 0x58,
};

void i2c_init();

void i2c_send(uint8_t dev_addr, uint8_t dev_mem_addr, const uint8_t *data, uint16_t size);

void i2c_read(uint8_t dev_addr, uint8_t dev_mem_addr, const uint8_t *data, uint16_t size);