/* Dates (proleptic Gregorian calendar), stopwatch and countdown. Plain C,
 * no UI. Day numbers count from 1970-01-01 = 0. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int y, m, d;
} dc_date_t;

bool dc_valid(dc_date_t d);                /* years 1..9999 */
int dc_days_in_month(int y, int m);
bool dc_leap(int y);
int64_t dc_to_days(dc_date_t d);
dc_date_t dc_from_days(int64_t days);
int dc_weekday(dc_date_t d);               /* 0 = Monday .. 6 = Sunday */
const char *dc_weekday_name(int wd);       /* "Monday" */
const char *dc_month_name(int m);          /* "January" */
int dc_day_of_year(dc_date_t d);           /* 1..366 */
int dc_iso_week(dc_date_t d, int *iso_year);
/* Parse "YYYYMMDD" digits (what the keypad types) or "YYYY-MM-DD". */
bool dc_parse(const char *s, dc_date_t *out);
void dc_format(dc_date_t d, char *out, size_t n);          /* "2026-11-10" */
void dc_format_long(dc_date_t d, char *out, size_t n);     /* "Tuesday 10 November 2026" */

/* The whole years, months and days from a to b (a <= b), as people count
 * birthdays. A month on from the 31st is the last day of a shorter month,
 * so a birthday on 29 February counts on 28 February in other years.
 * false if b < a. */
bool dc_age(dc_date_t a, dc_date_t b, int *years, int *months, int *days);
/* Weekdays (Monday to Friday) from a up to but not including b. */
int64_t dc_workdays(dc_date_t a, dc_date_t b);

/* ---- stopwatch -------------------------------------------------------------- */
#define SW_MAX_LAPS 99
typedef struct {
    bool running;
    int64_t start_us;       /* when it was last started */
    int64_t banked_us;      /* time from earlier runs */
    int nlaps;
    int64_t laps[SW_MAX_LAPS];   /* the total at each lap press */
} sw_t;
void sw_reset(sw_t *s);
void sw_start(sw_t *s, int64_t now_us);
void sw_stop(sw_t *s, int64_t now_us);
int64_t sw_elapsed(const sw_t *s, int64_t now_us);
bool sw_lap(sw_t *s, int64_t now_us);      /* false when the lap list is full or stopped */

/* ---- countdown -------------------------------------------------------------- */
typedef struct {
    bool running;
    int64_t duration_us;
    int64_t end_us;         /* while running */
    int64_t left_us;        /* while paused */
} cd_t;
void cd_set(cd_t *c, int64_t duration_us);
void cd_start(cd_t *c, int64_t now_us);
void cd_pause(cd_t *c, int64_t now_us);
int64_t cd_left(const cd_t *c, int64_t now_us);   /* 0 when done */
bool cd_done(const cd_t *c, int64_t now_us);
