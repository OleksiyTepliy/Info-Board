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
#include "gpio.h"

static volatile uint16_t timeTick = 0;
volatile bool flags[UPDATE_COUNT] = {0};
static volatile bool holdEventHappend = false;
uint8_t EEMEM eeprom_buff[MAX_MESSAGE_ARR_SIZE];
uint16_t EEMEM eeprom_buff_size;
uint8_t eeprom_update_buff[MAX_MESSAGE_ARR_SIZE] = {0};
volatile uint8_t photo_avg[PHOTO_MEASURE_SAMPLES] = {0};
static volatile DISPLAY_MODE activeDisplayMode = DISPLAY_MODE_CLOCK_SS; // DISPLAY_MODE_TEST;
static volatile bool settingsEnabled = false, settingsApply = false;
volatile BRIGHTNESS_MODE brightnessLevel = MINIMAL;
static volatile uint8_t *encoderCounter = NULL;
static const uint16_t debounce = 40;
static volatile uint8_t panelIndex = 0;

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
	[UPDATE_TEST] = 20,
	[UPDATE_BRIGHTNESS] = 30000,
	[UPDATE_SETTINGS] = 300,
};

/**
 * u_screen - updates panels according to the mode flag.
 */
static void u_screen();

/**
 * u_time - updates time on display according to the mode flag.
 */
static void updateTime();

static void updateTimeSettings();
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

static void processTestMode(void);

static void mainTimerInit(void)
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

static void mainTimerEnable(void)
{
	TCCR2B |= (1 << CS20) | (1 << CS22); // prescaler 128
}

static void mainTimerDisable(void)
{
	TCCR2B &= (~(1 << CS20) | ~(1 << CS22));
}

static void holdButtonTimerInit()
{	// Timer1 init
	TCCR1A |= 1 << COM1A1; // Clear OC0A on compare match
	TCCR1B |= 1 << WGM12; // mode 4 CTC
	TIMSK1 |= 1 << OCIE1A; // OCIE0A overflow interrupt
	OCR1A = 32200 - 1; // 2Hz
}

static void holdButtonTimerStart()
{
	TCCR1B &= ~((1 << CS10) | (1 << CS12));
	TCCR1B |= (1 << CS10) | (1 << CS12);
}

static void holdButtonTimerStop()
{
	TCCR1B &= ~((1 << CS10) | (1 << CS12));
}

static void encoderInit(void)
{
	DDRD &= ~(1 << DDD2) | ~(1 << DDD3) | ~(1 << DDD4); // configure as inputs
	PORTD |= (1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4); // enable pull up resistors
	EICRA = 0x00;
	EICRA |= (1 << ISC00); // INT0 - PD2 - logical change - button
	EICRA |= (1 << ISC10) | (1 << ISC11); // INT1 - PD3 - rising edge - encoder
	EIMSK |= (1 << INT0); // enable button interrupts
}

int main(void)
{
	SPI_MasterInit(); // pin 10 CS, pin 11 MOSI, pin 13 SCLK
	uart_init();
	i2c_init();
	adc_init();
	mainTimerInit();
	holdButtonTimerInit();
	max7219_Init(brightnessLevel);
	max7219_clear_panels(ALL);
	encoderInit();
	//set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	//sleep_enable();	/* Set SE (sleep enable bit) */
	sei();
	//ds1307_reset(); //do it after interrupts are enable
	
	/* Led Init */
	bool status = gpioPinInit(PIN_8, GPIO_PIN_OUTPUT, GPIO_PIN_STATE_LOW);
	while(!status);

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

	mainTimerEnable();

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
			if (settingsEnabled || settingsApply)
				updateTimeSettings();
			else
				updateTime();
			flags[UPDATE_CLOCK_HH] = flags[UPDATE_CLOCK_SS] = false;
		}

		if (flags[UPDATE_TEMP]) {
			/********** ONEWIRE START TEMP **********/
			u_temp(25);
			flags[UPDATE_TEMP] = false;
		}

		if (flags[UPDATE_TEST]) {
			/********** ONEWIRE START TEMP **********/
			processTestMode();
			flags[UPDATE_TEST] = false;
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
			mainTimerDisable();
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
			mainTimerEnable();
		}
		//sleep_cpu();	/* Put MCU to sleep */
		//sleep_disable();	/* Disable sleeps for safety */
	}
}

/* 1ms interrupts */
ISR(TIMER2_COMPA_vect)
{
	timeTick++;
	if (timeTick % timings[activeDisplayMode] == 0)
		flags[activeDisplayMode] = true;
	
	if (brightnessLevel == AUTO)
		if (timeTick % timings[UPDATE_BRIGHTNESS] == 0)
			ADCSRA |= (1 << ADSC); // start photo conversion
}

