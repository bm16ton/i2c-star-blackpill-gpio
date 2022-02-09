
/* Author, Copyright: Oleg Borodin <onborodin@gmail.com> 2018 */

#include <stdint.h>
#include <time.h>

#include <datetime.h>

time_t date_to_unixtime(const struct tm * date) {
    uint32_t year;
    uint32_t mon;
    uint32_t day;
    uint32_t time;

    year = date->tm_year;
    mon = date->tm_mon;
    day = date->tm_mday;

    /* January and February are counted as months 13 and 14 of the previous year */
    if (mon <= 2) {
        mon += 12;
        year -= 1;
    }

    /* Convert years to days */
    time = (365 * year) + (year / 4) - (year / 100) + (year / 400);
    /* Convert months to days */
    time += (30 * mon) + (3 * (mon + 1) / 5) + day;
    /* Unix time starts on January 1st, 1970 */
    time -= 719561;
    /* Convert days to seconds */
    time *= 86400;
    /* Add hours, minutes and seconds */
    time += (3600 * date->tm_hour) + (60 * date->tm_min) + date->tm_sec;

    /* Return Unix time */
    return (time_t)time;
}

uint8_t day_of_week(uint16_t year, uint8_t mon, uint8_t mday) {
    uint16_t h;
    uint16_t j;
    uint16_t k;

    /* January and February are counted as months 13 and 14 of the previous year */
    if (mon <= 2) {
        mon += 12;
        year -= 1;
    }
    /* J is the century */
    j = year / 100;
    /* K the year of the century */
    k = year % 100;

    /* Compute H using Zeller's congruence */
    h = mday + (26 * (mon + 1) / 10) + k + (k / 4) + (5 * j) + (j / 4);

    /* Return the day of the week */
    return ((h + 5) % 7) + 1;
}

void unixtime_to_date(time_t time, struct tm * date) {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;

    /* Negative Unix time values are not supported */
    if (time < 1)
        time = 0;

    /* Retrieve hours, minutes and seconds */
    date->tm_sec = time % 60;
    time /= 60;
    date->tm_min = time % 60;
    time /= 60;
    date->tm_hour = time % 24;
    time /= 24;

    /* Convert Unix time to date */
    a = (uint32_t) ((4 * time + 102032) / 146097 + 15);
    b = (uint32_t) (time + 2442113 + a - (a / 4));
    c = (20 * b - 2442) / 7305;
    d = b - 365 * c - (c / 4);
    e = d * 1000 / 30601;
    f = d - e * 30 - e * 601 / 1000;

    /* January and February are counted as months 13 and 14 of the previous year */
    if (e <= 13) {
        c -= 4716;
        e -= 1;
    } else {
        c -= 4715;
        e -= 13;
    }

    /* Retrieve year, month and day */
    date->tm_year = c;
    date->tm_mon = e;
    date->tm_mday = f;

    /* Calculate day of week */
    date->tm_wday = day_of_week(c, e, f);
}

/* EOF */
