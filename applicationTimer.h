typedef void (*applicationTimerCallback)(uint16_t timeStamp);

bool applicationTimerInit(uint16_t timerPeriod, applicationTimerCallback mainCallback);
void applicationTimerEnable(void);
void applicationTimerDisable(void);
uint16_t applicationTimerGetTick(void);

