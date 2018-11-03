#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"
#include "DS1307.h"

bool ds1307_reset(void)
{
    uint8_t null_arr[DS1307_REGISTER_COUNT] = {0};
    return i2c_send(DS1307_ADDRESS, DS1307_SECONDS_REGISTER, &null_arr[0], sizeof(null_arr));
}

bool ds1307_halt(bool stoped)
{
    if (stoped) {
        uint8_t ch_bit_mask = 1U << 7;
        return i2c_send(DS1307_ADDRESS, DS1307_SECONDS_REGISTER, &ch_bit_mask, sizeof(ch_bit_mask));
    }
    else {
        return i2c_send(DS1307_ADDRESS, DS1307_SECONDS_REGISTER, 0x00, 1);
    }
}

bool ds1307_set_seconds(uint8_t seconds)
{
    if (seconds > 58)
        return false;
    uint8_t units = seconds % 10;
    uint8_t tens = (seconds / 10) & 0x07;
    seconds = units | (tens << 4);
    return i2c_send(DS1307_ADDRESS, DS1307_SECONDS_REGISTER, &seconds, sizeof(seconds)); 
}

uint8_t ds1307_get_seconds(void)
{
    uint8_t seconds = 0x00;
    i2c_read(DS1307_ADDRESS, DS1307_SECONDS_REGISTER, &seconds, sizeof(seconds));
    return (((seconds & 0x70) >> 4) * 10 + (seconds & 0x0F));
}

bool ds1307_set_minutes(uint8_t minutes)
{
    if (minutes > 59) {
        return false;
    }
    uint8_t units = minutes % 10;
    uint8_t tens = (minutes / 10) & 0x07;
    minutes = units | (tens << 4);
    return i2c_send(DS1307_ADDRESS, DS1307_MINUTES_REGISTER, &minutes, sizeof(minutes));
}

uint8_t ds1307_get_minutes(void)
{
    uint8_t minutes = 0x00;
    i2c_read(DS1307_ADDRESS, DS1307_MINUTES_REGISTER, &minutes, sizeof(minutes));
    return (((minutes & 0x70) >> 4) * 10 + (minutes & 0x0F));
}

bool ds1307_set_hours(uint8_t hours)
{
    if (hours > 23) {
        return false;
    }
    uint8_t units = hours % 10;
    uint8_t tens = (hours / 10) & 0x03;
    hours = units | (tens << 4);
    return i2c_send(DS1307_ADDRESS, DS1307_HOURS_REGISTER, &hours, sizeof(hours));
}

uint8_t ds1307_get_hours(void)
{
    uint8_t hours = 0x00;
    i2c_read(DS1307_ADDRESS, DS1307_HOURS_REGISTER, &hours, sizeof(hours));
    return (((hours & 0x30) >> 4) * 10 + (hours & 0x0F));
}

bool ds1307_set_day(uint8_t day)
{
    if (day < 1 || day > 7) {
        return false;
    }
    day &= 0x07;
    return i2c_send(DS1307_ADDRESS, DS1307_DAY_REGISTER, &day, sizeof(day));
}

bool ds1307_set_date(uint8_t date)
{
    if (date > 31) {
        return false;
    }
    uint8_t units = date % 10;
    uint8_t tens = (date / 10) & 0x30;
    date = 0x00;
    date = ((tens << 4) | units);
    return i2c_send(DS1307_ADDRESS, DS1307_DATE_REGISTER, &date, sizeof(date));
}

// bool ds1307_set_mounth(uint8_t month)
// {
//     month &= 0x1F;
//     return i2c_send(DS1307_ADDRESS, DS1307_MONTH_REGISTER, &month, sizeof(month));
// }

// bool ds1307_set_year(uint8_t year)
// {
//     return i2c_send(DS1307_ADDRESS, DS1307_YEAR_REGISTER, &year, sizeof(year));
// }


