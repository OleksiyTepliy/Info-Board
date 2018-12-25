
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

/* display states */
typedef enum DISPLAY_MODE {
	DISPLAY_MODE_CLOCK_HH = 0,
	DISPLAY_MODE_CLOCK_SS,
	DISPLAY_MODE_STRING,
	DISPLAY_MODE_TEMP,
	DISPLAY_MODE_CANDLE,
	DISPLAY_MODE_TEST,
	DISPLAY_MODE_MODES_COUNT
} DISPLAY_MODE;

/* event flags */
typedef enum EVENT_FLAGS {
	EVENT_BRIGHTNESS,
	EVENT_BATTERY,
	EVENT_ALARM,
	EVENT_UART,
	EVENT_EEPROM,
	EVENT_COUNT
} EVENT_FLAGS;

typedef enum UPDATE_TIMINGS {
	UPDATE_CLOCK_HH,
	UPDATE_CLOCK_SS,
	UPDATE_STRING,
	UPDATE_TEMP,
	UPDATE_CANDLE,
	UPDATE_TEST,
	UPDATE_BRIGHTNESS,
	UPDATE_SETTINGS,
	UPDATE_COUNT
} UPDATE_TIMINGS;

/* brightness modes */
typedef enum BRIGHTNESS_MODE {
	MINIMAL = 0x00,
	LOW = 0x04,
	MEDIUM = 0x08,
	HIGH = 0x12,
	MAXIMAL = 0x15,
	AUTO,
} BRIGHTNESS_MODE;

typedef struct RTC_DATA {
	uint8_t hh;
	uint8_t mm;
	uint8_t ss;
} RTC_DATA;