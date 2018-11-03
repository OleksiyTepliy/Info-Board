#include "Board_Info.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/eeprom.h>
#include <stdint.h>
#include <stdio.h> /************************************************************************************/
#include <stdbool.h>
#include "spi.h"
#include "front.h"
#include "MAX7219.h"
#include "uart.h"
#include "process_cmd.h"
#include "adc.h"
#include "i2c.h"
#include "DS1307.h"

volatile uint16_t timer = 0; // time counter
volatile bool flags[U_SIZE] = {0};
uint8_t EEMEM eeprom_buff[MAX_MESSAGE_ARR_SIZE];
uint16_t EEMEM eeprom_buff_size;
uint8_t eeprom_update_buff[MAX_MESSAGE_ARR_SIZE] = {0};
volatile uint8_t photo_avg[PHOTO_MEASURE_SAMPLES] = {0};
volatile enum display_modes show_mode = CLOCK_SS;
volatile enum brightness_modes br_mode = BR_1;

struct rtc clock = {
	.hh = 0,
	.mm = 0,
	.ss = 0
};

/* default events timings in ms / 10 */
/* default timings divided by 10, because timer interrupt rise every 10 ms */
struct timings tm = { 
	.rtc = 10U, // 100 ms
	.screen = 4U, // 40 ms
	.temp = 300U, // 3sec
	.photo = 500U // 5 sec,
};


/**
 * u_screen - updates panels according to the mode flag.
 * @flag: display_modes flag
 */
static void u_screen(enum display_modes flag);

/**
 * u_time - updates time on display according to the mode flag.
 * @left: hours or minutes
 * @right: minutes or seconds
 */
static void u_time(uint8_t left, uint8_t right);

/**
 * u_temp - updates temperature on display.
 * @t: temperature.
 */
static void u_temp(uint8_t t);

/**
 * u_photo - adjust brightness of the panel.
 * 
 */
static void u_photo(void);


int main(void)
{
	SPI_MasterInit(); // pin 10 CS, pin 11 MOSI, pin 13 SCLK
	uart_init();
	i2c_init();
	adc_init();
	max7219_Init();
	max7219_clear_panels(ALL);
	sei();
	ds1307_reset(); //do it after interrupts are enable

	/* Timer 1 Init */
	TCCR1B |= (1 << WGM12);	 // mode 4 CTC Mode
	TIMSK1 |= (1 << OCIE1A); // enable timer interrupt bit
	OCR1A |= 20000 - 1; 	 //fOCnA == 100hz; 10 ms interrupt.
	
	/* Led Init */
	DDRB |= 1 << DDB0; // pin 8 
	PORTB &= ~(1 << DDB0); // logic 0.


	/* read eeprom message size */
	uint16_t size = eeprom_read_word(&eeprom_buff_size);
	
	/* create an array for storing a message from EEPROM buffer */
	uint8_t message[MAX_MESSAGE_ARR_SIZE];

	/* read eeprom buffer message */
	eeprom_read_block (&message, &eeprom_buff, size);	
	message[size] = '\0';
	concat(message, size);

	/* main_buff, each byte represents a column of LED panel */
	/* One character of the message takes up 8 columns on panel */
	/* every column is a byte */
	uint8_t main_buff[(MAX_MESSAGE_ARR_SIZE - 1) * LED_SIZE];

	/* Fill main buffer with columns */
	uint16_t mess_len = strsize(message);
	str_to_arr_trans(message, mess_len, main_buff, FRONT_ASCII);

	/* *screen_buff is a pointer to main_buff array */
	/* We shift this pointer, and update panels state via update_screen routine */
	uint8_t *screen_buff = main_buff;

	/* last index of main_buff array minus sizeof screen_buff - 1 because counting from 0 */
	uint16_t last_indx = mess_len * 8 - LED_SIZE * LED_NUM - 1;

	/* set prescaler to 256 and start timer */
	TCCR1B |= (1 << CS11);

	while(1) {

		if (flags[U_SCREEN]) {
			if (show_mode == STRING) {
				if (screen_buff == main_buff + last_indx)
					screen_buff = main_buff;
				update_screen(screen_buff);
				screen_buff++;
			}
			else {
				u_screen(show_mode);
			}
			flags[U_SCREEN] = false;
		}

		if (flags[U_TEMP]) {
			/********** ONEWIRE START TEMP **********/
			flags[U_TEMP] = false;
		}

		if (flags[U_PHOTO]) {
			u_photo();
			flags[U_PHOTO] = false;
		}

		if (flags[U_UART]) {
			process_command();
			flags[U_UART] = false;
			/* after command process finished, enable reciver and rx interrupts */
			UCSR0B |= (1 << RXEN0) | (1 << RXCIE0);
		}

		if (flags[U_EEPROM]) {
			if (show_mode == STRING)
				max7219_clear_panels(ALL);
			
			/* No interrupts can occur while this block is executed */
			TCCR1B &= ~(1 << CS11); // turn off timer
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
				/* calculate size of the new message */
				uint16_t new_size = strsize(eeprom_update_buff);
				/* write new message to eeprom buff */
				eeprom_write_block(&eeprom_update_buff, &eeprom_buff, new_size);
				/* write size of the new message to eeprom */
				eeprom_write_word (&eeprom_buff_size, new_size);

				eeprom_read_block (&message, &eeprom_buff, new_size);
				message[new_size] = '\0';
				concat(message, new_size);

				mess_len = strsize(message);
				str_to_arr_trans(message, mess_len, main_buff, FRONT_ASCII);
				screen_buff = main_buff;
				last_indx = mess_len * 8 - LED_SIZE * LED_NUM - 1;
				flags[U_EEPROM] = false;
			}
			TCCR1B |= (1 << CS11); // start timer again
		}
	}
}


