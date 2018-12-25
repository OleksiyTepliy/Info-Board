#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "applicationTimer.h"

#define NULL (0)

static volatile uint16_t timeStamp = 0;
static applicationTimerCallback timerCallBack = NULL;

// TODO: add period value dependency
bool applicationTimerInit(uint16_t timerPeriod, applicationTimerCallback Callback)
{
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

void applicationTimerEnable(void)
{
	TCCR2B |= (1 << CS20) | (1 << CS22); // prescaler 128
}

void applicationTimerDisable(void)
{
	TCCR2B &= (~(1 << CS20) | ~(1 << CS22));
}

uint16_t applicationTimerGetTick(void)
{
    uint16_t temp = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        temp = timeStamp;
    }
    return temp;
}

ISR(TIMER2_COMPA_vect)
{
	timeStamp++;
	if (timerCallBack)
    	timerCallBack(timeStamp);
}