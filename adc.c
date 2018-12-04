#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "Board_Info.h"

extern volatile uint8_t photo_avg[];
extern volatile bool flags[];

void adc_init(void)
{
	ADMUX |= (1 << REFS0) | (1 << ADLAR); //AV CC with external capacitor at AREF pin, result left adjusted.
	ADCSRA |= (1 << ADIE); // ADC and Interrupt Enable.
	ADCSRA |= (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2) | (1 << ADEN); // ADC Prescaler / 128, and enable adc.
}


ISR(ADC_vect, ISR_BLOCK)
{
	static uint8_t n = 0; 
	photo_avg[n++] = ADCH;
	if (n == PHOTO_MEASURE_SAMPLES - 1) {
		flags[UPDATE_BRIGHTNESS] = true;
		n = 0;
	}
}