#include <stdint.h>
#include <avr/io.h>


void SPI_MasterInit(void)
{
	/* Set CS, MOSI, and SCK pins to output */
	DDRB |= (1 << PB2) | (1 << PB3) | (1 << PB5);
	PORTB |= (1 << PB2); // CS pin need to be logic 1
	//PRR &= ~(1 << PRSPI); // enable SPI Block
	/* SPI Control Register */
	SPCR = 0; // reset register
	SPCR |= (1 << MSTR); // Master Mode
	SPCR &= ~(1 << SPR1) | ~(1 << SPR0); 
	SPSR |= (1 << SPI2X); // fosc / 2
	SPCR &= ~(1 << CPHA) | ~(1 << CPOL); // SPI Mode 0
	SPCR &= ~(1 << DORD); // MSB Mode
	//SPCR |= (1 << SPIE); // enable interrupt
	SPCR |= (1 << SPE); // enable SPI
}


/* SPI Serial Transfer Complete ISR*/
// ISR(SPI_STC_vect, ISR_BLOCK)
// {

// }


// void SPI_SlaveInit(void)
// {
// 	/* Set MISO output, all others input */
// 	DDR_SPI = (1 << DD_MISO);
// 	/* Enable SPI */
// 	SPCR = (1 << SPE);
// }


void SPI_Transmit(uint8_t data)
{
	/* Start transmission */
	SPDR = data;
	/* Wait for transmission complete */
	while(!(SPSR & (1 << SPIF)));
}


uint8_t SPI_Receive(void)
{
	/* Wait for reception complete */
	while(!(SPSR & (1 << SPIF)));
	/* Return Data Register */
	return SPDR;
}
