
bool encoderInit(GpioBoardPin encoderButton, GpioBoardPin channelA, GpioBoardPin channelB, uint8_t debounceTime);
bool encoderSetCounter(uint16_t *valuePtr);
void encoderEnableRotaryIsr(bool isrEnable);
void encoderEnableButtonIsr(bool isrEnable);
