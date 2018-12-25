#include <stdbool.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "encoder.h"
#include "applicationTimer.h"

#define NULL (0)

static encoderButtonShortPressCallback buttonShortPressCallBack = NULL;
static encoderButtonLongPressCallback buttonLongPressCallBack = NULL;
static encoderRotaryCallback rotaryCallback = NULL;
static void holdButtonTimerStart();
static void holdButtonTimerStop();

static bool holdButtonTimerInit(uint16_t timerPeriod /* add long press callback */);
static bool isEncoderInited = false;

bool encoderInit(GpioBoardPin encoderButton, GpioBoardPin channelA, GpioBoardPin channelB,
                 encoderButtonShortPressCallback buttonShortPressCb, encoderButtonLongPressCallback buttonLongPressCb,
                 encoderRotaryCallback rotaryCb)
{
    bool isEncoderInited = gpioPinInit(encoderButton, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= gpioPinInit(channelA, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= gpioPinInit(channelB, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    buttonShortPressCallBack = buttonShortPressCb;
    buttonLongPressCallBack = buttonLongPressCb;
    if (buttonLongPressCallBack)
        isEncoderInited &= holdButtonTimerInit(ENCODER_BUTTON_HOLD_TIME/*, buttonLongPressCallBack */);
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

/****************************** HOLD TIMER PART ********************************/

// TODO: put this timer in sepparate module, with adjusted period and callback
static bool holdButtonTimerInit(uint16_t timerPeriod /* add long press callback */)
{
    // TODO: make depending on timerPeriod, and return proper init status
    (void) timerPeriod;
	TCCR1A |= 1 << COM1A1; // Clear OC0A on compare match
	TCCR1B |= 1 << WGM12; // mode 4 CTC
	TIMSK1 |= 1 << OCIE1A; // OCIE0A overflow interrupt
	OCR1A = 32200 - 1;
    return true;
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

/****************************** HOLD TIMER PART ********************************/

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
                if(buttonShortPressCallback)
                    buttonShortPressCallback();
        }
    }
	buttonLastTick = currentTick;
}

ISR(TIMER1_COMPA_vect) // 2sec timer interrupt
{
	holdButtonTimerStop();
    if (buttonLongPressCallback)
        buttonLongPressCallback();
}
