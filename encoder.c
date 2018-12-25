#include <stdbool.h>
#include <avr/interrupt.h>
#include "gpio.h"
#include "encoder.h"

static bool isEncoderInited = false;
static bool isEncoderCounterSet = false;
static uint16_t *encoderCounter = 0;
static uint8_t encoderDebunceTime = 0;
static uint16_t timeTick = 0;

bool encoderInit(GpioBoardPin encoderButton, GpioBoardPin channelA, GpioBoardPin channelB, 
                 uint8_t debounceTime)
{
    bool isEncoderInited = gpioPinInit(encoderButton, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= gpioPinInit(channelA, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= gpioPinInit(channelB, GPIO_PIN_INPUT, GPIO_PULL_UP_ENABLE);
    isEncoderInited &= debounceTime ? (encoderDebunceTime = debounceTime) : false;
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

bool encoderSetCounter(uint16_t *valuePtr)
{
    if (isEncoderInited) {
        encoderCounter = valuePtr;
        return isEncoderCounterSet = true;
    }
    return isEncoderCounterSet = false;
}

ISR(INT1_vect) // PD3 interrupt, encoder.
{
	static uint16_t encoderLastTick = 0;
	uint16_t currentTick = 1; //timeTick;
    if (!isEncoderCounterSet)
        return;
							// TODO: debounce of encoder
	if(currentTick - encoderLastTick > encoderDebunceTime) {
        // ((PIND & (1 << PIND4)) && (PIND & (1 << PIND3)))
		if (gpioPinGetState(PIN_4) && gpioPinGetState(PIN_3))
			*encoderCounter = --(*encoderCounter) % 60;
		else
			*encoderCounter = ++(*encoderCounter) % 60;
	}
	encoderLastTick = currentTick;
}


