#define DS1307_ADDRESS (0x68)

enum ds1307_register {
    DS1307_SECONDS_REGISTER = 0x00,
    DS1307_MINUTES_REGISTER,
    DS1307_HOURS_REGISTER,
    DS1307_DAY_REGISTER,
    DS1307_DATE_REGISTER,
    DS1307_MONTH_REGISTER,
    DS1307_YEAR_REGISTER,
    DS1307_CONTROL_REGISTER,
    DS1307_REGISTER_COUNT
};

bool ds1307_reset(void);
bool ds1307_halt(bool stoped);
bool ds1307_set_seconds(uint8_t sec);
uint8_t ds1307_get_seconds();
bool ds1307_set_minutes(uint8_t min);
uint8_t ds1307_get_minutes(void);
bool ds1307_set_hours(uint8_t hour);
uint8_t ds1307_get_hours(void);
bool ds1307_set_day(uint8_t day);
bool ds1307_set_date(uint8_t date);
bool ds1307_set_mounth(uint8_t month);
bool ds1307_set_year(uint8_t year);
