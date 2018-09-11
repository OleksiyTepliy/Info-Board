enum i2c_mode {
	MT = 0,
	MR,
};


enum i2c_bus_state {
	IDLE,
	SEND_START,
	SEND_SECOND_START,
	SEND_ADDRESS,
	SEND_DATA,
	SEND_STOP,
};


enum i2c_master_transmitter_mode {
	START_TRANSMITTED = 0x08,
	REPEAT_START_TRANSMITTED = 0x10,
	SLA_W_TRANSMITTED_ACK = 0x18,
	SLA_W_TRANSMITTED_NACK = 0x20,
	DATA_TRANSMITTED_ACK = 0x28,
	DATA_TRANSMITTED_NACK = 0x30,
	ARBITRATION_LOST = 0x38,
};


enum i2c_master_reciver_mode {
	SLA_R_TRANSMITTED_ACK = 0x40,
	SLA_R_TRANSMITTED_NACK = 0x48,
	DATA_RECIVED_ACK = 0x50,
	DATA_RECIVED_NACK = 0x58,
};


void i2c_init();


void i2c_send(uint8_t dev_addr, uint8_t dev_mem_addr, uint8_t *data, uint16_t size);