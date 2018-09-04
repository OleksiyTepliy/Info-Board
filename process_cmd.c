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
extern volatile enum brightness_modes br_mode;

extern uint8_t Rx_buff[]; // UART - receive data buffer
volatile bool tx_flag = false;


/* 10 commands */
//uint8_t *commands[] = {"sh", "sm", "stp", "sl", "ut", "ub", "us", "ul", "help", "test"};

/* help message */
char *help[CMD_NUM] = {
	"sh - clock hh : mm",
	"ss - clock mm : ss",
	"stp - temperature mode",
	"sl - line mode",
	"ut - update time. ut [hh mm ss]",
	"ub - update brightness. ub [auto] or [0-15]",
	"us - update line speed. us [1-10]",
	"ul - update message. ul [message]",
	"test - lights all panels",
	"help - shows all commands"
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

static void error(void)
{	
	uart_send("WRONG COMMAND, to much input");
	return;
}


void process_command(void)
{
	char cmd[20];
	char args[MAX_MESSAGE_LEN];
	char wrong_cmd[] = "WRONG COMMAND";
	char wrong_arg[] = "WRONG ARGUMENT";
	uint8_t parsed;

	parsed = sscanf((char *) Rx_buff, "%s %s", cmd, args);
	/* command 2 characters + space + '\0' are extra 4 characters to the input */
	if (strlen((char *) Rx_buff) > MAX_MESSAGE_LEN + 4) {
		error();
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
			for (uint8_t i = 0; i < CMD_NUM; i++) {
				uart_send(&help[i][0]);
			}
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
			strcpy((char *) eeprom_update_buff, (Rx_buff + 3));
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
				uart_send(wrong_arg);
				uart_send("example ut 122500");
			}
		} else if (!strcmp(cmd, "us")) {
			uint8_t tmp = atoi(args);
			if (tmp < 1 || tmp > 10) {
				uart_send(wrong_arg);
				uart_send("accept 1 - 10");
			}
			else {
				tm.screen = tmp;
				uart_send("OK");
			}
		} else if (!strcmp(cmd, "ub")) {
			if (!strcmp(args, "auto")) {
				br_mode = AUTO;
			}
			else {
				uint8_t tmp = atoi(args);
				if (tmp > 15) {
					uart_send(wrong_arg);
				}
				else {
					br_mode = STATIC;
					max7219_cmd_to(ALL, MAX7219_INTENSITY_REG, tmp);
				}
			}
		}
		else {
			uart_send(wrong_cmd);
		}
	}
}

