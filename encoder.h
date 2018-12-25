
typedef void (*encoderButtonCallback)(void);
typedef void (*encoderRotaryCallback)(void);

bool encoderInit(GpioBoardPin encoderButton, GpioBoardPin channelA, GpioBoardPin channelB,
                 encoderButtonCallback buttonCb, encoderRotaryCallback rotaryCb);
void encoderEnableRotaryIsr(bool isrEnable);
void encoderEnableButtonIsr(bool isrEnable);
