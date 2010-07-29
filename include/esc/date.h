/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef DATE_H_
#define DATE_H_

#include <esc/common.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	u8 sec;
	u8 min;
	u8 hour;
	/* 1 - 7; 1 = sunday */
	u8 weekDay;
	/* 1 - 31 */
	u8 monthDay;
	/* 1 - 12 */
	u8 month;
	/* full year */
	u16 year;
	/* 1 - 365 */
	u16 yearDay;
	/* the timestamp of the date */
	u32 timestamp;
} sCMOSDate;

typedef u32 tTime;
typedef struct sDate sDate;

struct sDate {
/* public: */
	const u8 sec;
	const u8 min;
	const u8 hour;
	/* 1 - 7; 1 = sunday */
	const u8 weekDay;
	/* 1 - 31 */
	const u8 monthDay;
	/* 1 - 12 */
	const u8 month;
	/* full year */
	const u16 year;
	/* 1 - 365 */
	const u16 yearDay;
	/* whether daylightsaving time is enabled */
	const u8 isDst;
	const tTime timestamp;

	void (*set)(sDate *d,tTime ts);
	s32 (*format)(sDate *d,char *str,u32 max,const char *fmt);
};

/**
 * @return the current unix-timestamp
 */
tTime getTime(void);

/**
 * Fills the given date-struct with the current date- and time information
 *
 * @param date the date-struct to fill
 * @return 0 on success
 */
s32 getDate(sCMOSDate *date);

/**
 * Creates a date-object (on the stack, nothing to free) for the current date
 *
 * @return the date-object
 * @throws DateException if the date couldn't been read from CMOS
 */
sDate date_get(void);

/**
 * Creates a date-object (on the stack, nothing to free) for the given timestamp
 *
 * @param ts the timestamp
 * @return the date-object
 */
sDate date_getOfTS(tTime ts);

/**
 * Creates a date-object (on the stack, nothing to free) for the given date and time=0:0:0
 *
 * @param month the month (1..12)
 * @param day the day (1..31)
 * @param year the year (full)
 * @return the date-object
 */
sDate date_getOfDate(u8 month,u8 day,u16 year);

/**
 * Creates a date-object (on the stack, nothing to free) for the current date and given time
 *
 * @param hour the hour
 * @param min the minute
 * @param sec the second
 * @return the date-object
 * @throws DateException if the date couldn't been read from CMOS
 */
sDate date_getOfTime(u8 hour,u8 min,u8 sec);

/**
 * Creates a date-object (on the stack, nothing to free) for the given date
 *
 * @param month the month (1..12)
 * @param day the day (1..31)
 * @param year the year (full)
 * @param hour the hour
 * @param min the minute
 * @param sec the second
 * @return the date-object
 */
sDate date_getOfDateTime(u8 month,u8 day,u16 year,u8 hour,u8 min,u8 sec);

#if 0
/* all information about the current date and time */
typedef struct {
	u8 sec;
	u8 min;
	u8 hour;
	/* 1 - 7; 1 = sunday */
	u8 weekDay;
	/* 1 - 31 */
	u8 monthDay;
	/* 1 - 12 */
	u8 month;
	/* full year */
	u16 year;
	/* 1 - 365 */
	u16 yearDay;
	/* whether daylightsaving time is enabled */
	u8 isDst;
} sDate;

/**
 * Copies into str the content of format, expanding its format tags into the corresponding
 * values as specified by date, with a limit of max characters.
 * You can use the following:
 * 	%a	Abbreviated weekday name
 * 	%A	Full weekday name
 * 	%b	Abbreviated month name
 * 	%B	Full month name
 * 	%c	Date and time representation
 * 	%d	Day of the month (01-31)
 * 	%H	Hour in 24h format (00-23)
 * 	%I	Hour in 12h format (01-12)
 * 	%j	Day of the year (001-366)
 * 	%m	Month as a decimal number (01-12)
 * 	%M	Minute (00-59)
 * 	%p	AM or PM designation
 * 	%S	Second (00-61)
 * 	%U	Week number with the first Sunday as the first day of week one (00-53)
 * 	%w	Weekday as a decimal number with Sunday as 0 (0-6)
 * 	%W	Week number with the first Monday as the first day of week one (00-53)
 * 	%x	Date representation
 * 	%X	Time representation
 * 	%y	Year, last two digits (00-99)
 * 	%Y	Year
 * 	%Z	Timezone name or abbreviation
 * 	%%	A % sign
 *
 * @param str the string to fill
 * @param max the maximum number of chars to write to str
 * @param fmt the format
 * @param date the time-information
 * @return the number of written chars, if it fits, zero otherwise
 */
s32 dateToString(char *str,u32 max,const char *fmt,sDate *date);

/**
 * @return the current unix-timestamp
 */
u32 getTime(void);

/**
 * @param date the date
 * @return the unix-timestamp for the given date
 */
u32 getTimeOf(const sDate *date);

/**
 * Fills the given date-struct with the current date- and time information
 *
 * @param date the date-struct to fill
 * @return 0 on success
 */
s32 getDate(sDate *date);

/**
 * Stores the date- and time-information in the given date-structure from the given timestamp
 *
 * @param date the date-struct to fill
 * @param timestamp the timestamp to use
 */
void getDateOf(sDate *date,u32 timestamp);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DATE_H_ */
