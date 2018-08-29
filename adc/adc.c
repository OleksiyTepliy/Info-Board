#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "Board_Info.h"

extern volatile uint8_t temp;
extern volatile bool flags[];

void adc_init()
{
	ADMUX |= (1 << REFS0); //AV CC with external capacitor at AREF pin
	//ADLAR right adjustment
	//ADMUX 0000 ADC0 configured
	//ADMUX |= 1 << MUX3 for temp sensor
	ADCSRA |= (1 << ADEN) | (1<<ADLAR) | (1 << ADIE) | (1<<ADSC); // ADC Interrupt Enable
	ADCSRA |= (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2); // ADC Prescaler Select to 128
	//ADCSRB = 0; // free mode 
}

ISR(ADC_vect, ISR_BLOCK)
{
	temp = ADCH;
	flags[U_PHOTO] = true;
}

