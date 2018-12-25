#define ENCODER_ROTARY_DEBOUNCE (2)
#define ENCODER_BUTTON_DEBOUNCE (10)
#define ENCODER_BUTTON_HOLD_TIME (2000)

typedef void (*encoderButtonShortPressCallback)(void);
typedef void (*encoderButtonLongPressCallback)(void);
typedef void (*encoderRotaryCallback)(void);

bool encoderInit(GpioBoardPin encoderButton, GpioBoardPin channelA, GpioBoardPin channelB,
                 encoderButtonShortPressCallback buttonShortPressCb, encoderButtonLongPressCallback buttonLongPressCb,
                 encoderRotaryCallback rotaryCb);
void encoderEnableRotaryIsr(bool isrEnable);
void encoderEnableButtonIsr(bool isrEnable);
