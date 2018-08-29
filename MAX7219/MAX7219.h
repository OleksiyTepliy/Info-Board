
#define LED_NUM 4       // number of LED panels
#define LED_SIZE 8      // size of LED panels
#define ALL 0xAA        // allows send data to all panels simultaneously
#define CS_LOW  PORTB &= ~(1 << PB2)
#define CS_HIGH PORTB |= (1 << PB2)


enum MAX7219_registers {
    MAX7219_NOOP_REG = 0x00,
    MAX7219_ROW_1_REG = 0x01,
    MAX7219_ROW_2_REG = 0x02,
    MAX7219_ROW_3_REG = 0x03,
    MAX7219_ROW_4_REG = 0x04,
    MAX7219_ROW_5_REG = 0x05,
    MAX7219_ROW_6_REG = 0x06,
    MAX7219_ROW_7_REG = 0x07,
    MAX7219_ROW_8_REG = 0x08,
    MAX7219_DECODE_MODE_REG = 0x9,
    MAX7219_INTENSITY_REG = 0xA,
    MAX7219_SCAN_LIMIT_REG = 0xB,
    MAX7219_SHUTDOWN_REG = 0xC,
    MAX7219_DISPLAY_TEST_REG = 0xF
};


/**
 * MAX7219_Init - Sets MAX7219 default settings.
 * 
 */
void max7219_Init(void);


/**
 * MAX7219_clear_panels - clear all panels
 * @panel_num: number of the panel, can be set to ALL if need to clean all panels.
 */
void max7219_clear_panels(uint8_t panel_num);


/**
 * max7219_send_char_to - sends a character to the panel
 * @num: number of panel. @num can be set as macro ALL, this option allows send data to all panels.
 * @data: pointer to 8 byte data array
 * 
 */
void max7219_send_char_to(uint8_t num, const uint8_t *data);


/**
 * max7219_cmd_to - sends a command to the panel.
 * @num: number of panel. Can be set as ALL, this option allows send data to all panels.
 * @reg: max7219 register.
 * @data: data that will be stored in register.
 * 
 */
void max7219_cmd_to(uint8_t num, enum MAX7219_registers reg, uint8_t data);


/**
 * update_screen - update all led panels according to screenbuff array.
 * @screenbuff: array that holds new state of panels.
 * 
 */
void update_screen(uint8_t *screenbuff);


// /**
//  * trans_panel - transform array of columns into array of rows, that can be sent to max7219
//  * @array: source array
//  * @fill_array: array for max7219
//  */
// void trans_panel(uint8_t *array, uint8_t *fill_array);