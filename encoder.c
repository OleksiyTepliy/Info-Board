#include <stdbool.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "encoder.h"
#include "timers.h"

#define NULL (0)

static encoderButtonShortPressCallback buttonShortPressCallBack = NULL;
static encoderButtonLongPressCallback buttonLongPressCallBack = NULL;
static encoderRotaryCallback rotaryCallback = NULL;
static bool isEncoderInited = false;

bool encoderInit(GpioBoardPin encoderButton, GpioBoardPin channelA, GpioBoardPin channelB,
                 encoderButtonShortPressCallback buttonShortPressCb, encoderButtonLongPressCallback buttonLongPressCb,
                 encoderRotaryCallback rotaryCb)
{
    isEncoderInited = gpioPinInit(encoderButton, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= gpioPinInit(channelA, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= gpioPinInit(channelB, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    buttonShortPressCallBack = buttonShortPressCb;
    buttonLongPressCallBack = buttonLongPressCb;
    if (buttonLongPressCallBack)
        isEncoderInited &= holdButtonTimerInit(ENCODER_BUTTON_HOLD_TIME, buttonLongPressCallBack);
    rotaryCallback = rotaryCb;
    return isEncoderInited;
}

void encoderEnableRotaryIsr(bool isrEnable)
{
    /* Enable interrupts for encoder */
	EICRA |= (1 << ISC10) | (1 << ISC11); // INT1 - PD3 - rising edge - encoder

    if (isrEnable)
        EIMSK |= (1 << INT1);
    else
        EIMSK &= ~(1 << INT1);
}

void encoderEnableButtonIsr(bool isrEnable)
{
    EICRA |= (1 << ISC00); // INT0 - PD2 - logical change - button

    if (isrEnable)
        EIMSK |= (1 << INT0);
    else
        EIMSK &= ~(1 << INT0);
}


// TODO: make callback from main
ISR(INT1_vect) // PD3 interrupt, encoder.
{
    static uint16_t encoderLastTick = 0;
	uint16_t currentTick = applicationTimerGetTick();
		
    if(currentTick - encoderLastTick > ENCODER_ROTARY_DEBOUNCE) {
        if (rotaryCallback)
            rotaryCallback();
    }
	encoderLastTick = currentTick;
}

ISR(INT0_vect) // PD2 interrupt, button.
{
    static uint16_t buttonLastTick = 0;
	uint16_t currentTick = applicationTimerGetTick();
	bool buttonPinState = gpioPinGetState(PIN_2);
    if (currentTick - buttonLastTick > ENCODER_BUTTON_DEBOUNCE) {
        if (!buttonPinState) {
            holdButtonTimerStart();
        } else {
            holdButtonTimerStop();
            if (currentTick - buttonLastTick < ENCODER_BUTTON_HOLD_TIME)
                if(buttonShortPressCallBack)
                    buttonShortPressCallBack();
        }
    }
	buttonLastTick = currentTick;
}