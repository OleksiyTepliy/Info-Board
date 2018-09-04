#define CMD_NUM 10

/**
 * strsize - returns length of array string
 * @arr: data array
 */
uint16_t strsize(uint8_t *arr);

/**
 * str_to_arr_trans - this function takes a string message as input, forms 
 * an array that can be displayed on led panels, and stores it in main_buff array.
 * @message: character message.
 * @message_len: length ot hte message.
 * @main_buff: pointer to array container.
 * 
 */
void str_to_arr_trans(uint8_t *message, uint16_t message_len, uint8_t *arr, 
                        const uint8_t FRONT_ASCII[][8]);


/**
 * concat - adds spaces in head and tail of the message
 * @arr: message
 * @arr_size: size of the message
 * 
 */
void concat(uint8_t *arr, uint16_t arr_size);

/**
 * process command - process UART commands
 * 
 */
void process_command(void);
