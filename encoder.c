#include <stdbool.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "encoder.h"
#include "applicationTimer.h"

#define NULL (0)

static encoderButtonCallback buttonCallback = NULL;
static encoderRotaryCallback rotaryCallback = NULL;

static bool isEncoderInited = false;
static uint16_t *encoderCounter = 0;
static uint8_t encoderDebunceTime = 0;

bool encoderInit(GpioBoardPin encoderButton, GpioBoardPin channelA, GpioBoardPin channelB, 
                 encoderButtonCallback buttonCb, encoderRotaryCallback rotaryCb)
{
    bool isEncoderInited = gpioPinInit(encoderButton, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= gpioPinInit(channelA, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= gpioPinInit(channelB, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    buttonCallback = buttonCb;
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
    if (rotaryCallback)
        rotaryCallback();
}

ISR(INT0_vect) // PD2 interrupt, button.
{
    if (buttonCallback)
        buttonCallback();
}

