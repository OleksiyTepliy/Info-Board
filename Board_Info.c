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
volatile bool flags[UPDATE_COUNT] = {0};
uint8_t EEMEM eeprom_buff[MAX_MESSAGE_ARR_SIZE];
uint16_t EEMEM eeprom_buff_size;
uint8_t eeprom_update_buff[MAX_MESSAGE_ARR_SIZE] = {0};
volatile uint8_t photo_avg[PHOTO_MEASURE_SAMPLES] = {0};
volatile DISPLAY_MODE activeDisplayMode = DISPLAY_MODE_CLOCK_SS;
volatile BRIGHTNESS_MODE brightnessLevel = MINIMAL;
static volatile uint8_t counter = 50;
static const uint16_t debounce = 5;

RTC_DATA clock = {
	.hh = 0,
	.mm = 0,
	.ss = 0
};

/* screen update timings */
static uint16_t timings[UPDATE_COUNT] = { 
	[UPDATE_CLOCK_HH] = 100,
	[UPDATE_CLOCK_SS] = 100,
	[UPDATE_STRING] = 40,
	[UPDATE_TEMP] = 50,
	[UPDATE_CANDLE] = 20,
	[UPDATE_TEST] = 0,
	[UPDATE_BRIGHTNESS] = 30000,
};

/**
 * u_screen - updates panels according to the mode flag.
 */
static void u_screen();

/**
 * u_time - updates time on display according to the mode flag.
 */
static void updateTime();

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

static void encoderInit(void)
{
	DDRD &= ~(1 << DDD2) | ~(1 << DDD3) | ~(1 << DDD4); // configure as inputs
	PORTD |= (1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4); // enable pull up resistors
	EICRA = 0x00;
	EICRA |= (1 << ISC01); // INT0 - PD2 - falling edge - button
	EICRA |= (1 << ISC10) | (1 << ISC11); // INT1 - PD3 - rising edge - encoder
	EIMSK |= (1 << INT1) | (1 << INT0); // enable interrupts
}

int main(void)
{
	SPI_MasterInit(); // pin 10 CS, pin 11 MOSI, pin 13 SCLK
	uart_init();
	i2c_init();
	adc_init();
	timer_init();
	max7219_Init(brightnessLevel);
	max7219_clear_panels(ALL);
	encoderInit();
	//set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	//sleep_enable();	/* Set SE (sleep enable bit) */
	sei();
	//ds1307_reset(); //do it after interrupts are enable
	
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

	timer_enable();

	while(1) {

		if (flags[UPDATE_STRING]) {
			if (screen_buff == main_buff + last_indx) {
				screen_buff = main_buff;
			}
			update_screen(screen_buff);
			screen_buff++;
			flags[UPDATE_STRING] = false;
		}

		if (flags[UPDATE_CLOCK_HH] || flags[UPDATE_CLOCK_SS]) {
			updateTime();
			flags[UPDATE_CLOCK_HH] = flags[UPDATE_CLOCK_SS] = false;
		}

		if (flags[UPDATE_TEMP]) {
			/********** ONEWIRE START TEMP **********/
			u_temp(counter);
			flags[UPDATE_TEMP] = false;
		}

		if (flags[EVENT_UART]) {
			process_command();
			flags[EVENT_UART] = false;
			/* after command process finished, enable reciver and rx interrupts */
			UCSR0B |= (1 << RXEN0) | (1 << RXCIE0);
		}

		if (flags[EVENT_EEPROM]) {
			if (activeDisplayMode == DISPLAY_MODE_STRING)
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
				flags[EVENT_EEPROM] = false;
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
	if (timer % timings[activeDisplayMode] == 0)
		flags[activeDisplayMode] = true;
	
	if (brightnessLevel == AUTO)
		if (timer % timings[UPDATE_BRIGHTNESS] == 0)
			ADCSRA |= (1 << ADSC); // start photo conversion
}

ISR(INT0_vect) // PD2 interrupt, button.
{
	static uint16_t buttonLastTick = 0;
	uint16_t currentTick = timer;
	if (currentTick - buttonLastTick > debounce) {
		switch (activeDisplayMode) {
		case DISPLAY_MODE_STRING:
			activeDisplayMode = DISPLAY_MODE_CLOCK_HH;
			break;
		case DISPLAY_MODE_CLOCK_HH:
			activeDisplayMode = DISPLAY_MODE_CLOCK_SS;
			break;
		case DISPLAY_MODE_CLOCK_SS:
			activeDisplayMode = DISPLAY_MODE_TEMP;
			break;
		case DISPLAY_MODE_TEMP:
		default:
			activeDisplayMode = DISPLAY_MODE_STRING;
			break;
		}
	}
	buttonLastTick = currentTick;
}

ISR(INT1_vect) // PD3 interrupt, encoder.
{
	static uint16_t encoderLastTick = 0;
	uint16_t currentTick = timer;
	if(currentTick - encoderLastTick > debounce) {
		if ((PIND & (1 << PIND4)) && (PIND & (1 << PIND3))) {
			if (counter != 0)
				counter--;
		} else {
			if (counter != 99)
				counter++;
		}
	}
	encoderLastTick = currentTick;
}

/* update time on display */
static void updateTime()
{	
	uint8_t curerntTick = ds1307_get_seconds();
	if (clock.ss == curerntTick)
		return;
	clock.ss = curerntTick;
	clock.mm = ds1307_get_minutes();
	clock.hh = ds1307_get_hours();
	
	uint8_t clock_arr[4];

	if (activeDisplayMode == DISPLAY_MODE_CLOCK_HH) {
		clock_arr[0] = clock.hh / 10U;
		clock_arr[1] = clock.hh % 10U;
		clock_arr[2] = clock.mm / 10U;
		clock_arr[3] = clock.mm % 10;
	} else if (activeDisplayMode == DISPLAY_MODE_CLOCK_SS) {
		clock_arr[0] = clock.mm / 10U;
		clock_arr[1] = clock.mm % 10;
		clock_arr[2] = clock.ss / 10U;
		clock_arr[3] = clock.ss % 10;
	} 
	
	static bool blinkDots = true;
	blinkDots = !blinkDots;

	for (uint8_t i = 0; i < LED_NUM; i++) { // i - panel number
		uint8_t num[8];
		for (uint8_t j = 0; j < 8; j++) { // j - column number
			num[j] = pgm_read_byte(&NUM_ARR[clock_arr[i]][j]);
			/* By default, all letter are left aligned, we left 0 and 
			1st-panel as is, and align 2 and 3rd panel to the right. 
			It prevents blinking dots overlapping */
			if (i > 1)
				MOVE_TO_RIGHT(num[j], 2); // num[j] - row

			if (j == 1 || j == 2 || j == 5 || j == 6) { // form blinking dots
				if (blinkDots) {
					if (i == 1)
						num[j] ^= 0x01;
					else if (i == 2)
						num[j] |= 0x80;
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
