#include "datecalc.h"
#include <stdio.h>
#include <string.h>

bool dc_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

int dc_days_in_month(int y, int m)
{
    static const int dim[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1 || m > 12) return 0;
    return m == 2 && dc_leap(y) ? 29 : dim[m - 1];
}

bool dc_valid(dc_date_t d)
{
    return d.y >= 1 && d.y <= 9999 && d.m >= 1 && d.m <= 12 && d.d >= 1 && d.d <= dc_days_in_month(d.y, d.m);
}

/* Howard Hinnant's days_from_civil and civil_from_days. */
int64_t dc_to_days(dc_date_t dt)
{
    int64_t y = dt.y - (dt.m <= 2);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (dt.m + (dt.m > 2 ? -3 : 9)) + 2) / 5 + dt.d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

dc_date_t dc_from_days(int64_t z)
{
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int64_t d = doy - (153 * mp + 2) / 5 + 1;
    int64_t m = mp < 10 ? mp + 3 : mp - 9;
    return (dc_date_t){ (int)(y + (m <= 2)), (int)m, (int)d };
}

int dc_weekday(dc_date_t d)
{
    int64_t z = dc_to_days(d);          /* 1970-01-01 was a Thursday (3) */
    int64_t w = (z + 3) % 7;
    return (int)(w < 0 ? w + 7 : w);
}

const char *dc_weekday_name(int wd)
{
    static const char *const n[7] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
    return wd >= 0 && wd < 7 ? n[wd] : "?";
}

const char *dc_month_name(int m)
{
    static const char *const n[12] = { "January", "February", "March", "April", "May", "June", "July",
                                       "August", "September", "October", "November", "December" };
    return m >= 1 && m <= 12 ? n[m - 1] : "?";
}

int dc_day_of_year(dc_date_t d)
{
    return (int)(dc_to_days(d) - dc_to_days((dc_date_t){ d.y, 1, 1 })) + 1;
}

int dc_iso_week(dc_date_t d, int *iso_year)
{
    /* The week with the year's first Thursday is week 1. */
    int64_t days = dc_to_days(d);
    int wd = dc_weekday(d);
    dc_date_t thu = dc_from_days(days - wd + 3);
    int64_t jan1 = dc_to_days((dc_date_t){ thu.y, 1, 1 });
    if (iso_year) *iso_year = thu.y;
    return (int)((days - wd + 3 - jan1) / 7) + 1;
}

bool dc_parse(const char *s, dc_date_t *out)
{
    char digits[9];
    int n = 0;
    if (!s) return false;
    for (; *s; s++) {
        if (*s == '-' || *s == '/' || *s == ' ') continue;
        if (*s < '0' || *s > '9' || n >= 8) return false;
        digits[n++] = *s;
    }
    if (n != 8) return false;
    digits[8] = '\0';
    dc_date_t d = { (digits[0] - '0') * 1000 + (digits[1] - '0') * 100 + (digits[2] - '0') * 10 + (digits[3] - '0'),
                    (digits[4] - '0') * 10 + (digits[5] - '0'), (digits[6] - '0') * 10 + (digits[7] - '0') };
    if (!dc_valid(d)) return false;
    *out = d;
    return true;
}

void dc_format(dc_date_t d, char *out, size_t n)
{
    snprintf(out, n, "%04d-%02d-%02d", d.y, d.m, d.d);
}

void dc_format_long(dc_date_t d, char *out, size_t n)
{
    snprintf(out, n, "%s %d %s %d", dc_weekday_name(dc_weekday(d)), d.d, dc_month_name(d.m), d.y);
}

/* a moved on by `months`, the day clamped to the month's length. */
static dc_date_t add_months(dc_date_t a, int months)
{
    int idx = a.y * 12 + (a.m - 1) + months;
    dc_date_t r = { idx / 12, idx % 12 + 1, a.d };
    int dim = dc_days_in_month(r.y, r.m);
    if (r.d > dim) r.d = dim;
    return r;
}

bool dc_age(dc_date_t a, dc_date_t b, int *years, int *months, int *days)
{
    int64_t da = dc_to_days(a), db = dc_to_days(b);
    if (db < da) return false;
    int total = (b.y - a.y) * 12 + (b.m - a.m);
    if (dc_to_days(add_months(a, total)) > db) total--;
    int64_t rest = db - dc_to_days(add_months(a, total));
    *years = total / 12;
    *months = total % 12;
    *days = (int)rest;
    return true;
}

int64_t dc_workdays(dc_date_t a, dc_date_t b)
{
    int64_t da = dc_to_days(a), db = dc_to_days(b);
    int sign = 1;
    if (db < da) {
        int64_t t = da;
        da = db;
        db = t;
        sign = -1;
    }
    int64_t span = db - da, weeks = span / 7, count = weeks * 5;
    int wd = dc_weekday(dc_from_days(da));
    for (int64_t i = 0; i < span % 7; i++)
        if ((wd + i) % 7 < 5) count++;
    return sign * count;
}

/* ---- stopwatch ------------------------------------------------------------------ */
void sw_reset(sw_t *s) { memset(s, 0, sizeof *s); }

void sw_start(sw_t *s, int64_t now)
{
    if (s->running) return;
    s->running = true;
    s->start_us = now;
}

void sw_stop(sw_t *s, int64_t now)
{
    if (!s->running) return;
    s->banked_us += now - s->start_us;
    s->running = false;
}

int64_t sw_elapsed(const sw_t *s, int64_t now)
{
    return s->banked_us + (s->running ? now - s->start_us : 0);
}

bool sw_lap(sw_t *s, int64_t now)
{
    if (!s->running || s->nlaps >= SW_MAX_LAPS) return false;
    s->laps[s->nlaps++] = sw_elapsed(s, now);
    return true;
}

/* ---- countdown ------------------------------------------------------------------ */
void cd_set(cd_t *c, int64_t d)
{
    memset(c, 0, sizeof *c);
    c->duration_us = d < 0 ? 0 : d;
    c->left_us = c->duration_us;
}

void cd_start(cd_t *c, int64_t now)
{
    if (c->running || c->left_us <= 0) return;
    c->running = true;
    c->end_us = now + c->left_us;
}

void cd_pause(cd_t *c, int64_t now)
{
    if (!c->running) return;
    c->left_us = c->end_us - now;
    if (c->left_us < 0) c->left_us = 0;
    c->running = false;
}

int64_t cd_left(const cd_t *c, int64_t now)
{
    int64_t l = c->running ? c->end_us - now : c->left_us;
    return l < 0 ? 0 : l;
}

bool cd_done(const cd_t *c, int64_t now)
{
    return c->duration_us > 0 && cd_left(c, now) == 0;
}
