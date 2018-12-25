#ifndef F_CPU
#define F_CPU   16000000UL
#endif

#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PROGMEM
#define pgm_read_byte(addr) ({uint8_t byte__ = *(addr); byte__; }) 
#endif


#define MOVE_TO_LEFT(num, pos) ((num) <<= (pos))
#define MOVE_TO_RIGHT(num, pos) ((num) >>= (pos))

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <avr/eeprom.h>
#include "Board_Info.h"
#include "spi.h"
#include "front.h"
#include "MAX7219.h"
#include "uart.h"
#include "process_cmd.h"
#include "adc.h"
#include "i2c.h"
#include "DS1307.h"
#include "gpio.h"
#include "encoder.h"
#include "timers.h"

static void onApplicationTimerEventCallback(uint16_t timeStamp);
static void onEncoderButtonShortPressEventCallback(void);
static void onEncoderButtonLongPressEventCallBack(void);
static void onEncoderRotaryEventCallback(void);
static void updateTime();
static void updateTimeSettings();
static void u_temp(uint8_t t);
static void processTestMode(void);

volatile bool flags[UPDATE_COUNT] = {0};
uint8_t EEMEM eeprom_buff[MAX_MESSAGE_ARR_SIZE];
uint16_t EEMEM eeprom_buff_size;
uint8_t eeprom_update_buff[MAX_MESSAGE_ARR_SIZE] = {0};
volatile uint8_t photo_avg[PHOTO_MEASURE_SAMPLES] = {0};
static volatile DISPLAY_MODE activeDisplayMode = DISPLAY_MODE_CLOCK_SS; // DISPLAY_MODE_TEST;
static volatile bool settingsEnabled = false, settingsApply = false;
volatile BRIGHTNESS_MODE brightnessLevel = MINIMAL;
static uint16_t *encoderCounter = NULL;
static volatile uint8_t panelIndex = 0;

static RTC_DATA clock = {
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

int main(void)
{
	bool initStatus = true;
	SPI_MasterInit(); // pin 10 CS, pin 11 MOSI, pin 13 SCLK
	uart_init();
	i2c_init();
	adc_init();
	max7219_Init(brightnessLevel);
	max7219_clear_panels(ALL);
	
	// TODO: check status for all inits

	initStatus &= encoderInit(PIN_2, PIN_3, PIN_4, onEncoderButtonShortPressEventCallback,
							  onEncoderButtonLongPressEventCallBack, onEncoderRotaryEventCallback);
	encoderEnableButtonIsr(true);
	encoderEnableRotaryIsr(true);
	uint16_t timerPeriod = 1;
	initStatus &= applicationTimerInit(timerPeriod, onApplicationTimerEventCallback);

	/* Led Init */
	initStatus |= gpioPinInit(PIN_8, GPIO_PIN_OUTPUT, GPIO_PIN_STATE_LOW);
	// TODO: add watchdog to reboot device if we stuck here, or reset manualy.
	while(!initStatus);

	//set_sleep_mode(SLEEP_MODE_PWR_SAVE);
	//sleep_enable();	/* Set SE (sleep enable bit) */
	sei();
	//ds1307_reset(); //do it after interrupts are enable

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

	applicationTimerStart();

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
			applicationTimerStop();
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
			applicationTimerStart();
		}
		//sleep_cpu();	/* Put MCU to sleep */
		//sleep_disable();	/* Disable sleeps for safety */
	}
}

static void onApplicationTimerEventCallback(uint16_t timeStamp)
{
	uint16_t timeTick = timeStamp;
	if (timeTick % timings[activeDisplayMode] == 0)
		flags[activeDisplayMode] = true;
	
	if (brightnessLevel == AUTO)
		if (timeTick % timings[UPDATE_BRIGHTNESS] == 0)
			ADCSRA |= (1 << ADSC); // start photo conversion
}

static void onEncoderButtonShortPressEventCallback(void)
{
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

static void onEncoderButtonLongPressEventCallBack(void)
{
	if (!settingsEnabled) {
		gpioPinSetState(PIN_8, GPIO_PIN_STATE_HIGH);
		settingsEnabled = true;
		encoderEnableRotaryIsr(true);
	} else {
		gpioPinSetState(PIN_8, GPIO_PIN_STATE_LOW);
		settingsEnabled = false;
		settingsApply = true;	
		encoderEnableRotaryIsr(false);
	}
}

static void onEncoderRotaryEventCallback(void)
{
	if (gpioPinGetState(PIN_4) && gpioPinGetState(PIN_3))
		*encoderCounter = --(*encoderCounter) % 60;
	else
		*encoderCounter = ++(*encoderCounter) % 60;
}

static void sendFunc (uint8_t panelNumber, uint8_t value) 
{
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


// static void u_photo(void)
// {
// 	uint16_t ph = 0;
// 	for (uint8_t i = 0; i < PHOTO_MEASURE_SAMPLES; i++)
// 		ph += photo_avg[i];
// 	ph /= PHOTO_MEASURE_SAMPLES;
// 	/************************************************************************************/
// 	// char photo[20];
// 	// sprintf(photo, "photo = %u\n", ph / 17);
// 	// uart_send(photo);
// 	/************************************************************************************/
// 	/* 255 / 17 == 15, 0 - 15 are all possible intensity levels */
// 	max7219_cmd_to(ALL, MAX7219_INTENSITY_REG, ph / 17);
// }
