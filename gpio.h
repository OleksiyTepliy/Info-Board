typedef enum GpioBoardPin {
    PIN_0 = 0,
    PIN_1,
    PIN_2,
    PIN_3,
    PIN_4,
    PIN_5,
    PIN_6,
    PIN_7,
    PIN_8,
    PIN_9,
    PIN_10,
    PIN_11,
    PIN_12,
    PIN_13,
    PIN_A0,
    PIN_A1,
    PIN_A2,
    PIN_A3,
    PIN_A4,
    PIN_A5,
    GPIO_PIN_COUNT,
} GpioBoardPin;

typedef enum GpioPinDirection {
    GPIO_PIN_INPUT = 0x00,
    GPIO_PIN_OUTPUT = 0x01,
} GpioPinDirection;

typedef enum GpioPinState {
    GPIO_PIN_STATE_LOW = 0x00,
    GPIO_PIN_STATE_HIGH = 0x01,
    GPIO_PULL_UP_DISABLE = 0x00,
    GPIO_PULL_UP_ENABLE = 0x01,    // can be enabled only when pin configured as input
    GPIO_PIN_ERROR = 0x02,
} GpioPinState;

bool gpioPinInit(GpioBoardPin, GpioPinDirection dir, GpioPinState pinState);
bool gpioPinSetState(GpioBoardPin, GpioPinState pinState);
GpioPinState gpioPinGetState(GpioBoardPin);