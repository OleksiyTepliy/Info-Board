
/**
 * uart_init() - init uart
 * 
 */
void uart_init(void);


/**
 * uart_send() - sends data array
 * @arr: pointer to the data array
 * 
 */
void uart_send(char *arr);

void uart_tx(uint8_t *data, uint16_t size);
