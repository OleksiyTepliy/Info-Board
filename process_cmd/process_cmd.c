#include <avr/io.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "process_cmd.h"
#include "uart.h"
#include "Board_Info.h"
#include "MAX7219.h"

extern volatile enum display_modes show_mode;
extern volatile bool flags[U_SIZE];
extern uint8_t eeprom_update_buff[MAX_MESSAGE_ARR_SIZE];
extern volatile struct timings tm;
extern volatile struct rtc clock;

extern uint8_t Rx_buff[]; // UART - receive data buffer
volatile bool tx_flag = false;


/* 9 commands */
//uint8_t *commands[] = {"help", "test", "sh", "sm", "stp", "sl", "ut", "us", "ul"};

/* array that stores help message */
char cmd_desc[] = {
	"help - shows all commands\n"
	"test - lights all panels\n"
	"sh - clock hh : mm\n"
	"sm - clock mm : ss\n"
	"stp - temperature mode\n"
	"sl - message mode\n"
	"ut - update time. ut [hhmmss]\n"
	"us - update line speed. us [1 - 10]\n"
	"ul - update message. us [message]"
};


uint16_t strsize(uint8_t *arr)
{
	return strlen((char *) arr);
}


void str_to_arr_trans(uint8_t *message, uint16_t message_len, uint8_t *arr, const uint8_t FRONT_ASCII[][8])
{
	for (uint16_t i = 0; i < message_len; i++)
		for (uint16_t j = (i << 3); j < (i << 3) + 8; j++)
			arr[j] = pgm_read_byte(&FRONT_ASCII[message[i] - OFFSET][j % 8]);       
}


void concat(uint8_t *arr, uint16_t arr_size)
{
	char *fill = "    ";
	char buff[arr_size + LED_NUM * 2 + 1];
	buff[0] = '\0';
	strcat((char *) buff, fill);
	strcat((char *) arr, fill);
	strcat((char *) buff, (char *) arr);
	strcpy((char *) arr, (char *) buff);
}


void process_command(void)
{
	char cmd[20];
	char args[MAX_MESSAGE_LEN];
	char wrong_cmd[] = "WRONG COMMAND";
	uint8_t parsed = 1;

	if (Rx_buff[2] == ' ') {
		strcpy(cmd, (char *) Rx_buff);
		cmd[2] = '\0';
		strcpy(args, (char *) Rx_buff + 2);
		parsed = 2;
	}

	if (parsed == 1) {
		if (!strcmp((char *) Rx_buff, "sl")) {
			show_mode = STRING;
			uart_send("OK");
		} else if (!strcmp((char *) Rx_buff, "sh")) {
			// show hh
			show_mode = CLOCK_HH;
			uart_send("OK");
		} else if (!strcmp((char *) Rx_buff, "ss")) {
			// show mm
			show_mode = CLOCK_SS;
			uart_send("OK");
		} else if (!strcmp((char *) Rx_buff, "stp")) {
			// show temp
			show_mode = TEMP;
			uart_send("OK");
		} else if (!strcmp((char *) Rx_buff, "test")) {
			// test panels
			uart_send("TEST");
		} else if (!strcmp((char *) Rx_buff, "help")) {
			uart_send(cmd_desc);
		} else if (!strcmp((char *) Rx_buff, "ledon")) {
			PORTB |= (1 << DDB0);
			uart_send("ON");
		} else if (!strcmp((char *) Rx_buff, "ledoff")) {
			PORTB &= ~(1 << DDB0);
			uart_send("OFF");
		} else {
			uart_send(wrong_cmd);
		}

	} else if (parsed == 2) {
		if (strcmp((char *) cmd, "ul") == 0) {
			strcpy((char *) eeprom_update_buff, args);
			uart_send((char *) eeprom_update_buff);
			flags[U_EEPROM] = true;
			uart_send("OK");
		} else if (strcmp(cmd, "ut") == 0) {
			uint32_t time = atol(args);
			if (time < 235959) {
				clock.ss = time % 100;
				time /= 100;
				clock.mm = time % 100;
				time /= 100;
				clock.hh = time;
				uart_send("OK");	/* rewrite logic */
			}
			else {
				uart_send("wrong argument");
				uart_send("example ut 123500");
			}
		} else if (!strcmp(cmd, "us")) {
			uint8_t tmp = atoi(args);
			if (tmp == 0 || tmp > 10) {
				uart_send("wrong argument");
				uart_send("accept 1 - 10");
			}
			else {
				tm.screen = tmp;
				uart_send("OK");
			}
		}
		else {
			uart_send(wrong_cmd);
		}
	}
}

