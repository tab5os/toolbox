/* Date and time: days between dates, adding days, the day of the week, age,
 * a stopwatch and a countdown timer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <t5/audio.h>
#include <t5/power.h>
#include "app.h"
#include "engine/datecalc.h"

/* Today as typed digits, "20261110", or "" if the clock isn't set. */
static void today_digits(char *out, size_t n)
{
    struct tm tm;
    out[0] = '\0';
    if (!t5_time_valid() || !t5_time_local(&tm)) return;
    snprintf(out, n, "%04d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

static bool get_date(form_t *f, int i, dc_date_t *d)
{
    return dc_parse(form_text(f, i), d);
}

static void date_error(form_t *f, int i)
{
    dc_date_t d;
    form_set_error(f, i, strlen(form_text(f, i)) == 8 && !get_date(f, i, &d));
}

/* A "Today" button for the selected date field. */
static void today_clicked(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    char t[12];
    today_digits(t, sizeof t);
    if (!t[0]) {
        ui_toast("The clock isn't set yet");
        return;
    }
    int i = form_active(f);
    if (i < 0 || !form_is_date(f, i)) return;
    form_set_text(f, i, t);
    form_recompute(f);
}

static void today_button(form_t *f, lv_obj_t *parent)
{
    lv_obj_t *b = md_button_create(parent, "Today", MD_BUTTON_OUTLINED);
    lv_obj_add_event_cb(b, today_clicked, LV_EVENT_CLICKED, f);
}

static form_t *date_form(tool_page_t *tp, form_cb_t cb, void *user)
{
    form_t *f = form_create(tp, cb, KP_NUM);
    if (f) form_set_user(f, user);
    return f;
}

/* ---- days between ----------------------------------------------------------------- */
typedef struct {
    int a, b, days, weeks, ymd, work;
} diff_t;

static void diff_compute(form_t *f)
{
    diff_t *d = form_user(f);
    dc_date_t a, b;
    char s[80];
    date_error(f, d->a);
    date_error(f, d->b);
    if (!get_date(f, d->a, &a) || !get_date(f, d->b, &b)) {
        int outs[4] = { d->days, d->weeks, d->ymd, d->work };
        for (int i = 0; i < 4; i++) form_set_out(f, outs[i], "-");
        return;
    }
    int64_t n = dc_to_days(b) - dc_to_days(a);
    snprintf(s, sizeof s, "%lld day%s", (long long)n, n == 1 || n == -1 ? "" : "s");
    form_set_out(f, d->days, s);
    int64_t an = n < 0 ? -n : n;
    snprintf(s, sizeof s, "%lld weeks %lld days", (long long)(an / 7), (long long)(an % 7));
    form_set_out(f, d->weeks, s);
    int y, m, dd;
    if (dc_age(n < 0 ? b : a, n < 0 ? a : b, &y, &m, &dd)) {
        snprintf(s, sizeof s, "%d years %d months %d days", y, m, dd);
        form_set_out(f, d->ymd, s);
    }
    snprintf(s, sizeof s, "%lld", (long long)dc_workdays(a, b));
    form_set_out(f, d->work, s);
}

void datediff_open(tool_page_t *tp)
{
    diff_t *d = calloc(1, sizeof *d);
    if (!d) return;
    tp->data = d;
    form_t *f = date_form(tp, diff_compute, d);
    if (!f) return;
    char t[12];
    today_digits(t, sizeof t);
    d->a = form_input(f, "From", "", F_DATE, t);
    d->b = form_input(f, "To", "", F_DATE, "20270101");
    today_button(f, form_extra(f));
    d->days = form_output(f, "Difference");
    d->weeks = form_output(f, "In weeks");
    d->ymd = form_output(f, "In years, months and days");
    d->work = form_output(f, "Weekdays (Mon-Fri), end day not counted");
    form_ready(f);
}

/* ---- add days ---------------------------------------------------------------------- */
typedef struct {
    int a, n, res, wk;
} add_t;

static void add_compute(form_t *f)
{
    add_t *d = form_user(f);
    dc_date_t a;
    double n;
    char s[64];
    date_error(f, d->a);
    if (!get_date(f, d->a, &a) || !form_get(f, d->n, &n) || n > 3e6 || n < -3e6) {
        form_set_out(f, d->res, "-");
        form_set_out(f, d->wk, "-");
        return;
    }
    dc_date_t r = dc_from_days(dc_to_days(a) + (int64_t)n);
    if (!dc_valid(r)) {
        form_set_out(f, d->res, "Out of range");
        return;
    }
    dc_format_long(r, s, sizeof s);
    form_set_out(f, d->res, s);
    int iy;
    int w = dc_iso_week(r, &iy);
    snprintf(s, sizeof s, "Week %d of %d, day %d of the year", w, iy, dc_day_of_year(r));
    form_set_out(f, d->wk, s);
}

void dateadd_open(tool_page_t *tp)
{
    add_t *d = calloc(1, sizeof *d);
    if (!d) return;
    tp->data = d;
    form_t *f = date_form(tp, add_compute, d);
    if (!f) return;
    char t[12];
    today_digits(t, sizeof t);
    d->a = form_input(f, "Start", "", F_DATE, t);
    today_button(f, form_extra(f));
    d->n = form_input(f, "Days to add (" MINUS " to subtract)", "days", F_INT, "100");
    form_set_max_len(f, d->n, 7);
    d->res = form_output(f, "Date");
    form_set_value_font(f, d->res, 48, true);
    d->wk = form_output(f, "Week");
    form_ready(f);
}

/* ---- day of the week ---------------------------------------------------------------- */
typedef struct {
    int a, wd, info, leap;
} wd_t;

static void wd_compute(form_t *f)
{
    wd_t *d = form_user(f);
    dc_date_t a;
    char s[80];
    date_error(f, d->a);
    if (!get_date(f, d->a, &a)) {
        form_set_out(f, d->wd, "-");
        form_set_out(f, d->info, "-");
        form_set_out(f, d->leap, "-");
        return;
    }
    form_set_out(f, d->wd, dc_weekday_name(dc_weekday(a)));
    int iy, w = dc_iso_week(a, &iy);
    snprintf(s, sizeof s, "Day %d, ISO week %d of %d", dc_day_of_year(a), w, iy);
    form_set_out(f, d->info, s);
    snprintf(s, sizeof s, "%d is %sa leap year; %s has %d days", a.y, dc_leap(a.y) ? "" : "not ",
             dc_month_name(a.m), dc_days_in_month(a.y, a.m));
    form_set_out(f, d->leap, s);
}

void weekday_open(tool_page_t *tp)
{
    wd_t *d = calloc(1, sizeof *d);
    if (!d) return;
    tp->data = d;
    form_t *f = date_form(tp, wd_compute, d);
    if (!f) return;
    char t[12];
    today_digits(t, sizeof t);
    d->a = form_input(f, "Date", "", F_DATE, t[0] ? t : "20000101");
    today_button(f, form_extra(f));
    d->wd = form_output(f, "Day of the week");
    form_set_value_font(f, d->wd, 68, false);
    d->info = form_output(f, "In the year");
    d->leap = form_output(f, "Year and month");
    form_set_value_font(f, d->leap, 32, true);
    form_ready(f);
}

/* ---- age --------------------------------------------------------------------------- */
typedef struct {
    int born, on, age, total, next;
} age_t;

static void age_compute(form_t *f)
{
    age_t *d = form_user(f);
    dc_date_t a, b;
    char s[80];
    date_error(f, d->born);
    date_error(f, d->on);
    int y, m, dd;
    if (!get_date(f, d->born, &a) || !get_date(f, d->on, &b) || !dc_age(a, b, &y, &m, &dd)) {
        form_set_out(f, d->age, get_date(f, d->born, &a) && get_date(f, d->on, &b) ? "Born after that date" : "-");
        form_set_out(f, d->total, "-");
        form_set_out(f, d->next, "-");
        return;
    }
    snprintf(s, sizeof s, "%d years, %d months, %d days", y, m, dd);
    form_set_out(f, d->age, s);
    int64_t days = dc_to_days(b) - dc_to_days(a);
    snprintf(s, sizeof s, "%lld days, about %lld weeks", (long long)days, (long long)(days / 7));
    form_set_out(f, d->total, s);
    /* The next birthday: the day the whole years go up by one. */
    dc_date_t nb = { a.y + y + 1, a.m, a.d };
    if (!dc_valid(nb)) nb.d = dc_days_in_month(nb.y, nb.m);
    int64_t to = dc_to_days(nb) - dc_to_days(b);
    if (m == 0 && dd == 0) snprintf(s, sizeof s, "Today! Turning %d", y);
    else snprintf(s, sizeof s, "In %lld days, a %s (turning %d)", (long long)to, dc_weekday_name(dc_weekday(nb)), y + 1);
    form_set_out(f, d->next, s);
}

void age_open(tool_page_t *tp)
{
    age_t *d = calloc(1, sizeof *d);
    if (!d) return;
    tp->data = d;
    form_t *f = date_form(tp, age_compute, d);
    if (!f) return;
    char t[12];
    today_digits(t, sizeof t);
    d->born = form_input(f, "Born", "", F_DATE, "19900515");
    d->on = form_input(f, "On", "", F_DATE, t);
    today_button(f, form_extra(f));
    d->age = form_output(f, "Age");
    form_set_value_font(f, d->age, 48, true);
    d->total = form_output(f, "In days");
    d->next = form_output(f, "Next birthday");
    form_ready(f);
}

/* ---- stopwatch -------------------------------------------------------------------- */
/* Kept while the app runs, so leaving the page doesn't stop it. */
static sw_t s_sw;

typedef struct {
    lv_obj_t *time, *start, *lap, *laps, *start_l, *lap_l;
    lv_timer_t *tick;
    int shown_laps;
} swp_t;

static void sw_buttons(swp_t *p)
{
    lv_label_set_text(p->start_l, s_sw.running ? "Stop" : "Start");
    lv_label_set_text(p->lap_l, s_sw.running ? "Lap" : "Reset");
}

static void sw_show(swp_t *p)
{
    char s[32];
    fmt_clock(sw_elapsed(&s_sw, t5_time_us()) / 10000, s, sizeof s);
    ui_set_text(p->time, s);
    if (p->shown_laps == s_sw.nlaps) return;
    lv_obj_clean(p->laps);
    /* Newest first, with each lap's own time and the total. */
    for (int i = s_sw.nlaps - 1; i >= 0; i--) {
        char a[24], b[24], line[64];
        fmt_clock((s_sw.laps[i] - (i ? s_sw.laps[i - 1] : 0)) / 10000, a, sizeof a);
        fmt_clock(s_sw.laps[i] / 10000, b, sizeof b);
        snprintf(line, sizeof line, "Lap %d      %s      %s", i + 1, a, b);
        ui_label(p->laps, line, 32, md_scheme()->on_surface.medium);
    }
    p->shown_laps = s_sw.nlaps;
}

static void sw_tick(lv_timer_t *t)
{
    sw_show(lv_timer_get_user_data(t));
}

static void sw_start_clicked(lv_event_t *e)
{
    swp_t *p = lv_event_get_user_data(e);
    int64_t now = t5_time_us();
    if (s_sw.running) sw_stop(&s_sw, now);
    else sw_start(&s_sw, now);
    sw_buttons(p);
    sw_show(p);
}

static void sw_lap_clicked(lv_event_t *e)
{
    swp_t *p = lv_event_get_user_data(e);
    if (s_sw.running) sw_lap(&s_sw, t5_time_us());
    else sw_reset(&s_sw);
    sw_buttons(p);
    sw_show(p);
}

static void sw_close(tool_page_t *tp)
{
    swp_t *p = tp->data;
    if (p && p->tick) lv_timer_delete(p->tick);
}

static lv_obj_t *big_button(lv_obj_t *parent, const char *text, md_button_variant_t v, lv_obj_t **label)
{
    lv_obj_t *b = ui_text_button(parent, text, v == MD_BUTTON_CONTAINED, label);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, T5_DP(56));
    return b;
}

void stopwatch_open(tool_page_t *tp)
{
    swp_t *p = calloc(1, sizeof *p);
    if (!p) return;
    tp->data = p;
    tp->on_close = sw_close;
    p->shown_laps = -1;
    lv_obj_t *body = tp->body;
    lv_obj_set_style_pad_all(body, T5_DP(16), 0);
    lv_obj_set_style_pad_row(body, T5_DP(12), 0);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    p->time = ui_label(body, "00:00.00", 68, LV_OPA_COVER);
    lv_obj_set_style_transform_scale(p->time, 384, 0);       /* 1.5x the 68 px font */
    lv_obj_set_style_transform_pivot_x(p->time, LV_PCT(50), 0);
    lv_obj_set_style_transform_pivot_y(p->time, LV_PCT(50), 0);
    lv_obj_set_style_pad_ver(p->time, T5_DP(20), 0);
    lv_obj_t *row = ui_box(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_max_width(row, T5_DP(420), 0);
    lv_obj_set_style_pad_column(row, T5_DP(12), 0);
    p->lap = big_button(row, "Reset", MD_BUTTON_OUTLINED, &p->lap_l);
    p->start = big_button(row, "Start", MD_BUTTON_CONTAINED, &p->start_l);
    lv_obj_add_event_cb(p->start, sw_start_clicked, LV_EVENT_CLICKED, p);
    lv_obj_add_event_cb(p->lap, sw_lap_clicked, LV_EVENT_CLICKED, p);
    p->laps = ui_box(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(p->laps, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(p->laps, T5_DP(4), 0);
    if (!p->start_l || !p->lap_l) return;
    sw_buttons(p);
    sw_show(p);
    p->tick = lv_timer_create(sw_tick, 50, p);
}

/* ---- countdown --------------------------------------------------------------------- */
static cd_t s_cd;
static volatile bool s_alarm_on, s_alarm_task;
static t5_wakelock_t *s_cd_lock;

/* Beeps on a task: writing audio blocks. Stops when told or after 30 s. */
static void alarm_task(void *arg)
{
    (void)arg;
    t5_audio_out_cfg_t cfg = { .rate = 16000, .channels = 1 };
    t5_audio_out_t *o = NULL;
    if (t5_audio_out_open_usage(&cfg, T5_AUDIO_USAGE_ALERT, &o) != T5_OK) o = NULL;
    static int16_t buf[1600];                      /* 100 ms */
    int64_t end = t5_time_us() + 30 * 1000000LL;
    uint32_t phase = 0;
    for (int k = 0; o && s_alarm_on && t5_app_running(t5_app_self()) && t5_time_us() < end; k++) {
        /* Beep beep beep, pause: 100 ms blocks, a tone in 3 of every 8. */
        bool tone = (k % 8) < 5 && (k % 2) == 0;
        for (int i = 0; i < 1600; i++) {
            phase += (uint32_t)(((uint64_t)1760 << 32) / 16000);
            int32_t p = (int32_t)(phase >> 16) - 32768;
            int32_t tri = (p < 0 ? -p : p) * 2 - 32768;
            buf[i] = tone ? (int16_t)(tri / 3) : 0;
        }
        if (t5_audio_out_write(o, buf, sizeof buf, 100) < 0) break;
    }
    if (o) T5_IGNORE(t5_audio_out_close(o));     /* nothing to do if closing fails */
    s_alarm_task = false;
}

static void alarm_start(void)
{
    s_alarm_on = true;
    if (s_alarm_task) return;
    s_alarm_task = true;
    t5_task_t h;
    if (t5_task_create("tb_alarm", alarm_task, NULL, 6144, 2, T5_CORE_ANY, &h) != T5_OK) s_alarm_task = false;
}

static void cd_lock(bool on)
{
    /* Keep the screen on while a timer runs: LVGL timers stop when it goes off. */
    if (on && !s_cd_lock) s_cd_lock = t5_wakelock_acquire(T5_LOCK_SCREEN, "toolbox timer");
    if (!on && s_cd_lock) {
        t5_wakelock_release(s_cd_lock);
        s_cd_lock = NULL;
    }
}

typedef struct {
    int h, m, s;
    lv_obj_t *left, *start_l, *bar, *done;
    lv_timer_t *tick;
    form_t *f;
} cdp_t;

static int64_t cd_typed_us(form_t *f, cdp_t *p)
{
    double h = 0, m = 0, s = 0;
    form_get(f, p->h, &h);
    form_get(f, p->m, &m);
    form_get(f, p->s, &s);
    return (int64_t)((h * 3600 + m * 60 + s) * 1e6);
}

static void cd_show(cdp_t *p)
{
    int64_t now = t5_time_us();
    int64_t left = s_cd.duration_us ? cd_left(&s_cd, now) : cd_typed_us(p->f, p);
    long long t = (left + 999999) / 1000000;                   /* whole seconds, rounded up */
    char s[32];
    if (t >= 3600) snprintf(s, sizeof s, "%lld:%02lld:%02lld", t / 3600, t / 60 % 60, t % 60);
    else snprintf(s, sizeof s, "%02lld:%02lld", t / 60, t % 60);
    ui_set_text(p->left, s);
    int32_t pct = s_cd.duration_us ? (int32_t)(1000 - left * 1000 / s_cd.duration_us) : 0;
    if (lv_bar_get_value(p->bar) != pct) lv_bar_set_value(p->bar, pct, LV_ANIM_OFF);
    ui_set_text(p->start_l, s_cd.running ? "Pause" : s_cd.duration_us && left > 0 && left < s_cd.duration_us
                                                       ? "Resume" : "Start");
    lv_obj_set_hidden(p->done, !(s_cd.duration_us && cd_done(&s_cd, now)));
}

/* Watches a running timer for the whole app, so it goes off on any page. */
static lv_timer_t *s_cd_watch;

static void cd_watch(lv_timer_t *t)
{
    int64_t now = t5_time_us();
    if (s_cd.running && !cd_done(&s_cd, now)) return;
    if (s_cd.running) {
        s_cd.running = false;
        s_cd.left_us = 0;
        alarm_start();
        tool_page_t *tp = tool_current();
        if (!tp || strcmp(tp->def->id, "time.countdown") != 0) ui_toast("Timer: time's up");
    }
    cd_lock(false);
    lv_timer_delete(t);
    s_cd_watch = NULL;
}

static void cd_tick(lv_timer_t *t)
{
    cd_show(lv_timer_get_user_data(t));
}

static void cd_compute(form_t *f)
{
    cdp_t *p = form_user(f);
    /* Typing sets a new time; a running or paused timer is kept. */
    if (!s_cd.running && (cd_done(&s_cd, t5_time_us()) || s_cd.left_us == s_cd.duration_us)) cd_set(&s_cd, 0);
    if (p->left) cd_show(p);
}

static void cd_start_clicked(lv_event_t *e)
{
    cdp_t *p = lv_event_get_user_data(e);
    int64_t now = t5_time_us();
    s_alarm_on = false;
    if (s_cd.running) {
        cd_pause(&s_cd, now);
        cd_lock(false);
    } else {
        if (!s_cd.duration_us || cd_done(&s_cd, now)) cd_set(&s_cd, cd_typed_us(p->f, p));
        if (!s_cd.duration_us) {
            ui_toast("Set a time first");
            return;
        }
        cd_start(&s_cd, now);
        cd_lock(true);
        if (!s_cd_watch) s_cd_watch = lv_timer_create(cd_watch, 100, NULL);
    }
    cd_show(p);
}

static void cd_reset_clicked(lv_event_t *e)
{
    cdp_t *p = lv_event_get_user_data(e);
    s_alarm_on = false;
    cd_set(&s_cd, 0);
    cd_lock(false);
    cd_show(p);
}

static void cd_preset(lv_event_t *e)
{
    cdp_t *p = lv_event_get_user_data(e);
    lv_obj_t *chip = lv_event_get_target_obj(e);
    static const char *const mins[] = { "1", "3", "5", "10", "15", "30" };
    int i = ui_chip_index(chip);
    md_chip_set_selected(chip, false);
    if (i < 0 || i > 5 || s_cd.running) return;
    form_set_text(p->f, p->h, "0");
    form_set_text(p->f, p->m, mins[i]);
    form_set_text(p->f, p->s, "0");
    form_recompute(p->f);
}

static void cd_close(tool_page_t *tp)
{
    cdp_t *p = tp->data;
    if (p && p->tick) lv_timer_delete(p->tick);
    s_alarm_on = false;
    /* A running timer keeps going (and the screen on) until it ends. */
}

void countdown_open(tool_page_t *tp)
{
    cdp_t *p = calloc(1, sizeof *p);
    if (!p) return;
    tp->data = p;
    tp->on_close = cd_close;
    form_t *f = form_create(tp, cd_compute, KP_NUM);
    if (!f) return;
    p->f = f;
    form_set_user(f, p);
    lv_obj_t *top = form_extra(f);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    p->left = ui_label(top, "00:00", 68, LV_OPA_COVER);
    p->bar = lv_bar_create(top);
    lv_bar_set_range(p->bar, 0, 1000);
    lv_obj_set_size(p->bar, LV_PCT(100), T5_DP(6));
    lv_obj_set_style_bg_color(p->bar, md_color(md_scheme()->c.primary), LV_PART_INDICATOR);
    p->done = md_label_create(top, "Time's up", MD_TYPE_HEADLINE6);
    lv_obj_set_style_text_color(p->done, md_color(md_scheme()->c.secondary), 0);
    lv_obj_t *row = ui_box(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_pad_column(row, T5_DP(8), 0);
    lv_obj_t *reset = big_button(row, "Reset", MD_BUTTON_OUTLINED, NULL);
    lv_obj_t *start = big_button(row, "Start", MD_BUTTON_CONTAINED, &p->start_l);
    lv_obj_add_event_cb(start, cd_start_clicked, LV_EVENT_CLICKED, p);
    lv_obj_add_event_cb(reset, cd_reset_clicked, LV_EVENT_CLICKED, p);
    static const char *const presets[] = { "1 min", "3 min", "5 min", "10 min", "15 min", "30 min" };
    lv_obj_t *chips = ui_chips(top, presets, 6, -1, NULL, NULL);
    for (uint32_t i = 0; i < lv_obj_get_child_count(chips); i++)
        lv_obj_add_event_cb(lv_obj_get_child(chips, (int32_t)i), cd_preset, LV_EVENT_CLICKED, p);
    p->h = form_input(f, "Hours", "h", F_UNSIGNED, "0");
    p->m = form_input(f, "Minutes", "min", F_UNSIGNED, "5");
    p->s = form_input(f, "Seconds", "s", F_UNSIGNED, "0");
    form_set_max_len(f, p->h, 2);
    form_set_max_len(f, p->m, 3);
    form_set_max_len(f, p->s, 4);
    ui_note(form_extra(f), "The screen stays on while the timer runs, and it keeps running if you go back. "
                           "It stops if you leave Toolbox.");
    if (!p->start_l) return;
    form_ready(f);
    p->tick = lv_timer_create(cd_tick, 200, p);
    cd_show(p);
}
