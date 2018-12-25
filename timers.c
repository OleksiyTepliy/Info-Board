#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "timers.h"

#define NULL (0)

static volatile uint16_t timeStamp = 0;
static applicationTimerCallback timerCallBack = NULL;
static holdButtonTimerCallback holdEventCallBack = NULL;

// TODO: add period value dependency
bool applicationTimerInit(uint16_t timerPeriod, applicationTimerCallback Callback)
{
	// TODO: make depending on timerPeriod, and return proper init status
    if (timerPeriod == 0)
        return false;
    
    timerCallBack = Callback;
		/* Timer 2 Init */
	TCCR2A = 0x00;// TC2 Control Register A
	TIMSK2 = 0x00;// Interrupt mask register
	TCCR2B = 0x00;

	TCCR2A |= (1 << WGM21); // CTC mode
	TIMSK2 |= (1 << OCIE2A); // enable interrupts
	/* f(1kHz) = 16Mhz / (prescaler * (1 + OCR2A)) */
	OCR2A = 0x7C; // 124
    return true;
}

// TODO: add period value dependency
bool holdButtonTimerInit(uint16_t timerPeriod, holdButtonTimerCallback callBack)
{
    // TODO: make depending on timerPeriod, and return proper init status
    (void) timerPeriod;
	TCCR1A |= 1 << COM1A1; // Clear OC0A on compare match
	TCCR1B |= 1 << WGM12; // mode 4 CTC
	TIMSK1 |= 1 << OCIE1A; // OCIE0A overflow interrupt
	OCR1A = 32200 - 1;

	holdEventCallBack = callBack;
    return true;
}

bool watchDogTimerInit(uint16_t timerPeriod)
{
	// TODO: make depending on timerPeriod, and return proper init status
	return true;
}

void applicationTimerStart(void)
{
	TCCR2B |= (1 << CS20) | (1 << CS22); // prescaler 128
}

void holdButtonTimerStart(void)
{
	TCCR1B &= ~((1 << CS10) | (1 << CS12));
	TCCR1B |= (1 << CS10) | (1 << CS12);
}

void watchDogTimerStart(void)
{

}

void applicationTimerStop(void)
{
	TCCR2B &= (~(1 << CS20) | ~(1 << CS22));
}

void holdButtonTimerStop(void)
{
	TCCR1B &= ~((1 << CS10) | (1 << CS12));
}

void watchDogTimerStop(void)
{

}

uint16_t applicationTimerGetTick(void)
{
    uint16_t temp = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        temp = timeStamp;
    }
    return temp;
}

/* Application Timer ISR */
ISR(TIMER2_COMPA_vect)
{
	timeStamp++;
	if (timerCallBack)
    	timerCallBack(timeStamp);
}

/* Hold Timer ISR */
ISR(TIMER1_COMPA_vect)
{
	holdButtonTimerStop();
    if (holdEventCallBack)
        holdEventCallBack();
}
