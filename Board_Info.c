#include "Board_Info.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <avr/eeprom.h>
#include <stdint.h>
#include <stdbool.h>
#include "spi.h"
#include "front.h"
#include "MAX7219.h"
#include "uart.h"
#include "process_cmd.h"
#include "adc.h"
#include "i2c.h"
#include "DS1307.h"

static volatile uint16_t timer = 0;
volatile bool flags[U_SIZE] = {0};
bool dots_blink = false;
uint8_t EEMEM eeprom_buff[MAX_MESSAGE_ARR_SIZE];
uint16_t EEMEM eeprom_buff_size;
uint8_t eeprom_update_buff[MAX_MESSAGE_ARR_SIZE] = {0};
volatile uint8_t photo_avg[PHOTO_MEASURE_SAMPLES] = {0};
volatile enum display_modes show_mode = CLOCK_SS;
volatile enum brightness_modes br_mode = BR_0;
static volatile uint8_t counter = 0;

struct rtc clock = {
	.hh = 0,
	.mm = 0,
	.ss = 0
};

/* default events timings */
struct timings tm = { 
	.rtc = 100, // 100 ms
	.screen = 30, // 40 ms
	.temp = 3000, // 3sec
	.photo = 5000 // 5 sec,
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

static void timer_init(void)
{
		/* Timer 2 Init */
	TCCR2A = 0x00;// TC2 Control Register A
	TIMSK2 = 0x00;// Interrupt mask register
	TCCR2B = 0x00;

	TCCR2A |= (1 << WGM21); // CTC mode
	TIMSK2 |= (1 << OCIE2A); // enable interrupts
	/* f(1kHz) = 16Mhz / (prescaler * (1 + OCR2A)) */
	OCR2A = 0x7C; // 124
}

static void timer_enable(void)
{
	TCCR2B |= (1 << CS20) | (1 << CS22); // prescaler 128
}

static void timer_disable(void)
{
	TCCR2B &= (~(1 << CS20) | ~(1 << CS22));
}


int main(void)
{
	SPI_MasterInit(); // pin 10 CS, pin 11 MOSI, pin 13 SCLK
	uart_init();
	i2c_init();
	adc_init();
	timer_init();
	max7219_Init();
	max7219_clear_panels(ALL);
	set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	sleep_enable();	/* Set SE (sleep enable bit) */
	sei();
	//ds1307_reset(); //do it after interrupts are enable
	
	/* Led Init */
	DDRB |= 1 << DDB0; // pin 8 
	PORTB &= ~(1 << DDB0); // logic 0.

	DDRD &= ~(1 << DDD2) | ~(1 << DDD3) | ~(1 << DDD7); // configure as inputs
	//PORTD |= (1 << PORTD2) | (1 << PORTD3) | (1 << PORTD7); // enable pull up resistors
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT23); // pin change interrupt on PD7
	/* PD3, PD2 rising edge interrupt */
	EICRA |= (1 << ISC10) | (1 << ISC11) | (1 << ISC00) | (1 << ISC01);
	EIMSK |= (1 << INT1) | (1 << INT0);

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

	timer_enable();

	while(1) {

		if (flags[U_SCREEN] && (show_mode == STRING)) {
			if (screen_buff == main_buff + last_indx) {
				screen_buff = main_buff;
			}
			update_screen(screen_buff);
			screen_buff++;
			flags[U_SCREEN] = false;
		}

		if (flags[U_RTC] && (show_mode == CLOCK_HH || 
			show_mode == CLOCK_SS)) {
			u_screen(show_mode);
			flags[U_RTC] = false;
		} else {
			flags[U_RTC] = false;
		}

		if (flags[U_TEMP]) {
			/********** ONEWIRE START TEMP **********/
			u_temp(counter);
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
			timer_disable();
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
			timer_enable();
		}

		//sleep_cpu();	/* Put MCU to sleep */
		//sleep_disable();	/* Disable sleeps for safety */
	}
}

/* 1ms interrupts */
ISR(TIMER2_COMPA_vect)
{
	timer++;
	// if (timer % 10) {
	// 	return;
	// }

	if (timer % tm.rtc == 0) {
		flags[U_RTC] = true;
	}

	if (timer % tm.screen == 0) {
		flags[U_SCREEN] = true;
	} 

	if (timer % tm.temp == 0) {
		flags[U_TEMP] = true;
	}

	if (timer % tm.photo == 0) {
		if (br_mode == AUTO)
			ADCSRA |= (1 << ADSC); // start photo conversion
	}
}

enum rotation {
	COUNTERCLOCKWISE = 0,
	CLOCKWISE,
};

static volatile bool rotation_flag = false;
static volatile enum rotation direction;
static volatile uint16_t last_tick = 0;
static const uint8_t debounce = 1;
static volatile bool pressed = false;

ISR(PCINT2_vect)
{
	if ((timer - last_tick > debounce)) {
		uint8_t state = PIND & (1 << PIND7);
		if (!state) {
			switch (show_mode) {
			case STRING:
				show_mode = CLOCK_HH;
				break;
			case CLOCK_HH:
				show_mode = CLOCK_SS;
				break;
			case CLOCK_SS:
				show_mode = STRING;
				break;
			default:
				show_mode = STRING;
			}
		}
	}
	last_tick = timer;
}

ISR(INT0_vect) // PD2, counter_clockwise
{

}

ISR(INT1_vect) // PD3, clockwise
{
	uint8_t pd2_state = PIND & (1 << PIND2);
	//uint8_t pd3_state = PIND & (1 << PIND3);
	if (!pd2_state) {
		counter++;
		PORTB |= (1 << DDB0);
	} else {
		counter--;
		PORTB &= ~(1 << DDB0);
	}
}


/* u_screen - updates panels according to the mode flag */
static void u_screen(enum display_modes flag)
{
	clock.hh = ds1307_get_hours();
	clock.mm = ds1307_get_minutes();
	uint8_t temp = ds1307_get_seconds();
	if (temp != clock.ss) {
		clock.ss = temp;
		dots_blink = !dots_blink;
	}

	if (flag == CLOCK_HH) {
		u_time(clock.hh, clock.mm);
	} else if (flag == CLOCK_SS) {
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
			/* By default, all letter are left aligned, we left 0 and 
			1st-panel as is, and align 2 and 3rd panel to the right. 
			It prevents blinking dots overlapping */
			if (i > 1) {
				MOVE_TO_RIGHT(num[j], 2);
			}
			if (j == 1 || j == 2 || j == 5 || j == 6) { // form blinking dots
				if (i == 1) {
					if (dots_blink)
						num[j] ^= 0x01;
				} else if (i == 2) {
					if (dots_blink)
						num[j] ^= 0x80;
				}
			}
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