ISR(TIMER1_COMPA_vect)
{
	holdButtonTimerStop();
	holdEventHappend = true;
	if (!settingsEnabled) {
		gpioPinSetState(PIN_8, GPIO_PIN_STATE_HIGH);
		settingsEnabled = true;
		EIMSK |= (1 << INT1); // enable encoder interrupts
	} else {
		gpioPinSetState(PIN_8, GPIO_PIN_STATE_LOW);
		settingsEnabled = false;
		settingsApply = true;	
		EIMSK &= ~(1 << INT1); // disable encoder interrupts
	}
}

ISR(INT0_vect) // PD2 interrupt, button.
{
	static uint16_t buttonLastTick = 0;
	uint16_t currentTick = timeTick;
	uint8_t buttonPinState = PIND & (1 << PIND2);

	if (!buttonPinState && currentTick - buttonLastTick > debounce) {
		holdButtonTimerStart();
	} else if (buttonPinState && currentTick - buttonLastTick > debounce) {
		if (holdEventHappend) { // to discard button release after long press
			holdEventHappend = false;
			return;
		}
		holdButtonTimerStop();
		switch (activeDisplayMode) {
		case DISPLAY_MODE_STRING:
			activeDisplayMode = DISPLAY_MODE_CLOCK_HH;
			break;
		case DISPLAY_MODE_CLOCK_HH:
			if (settingsEnabled)
				panelIndex += 2;
			else
				activeDisplayMode = DISPLAY_MODE_CLOCK_SS;
			break;
		case DISPLAY_MODE_CLOCK_SS:
			if (settingsEnabled)
				panelIndex += 2; // index tells witch variable on witch panels to updade
			else
				activeDisplayMode = DISPLAY_MODE_TEMP;
			break;
		case DISPLAY_MODE_TEST:
			activeDisplayMode = DISPLAY_MODE_CLOCK_HH; //DISPLAY_MODE_TEST;
			break;
		case DISPLAY_MODE_TEMP:
		case DISPLAY_MODE_CANDLE:
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
	uint16_t currentTick = timeTick;
	if (encoderCounter == NULL)
		return;
							// TODO: debounce of encoder
	if(currentTick - encoderLastTick > debounce) {
		if ((PIND & (1 << PIND4)) && (PIND & (1 << PIND3))) {
			*encoderCounter = --(*encoderCounter) % 60;
		} else {
			*encoderCounter = ++(*encoderCounter) % 60;
		}
	}
	encoderLastTick = currentTick;
}

static void sendFunc (uint8_t panelNumber, uint8_t value) {
	uint8_t num[8];
	for (uint8_t j = 0; j < 8; j++) { // j - column number
		num[j] = pgm_read_byte(&NUM_ARR[value][j]);
	}
	max7219_send_char_to(panelNumber, num);
}

static uint8_t value = 5;

static void processTestMode()	// works, value change
{
	sendFunc(3, 8);
	encoderCounter = &value;
	sendFunc(0, *encoderCounter);
	EIMSK |= (1 << INT1); // enable encoder interrupts
}

static void updateTimeSettings()
{
	static uint16_t period = 0;
	period += timings[UPDATE_CLOCK_HH];
	if (period < timings[UPDATE_SETTINGS])
		return;
	period = 0;

	switch(activeDisplayMode) {
		case DISPLAY_MODE_CLOCK_HH:
			encoderCounter = (panelIndex % LED_NUM == 0) ? &clock.hh : &clock.mm;
			break;
		case DISPLAY_MODE_CLOCK_SS:
			encoderCounter = (panelIndex % LED_NUM == 0) ? &clock.mm : &clock.ss;
			break;
		default:
			break;
	}
	static uint8_t panelState = 0x01;
	panelState ^= 0x01;

	if (settingsApply) {
		ds1307_set_hours(clock.hh);
		ds1307_set_minutes(clock.mm);
		ds1307_set_seconds(clock.ss);
		encoderCounter = NULL;
		settingsApply = false;
		panelState = 0x01;
		panelIndex = 0;
		max7219_cmd_to(ALL, MAX7219_SHUTDOWN_REG, 0x01);
		return;
	}

	static bool blinkDots = true; // blink dots every second
	blinkDots = !blinkDots;

	sendFunc(panelIndex, *encoderCounter / 10);
	sendFunc(panelIndex + 1, *encoderCounter % 10);

	max7219_cmd_to(panelIndex % LED_NUM, MAX7219_SHUTDOWN_REG, panelState); // toggle panel state
	max7219_cmd_to((panelIndex + 1) % LED_NUM, MAX7219_SHUTDOWN_REG, panelState); 
}

/* update time on display */
static void updateTime()
{	
	uint8_t currentTick = ds1307_get_seconds();
	if (clock.ss == currentTick)
		return;
	clock.ss = currentTick;
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
	
	static bool blinkDots = true; // blink dots every second
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

static void u_temp(uint8_t tempValue)
{
	uint8_t tempData[2] = {tempValue / 10U, tempValue % 10U};
	uint8_t num[8];
	/* send temp */
	for (uint8_t i = 0; i < 2; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			num[j] = pgm_read_byte(&NUM_ARR[tempData[i]][j]);
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
