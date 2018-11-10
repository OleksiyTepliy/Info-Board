#ifndef F_CPU
#define F_CPU   16000000UL
#endif

#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PROGMEM
#define pgm_read_byte(addr) ({uint8_t byte__ = *(addr); byte__; }) 
#endif

#define OFFSET 32U // 32 - offset of ascii array

/* max number of characters in message */
#define MAX_MESSAGE_LEN 50U

/* LED_NUM * 2 - padding spaces + one '\0' string terminator */
#define MAX_MESSAGE_ARR_SIZE (MAX_MESSAGE_LEN + ((LED_NUM) << 1) + 1)

/* how many times to measure resistor before brightness adjustment */
/* measuring interval can be set in timings struct */
#define PHOTO_MEASURE_SAMPLES 5U

#define MOVE_TO_LEFT(num, pos) ((num) <<= (pos))
#define MOVE_TO_RIGHT(num, pos) ((num) >>= (pos))


/* event flags */
enum u_flags {
	U_RTC = 0,
	U_SCREEN,
	U_TEMP,
	U_PHOTO,
	U_UART,
	U_EEPROM,
	U_SIZE
};


/* display states */
enum display_modes {
	CLOCK_HH = 0,
	CLOCK_SS,
	STRING,
	TEMP,
	TEST,
	CANDLE
};


/* brightness modes */
enum brightness_modes {
	BR_0 = 0,
	BR_1,
	BR_2,
	BR_3,
	BR_4,
	BR_5,
	BR_6,
	BR_7,
	BR_8,
	BR_9,
	BR_10,
	BR_11,
	BR_12,
	BR_13,
	BR_14,
	BR_15,
	AUTO
};


/* default timings divided by 10, because timer interrupt rise every 10 ms */
struct timings {
	uint16_t rtc;   // 100 ms
	uint16_t screen; // 10 - 50 ms
	uint16_t temp; // 1 sec - 10sec
	uint16_t photo; // 3sec - 60sec	
};


struct rtc {
	uint8_t hh;
	uint8_t mm;
	uint8_t ss;
};
