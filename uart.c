/* spell ok */
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "Board_Info.h"

uint8_t Rx_buff[128]; // UART - receive data buffer
uint8_t Tx_buff[128]; // UART - transmit data buffer
static uint16_t data_len = 0;
volatile uint8_t rx_idx; // Rx_buff index
volatile uint8_t tx_idx; // Tx_buff index
extern volatile bool flags[U_SIZE];
extern volatile bool tx_flag;

void uart_init(void)
{
	PRR &= ~(1 << PRUSART0); // USART enable

	//UBRR0 BAUDE_RATE
	// 8	115200
	// 12	76800
	// 34	28800
	// 103	9600

	UBRR0 = 103;
	// UBRR0H = 0x00;
	// UBRR0L = 103;

        /* Asynchronous USART, no pairity, 1 stop bit, 8 data bits */
	UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);

	/* enable receiver and transmitter modules and receiver interrupts */
	UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
}


/* USART RX Complete interrupt handler */
ISR(USART_RX_vect, ISR_BLOCK)
{
	/* Read one byte from the register */
    	uint8_t temp = UDR0;
	if(temp == '\r')
		return;
		
        if (temp == '\n') {
		/* disable receiver and rx interrupts */
		/* give some time to process the new command */
		UCSR0B &= ~(1 << RXEN0) | ~(1 << RXCIE0);
                Rx_buff[rx_idx] = '\0';
                rx_idx = 0;
                flags[U_UART] = true; // update UART flag
                return;
        }
       	Rx_buff[rx_idx++] = temp;

        /* if the message too long, start from 0 index */
        if (rx_idx >= MAX_MESSAGE_LEN)
                rx_idx = 0;
}


void uart_send(char *arr)
{
	/* copy data array to Tx_buffer */
	while(tx_flag); // waiting until the end of the previous transmission

	uint16_t i;
	for (i = 0; arr[i]; i++) {
		Tx_buff[i] = arr[i];
	}
	Tx_buff[i++] = '\n';
	Tx_buff[i] = '\0';

	/* other bytes will be sent in interrupt */
	tx_idx = 0;
	UDR0 = Tx_buff[tx_idx];
	tx_idx++;
	/* set flag */
	tx_flag = true;
	/* Enable transmitter and UDRE interrupts */
	UCSR0B |=  (1 << TXCIE0) | (1 << UDRIE0); // UDRE interrupt will be executed immediately
}

void uart_tx(uint8_t *data, uint16_t size)
{
	while(tx_flag);
	memcpy(Tx_buff, data, size);

	tx_idx = 0;
	UDR0 = Tx_buff[tx_idx];
	tx_idx++;

	tx_flag = true;
	UCSR0B |=  (1 << TXCIE0) | (1 << UDRIE0);
}

// void uart_send_byte(char byte)
// {
// 	while(tx_flag);
// 	tx_idx = 0;
// 	Tx_buff[0] = '\n';
// 	Tx_buff[1] = '\0';
// 	UDR0 = byte;
// 	tx_flag = true;
// 	UCSR0B |=  (1 << TXCIE0) | (1 << UDRIE0);
// }

/* test func */
// void uart_send(uint8_t *data)
// {
// 	uint16_t len = strlen((char *) data);
// 	for (uint8_t i = 0; i < len; i++) {
// 		while (!(UCSR0A & (1 << UDRE0)));
// 		UDR0 = data[i];
// 	}
// }


/* USART TX Complete interrupt handler */
ISR(USART_TX_vect, ISR_BLOCK)
{
	/* data has been sent, disable interrupt */
	UCSR0B &= ~(1 << TXCIE0);
	tx_flag = false; // finish
}


/* USART Data Register Empty interrupt handler */
ISR(USART_UDRE_vect, ISR_BLOCK)
{
	/* we need to write a new data byte */
	if (tx_flag && Tx_buff[tx_idx] != '\0') {
		UDR0 = Tx_buff[tx_idx++]; 
		return;
	}
	// if (tx_idx < data_len) {
	// 	UDR0 = Tx_buff[tx_idx++];
	// 	return;
	// }
	/* if all data has been sent need to disable this interrupt */
	UCSR0B &= ~(1 << UDRIE0);
}

