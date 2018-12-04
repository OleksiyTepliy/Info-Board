#include <stdint.h>
#include <avr/io.h>
#include "Board_Info.h"
#include "MAX7219.h"
#include "spi.h"


void max7219_Init(BRIGHTNESS_MODE brightnessLevel)
{
	max7219_cmd_to(ALL, MAX7219_SHUTDOWN_REG, 0x01); // Turn On.
	max7219_cmd_to(ALL, MAX7219_SCAN_LIMIT_REG, 0x07); // Activate all rows.
	max7219_cmd_to(ALL, MAX7219_INTENSITY_REG, brightnessLevel); // 0 - 15 levels of Intensity.
	max7219_cmd_to(ALL, MAX7219_DECODE_MODE_REG, 0x00); // No decode mode.
	max7219_cmd_to(ALL, MAX7219_DISPLAY_TEST_REG, 0x00); // Display-Test off.
}


void max7219_clear_panels(uint8_t panel_num)
{
	enum MAX7219_registers reg;
	for (uint8_t n = 0; n < LED_NUM; n++)
		for (reg =  MAX7219_ROW_1_REG; reg <= MAX7219_ROW_8_REG; reg++)
			max7219_cmd_to(panel_num, reg, 0x00); // clean all registers
}


/* function send 16bit word array and latch it in max7219 registers */
static void max7219_send_array(uint16_t *arr, uint16_t size)
{
	CS_LOW;
	for (uint16_t i = 0; i < size; i++) {
		SPI_Transmit(arr[i] >> 8); // send register
		SPI_Transmit(arr[i] & 0x00FF); // send data
	}
	CS_HIGH;
}


void max7219_send_char_to(uint8_t num, const uint8_t *data)
{
	uint16_t buffer[LED_NUM];
	enum MAX7219_registers reg;
	for (reg = MAX7219_ROW_1_REG; reg <= MAX7219_ROW_8_REG; reg++) {
		uint16_t two_bytes = data[reg - 1] | (reg << 8);

		if (num == ALL)	// send bytes to all panels
			for (uint8_t i = 0; i < LED_NUM; i++)
				buffer[i] = two_bytes;
		else
			for (uint8_t i = 0; i < LED_NUM; i++)
				buffer[i] = (i == num) ? two_bytes : 0x0000; // No Op
		
		max7219_send_array(buffer, LED_NUM);
	}
}


void max7219_cmd_to(uint8_t num, enum MAX7219_registers reg, uint8_t data)
{
	uint16_t buffer[LED_NUM];
	uint16_t two_bytes = data | (reg << 8);
	uint8_t i;
	
	if (num == ALL)	// send bytes to all panels
		for (i = 0; i < LED_NUM; i++)
			buffer[i] = two_bytes;
	else
		for (i = 0; i < LED_NUM; i++)
			buffer[i] = (i == num) ? two_bytes : 0x0000; // No Op
	
	max7219_send_array(buffer, LED_NUM);
}

/* function transform array of columns into array of rows, that can be sent to max7219 */
static void trans_panel(uint8_t *array, uint8_t *fill_array)
{
	for (uint8_t row = 0; row < 8; row++) {
		uint8_t bit = 1 << row;
		for (int8_t i = 7, bit_indx = 0; i >= 0; i--, bit_indx++) {
			uint8_t data = 0x00;
			data = array[i] & bit; // read bit
			data >>= row; // move to 0 position
			data <<= bit_indx; // move to proper position
			if (!data) { // == 0x00
				uint8_t data_and_mask = ~(1 << bit_indx); // create mask
				fill_array[row] &= (data | data_and_mask); // write 0 in bit
			}
			else
				fill_array[row] |= (data); // write 1 in bit
		}
    }
}


void update_screen(uint8_t *screenbuff)
{
	uint8_t buff_to_send[LED_NUM * 8];
	for (uint8_t n = 0; n < LED_NUM; n++) {
		uint8_t indx = n << 3;
		trans_panel(&screenbuff[indx], &buff_to_send[indx]);
		max7219_send_char_to(n, &buff_to_send[indx]);
	}
}

