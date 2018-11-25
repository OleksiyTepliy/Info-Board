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
#include "DS1307.h"

extern volatile enum display_modes show_mode;
extern volatile bool flags[U_SIZE];
extern uint8_t eeprom_update_buff[MAX_MESSAGE_ARR_SIZE];
extern volatile struct timings tm;
extern struct rtc clock;
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


void process_command(void)
{
	enum errs err = E_OK;
	char *errors[] = {
		"OK",
		"WRONG_COMMAND",
		"WRONG_ARGUMENT",
		"TO MUCH INPUT",
	};

	/* command 2 characters + space + '\0' are extra 4 characters to the input */
	if (strlen((char *) Rx_buff) > MAX_MESSAGE_LEN + 4) {
		err = E_SIZE;
		uart_send(errors[E_SIZE]);
		return;
	}

	char cmd[10] = {'\0'};
	char args[MAX_MESSAGE_LEN] = {'\0'};

	uint8_t parsed = sscanf((char *) Rx_buff, "%s %s", cmd, args);

	if (parsed == 1) {
		if (!strcmp((char *) Rx_buff, "sl")) {
			show_mode = STRING;
		} else if (!strcmp((char *) Rx_buff, "sh")) {
			show_mode = CLOCK_HH;
		} else if (!strcmp((char *) Rx_buff, "ss")) {
			show_mode = CLOCK_SS;
		} else if (!strcmp((char *) Rx_buff, "stp")) {
			show_mode = TEMP;
		} else if (!strcmp((char *) Rx_buff, "test")) {
			// test panels
		} else if (!strcmp((char *) Rx_buff, "help")) {
			for (uint8_t i = 0; i < CMD_NUM; i++) {
				uart_send(&help[i][0]);
			}
		} else {
			err = E_CMD;
		}
	} else if (parsed == 2) {
		if (strcmp((char *) cmd, "ul") == 0) {
			strcpy((char *) eeprom_update_buff, (char *) (Rx_buff + 3));
			uart_send((char *) eeprom_update_buff);
			flags[U_EEPROM] = true;
		} else if (strcmp(cmd, "ut") == 0) {
			uint32_t time;
			if (strlen(args) > 6 || (time = atol(args)) > 235958) {
				err = E_ARG;
				uart_send("example: ut 123000, sets time to 12:30:00");
			}
			else {
				if (time % 100 > 58) {
					uart_send("max seconds value 58");
					return;
				}
				ds1307_set_seconds(time % 100);
				time /= 100;			
				ds1307_set_minutes(time % 100);
				time /= 100;
				ds1307_set_hours(time);	
			}
		} else if (!strcmp(cmd, "us")) {
			uint8_t tmp = atoi(args);
			if (tmp < 10 || tmp > 100) {
				err = E_ARG;
				uart_send("accept 10 - 100 ms");
			}
			else {
				tm.screen = tmp;
			}
		} else if (!strcmp(cmd, "ub")) {
			if (!strcmp(args, "auto")) {
				br_mode = AUTO;
			}
			else {
				uint8_t tmp = atoi(args);
				if (tmp > 15) {
					err = E_ARG;
				}
				else {
					br_mode = BR_0;
					max7219_cmd_to(ALL, MAX7219_INTENSITY_REG, tmp);
				}
			}
		}
		else {
			err = E_CMD;
		}
	}
	uart_send(errors[err]);
}