/* 10ms interrupts */
ISR(TIMER1_COMPA_vect, ISR_BLOCK)
{
	timer++;
	if (timer % tm.screen == 0)
		flags[U_SCREEN] = true;
	if (timer % tm.temp == 0)
		flags[U_TEMP] = true;
	if (timer % tm.photo == 0) {
		if (br_mode == AUTO)
			ADCSRA |= (1 << ADSC); // start photo conversion
	}
}


/* u_screen - updates panels according to the mode flag */
static void u_screen(enum display_modes flag)
{
	if (flag == CLOCK_HH) {
		clock.hh = ds1307_get_hours();
		clock.mm = ds1307_get_minutes();
		u_time(clock.hh, clock.mm);
	} else if (flag == CLOCK_SS) {
		clock.ss = ds1307_get_seconds();
		clock.mm = ds1307_get_minutes();
		u_time(clock.mm, clock.ss);
	} else if (flag == TEMP) {
		u_temp(25);
	} else if (flag == TEST) {

	}
}


/* update time on display */
static void u_time(uint8_t left, uint8_t right)
{
	uint8_t clock_arr[4] = {left / 10U, left % 10U, right / 10U, right % 10};
	for (uint8_t i = 0; i < LED_NUM; i++) {
		uint8_t num[8];
		for (uint8_t j = 0; j < 8; j++) {
			num[j] = pgm_read_byte(&NUM_ARR[clock_arr[i]][j]);
		}
		max7219_send_char_to(i, num);
	}
}


static void u_temp(uint8_t t)
{
	uint8_t t_arr[2] = {t / 10U, t % 10U};
	uint8_t num[8];
	/* send temp */
	for (uint8_t i = 0; i < 2; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			num[j] = pgm_read_byte(&NUM_ARR[t_arr[i]][j]);
		}
		max7219_send_char_to(i, num);
	}
	/* send 'C' */
	for (uint8_t j = 0; j < 8; j++) {
		num[j] = pgm_read_byte(&NUM_ARR[10][j]);
	}
	max7219_send_char_to(3, num);
	max7219_clear_panels(2);
}


static void u_photo(void)
{
	uint16_t ph = 0;
	for (uint8_t i = 0; i < PHOTO_MEASURE_SAMPLES; i++)
		ph += photo_avg[i];
	ph /= PHOTO_MEASURE_SAMPLES;
	/************************************************************************************/
	// char photo[20];
	// sprintf(photo, "photo = %u\n", ph / 17);
	// uart_send(photo);
	/************************************************************************************/
	/* 255 / 17 == 15, 0 - 15 are all possible intensity levels */
	max7219_cmd_to(ALL, MAX7219_INTENSITY_REG, ph / 17);
}
