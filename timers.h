typedef void (*applicationTimerCallback)(uint16_t timeStamp);
typedef void (*holdButtonTimerCallback)(void);

    /* Timer 2 8 bit */
bool applicationTimerInit(uint16_t timerPeriod, applicationTimerCallback callBack);
void applicationTimerStart(void);
void applicationTimerStop(void);
uint16_t applicationTimerGetTick(void);

    /* Timer 1 16 bit*/
bool holdButtonTimerInit(uint16_t timerPeriod, holdButtonTimerCallback callBack);
void holdButtonTimerStart(void);
void holdButtonTimerStop(void);

bool watchDogTimerInit(uint16_t timerPeriod);
void watchDogTimerStart(void);
void watchDogTimerStop(void);


