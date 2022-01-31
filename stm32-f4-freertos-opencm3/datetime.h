
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#ifndef DATETIME_H_ITU
#define DATETIME_H_ITU

time_t date_to_unixtime(const struct tm * date);
uint8_t day_of_week(uint16_t year, uint8_t mon, uint8_t mday);
void unixtime_to_date(time_t time, struct tm * date);

#endif
/* EOF */


