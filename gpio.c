#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include "gpio.h"

typedef struct GpioPinHandle {
	volatile uint8_t *DDR;	/* addr of GPIO DDRx direction register */
	volatile uint8_t *PIN;	/* addr of GPIO PINx input register */
	volatile uint8_t *PORT;	/* addr of GPIO PORTx data register */
    uint8_t pinNumber;
} GpioPinHandle;

static const GpioPinHandle gpioPinMap[GPIO_PIN_COUNT] = {
    [PIN_0]  =  {&DDRD, &PIND, &PORTD, 0},
    [PIN_1]  =  {&DDRD, &PIND, &PORTD, 1},
    [PIN_2]  =  {&DDRD, &PIND, &PORTD, 2},
    [PIN_3]  =  {&DDRD, &PIND, &PORTD, 3},
    [PIN_4]  =  {&DDRD, &PIND, &PORTD, 4},
    [PIN_5]  =  {&DDRD, &PIND, &PORTD, 5},
    [PIN_6]  =  {&DDRD, &PIND, &PORTD, 6},
    [PIN_7]  =  {&DDRD, &PIND, &PORTD, 7},
    [PIN_8]  =  {&DDRB, &PINB, &PORTB, 0},
    [PIN_9]  =  {&DDRB, &PINB, &PORTB, 1},
    [PIN_10] =  {&DDRB, &PINB, &PORTB, 2},
    [PIN_11] =  {&DDRB, &PINB, &PORTB, 3},
    [PIN_12] =  {&DDRB, &PINB, &PORTB, 4},
    [PIN_13] =  {&DDRB, &PINB, &PORTB, 5},
    [PIN_A0] =  {&DDRC, &PINC, &PORTC, 0},
    [PIN_A1] =  {&DDRC, &PINC, &PORTC, 1},
    [PIN_A2] =  {&DDRC, &PINC, &PORTC, 2},
    [PIN_A3] =  {&DDRC, &PINC, &PORTC, 3},
    [PIN_A4] =  {&DDRC, &PINC, &PORTC, 4},
    [PIN_A5] =  {&DDRC, &PINC, &PORTC, 5},
};

bool gpioPinInit(GpioBoardPin gpioPin, GpioPinDirection dir, GpioPinState pinState)
{
    if (gpioPin > GPIO_PIN_COUNT)
        return false;

    *gpioPinMap[gpioPin].DDR &= ~(1 << gpioPinMap[gpioPin].pinNumber);
    *gpioPinMap[gpioPin].DDR |= (dir << gpioPinMap[gpioPin].pinNumber);

    *gpioPinMap[gpioPin].PORT &= ~(1 << gpioPinMap[gpioPin].pinNumber);
    *gpioPinMap[gpioPin].PORT |= (pinState << gpioPinMap[gpioPin].pinNumber);

    return true;
}

bool gpioPinSetState(GpioBoardPin gpioPin, GpioPinState pinState)
{
    if (gpioPin > GPIO_PIN_COUNT)
        return false;
    
    *gpioPinMap[gpioPin].PORT &= ~(1 << gpioPinMap[gpioPin].pinNumber);
    *gpioPinMap[gpioPin].PORT |= (pinState << gpioPinMap[gpioPin].pinNumber);
    
    return true;
}

GpioPinState gpioPinGetState(GpioBoardPin gpioPin)
{
    if (gpioPin > GPIO_PIN_COUNT)
        return GPIO_PIN_ERROR;
    
    return *gpioPinMap[gpioPin].PIN &= (1 << gpioPinMap[gpioPin].pinNumber);
}