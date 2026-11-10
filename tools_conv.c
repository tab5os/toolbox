/* The unit converters: one page for every category in engine/units.c.
 *
 * The typed value is in the "from" unit; the big result is in the "to"
 * unit, and every unit of the category is listed below it with the value
 * in that unit. Tap a unit to convert to it, hold it to convert from it, or
 * swap the two. Leaving the page records the conversion in Recent. */
#include <stdio.h>
#include <stdlib.h>
#include "app.h"

#define CONV_MAX 16

typedef struct {
    units_cat_t cat;
    int from, to;
    int in, out;
    lv_obj_t *rows[CONV_MAX], *vals[CONV_MAX], *names[CONV_MAX];
} conv_t;

static void row_style(conv_t *c, int i)
{
    const md_scheme_t *sc = md_scheme();
    lv_obj_t *r = c->rows[i];
    bool from = i == c->from, to = i == c->to;
    lv_obj_set_style_border_width(r, from ? T5_DP(2) : 0, 0);
    lv_obj_set_style_bg_color(r, md_color(to ? md_mix(sc->c.secondary, sc->c.background, 0x30)
                                             : md_mix(sc->c.on_surface, sc->c.background, 0x0a)), 0);
}

static void compute(form_t *f)
{
    conv_t *c = form_user(f);
    const unit_cat_t *cat = units_cat(c->cat);
    char num[48], buf[96];
    double v;
    bool have = form_get(f, c->in, &v);
    form_set_unit(f, c->in, cat->units[c->from].sym);
    snprintf(buf, sizeof buf, "From %s", cat->units[c->from].name);
    form_set_label(f, c->in, buf);
    snprintf(buf, sizeof buf, "To %s", cat->units[c->to].name);
    form_set_label(f, c->out, buf);
    double r;
    if (!have) {
        form_set_out(f, c->out, "-");
    } else if (units_convert(c->cat, c->from, c->to, v, &r)) {
        fmt_num(r, 10, true, num, sizeof num);
        snprintf(buf, sizeof buf, "%s %s", num, cat->units[c->to].sym);
        form_set_out(f, c->out, buf);
    } else {
        form_set_out(f, c->out, "No answer");
    }
    form_set_error(f, c->out, have && !units_convert(c->cat, c->from, c->to, v, &r));
    for (int i = 0; i < cat->count && i < CONV_MAX; i++) {
        if (have && units_convert(c->cat, c->from, i, v, &r)) fmt_num(r, 8, true, num, sizeof num);
        else snprintf(num, sizeof num, "-");
        snprintf(buf, sizeof buf, "%s %s", num, cat->units[i].sym);
        ui_set_text(c->vals[i], buf);
        row_style(c, i);
    }
}

static void save_units(form_t *f, conv_t *c)
{
    form_set_int(f, 0, c->from);
    form_set_int(f, 1, c->to);
}

static int row_index(conv_t *c, lv_obj_t *row)
{
    for (int i = 0; i < CONV_MAX; i++)
        if (c->rows[i] == row) return i;
    return -1;
}

static void unit_tapped(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    conv_t *c = form_user(f);
    int i = row_index(c, lv_event_get_current_target_obj(e));
    if (i < 0) return;
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        /* Convert from this unit: keep the quantity, show it in the new unit. */
        double v, r;
        char num[32];
        if (form_get(f, c->in, &v) && units_convert(c->cat, c->from, i, v, &r)) {
            fmt_num(r, 10, false, num, sizeof num);
            form_set_text(f, c->in, num);
        }
        if (i == c->to) c->to = c->from;
        c->from = i;
    } else {
        if (i == c->from) c->from = c->to;
        c->to = i;
    }
    save_units(f, c);
    form_recompute(f);
}

static void swap_clicked(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    conv_t *c = form_user(f);
    double v, r;
    char num[32];
    /* The result becomes the input. */
    if (form_get(f, c->in, &v) && units_convert(c->cat, c->from, c->to, v, &r)) {
        fmt_num(r, 10, false, num, sizeof num);
        form_set_text(f, c->in, num);
    }
    int t = c->from;
    c->from = c->to;
    c->to = t;
    save_units(f, c);
    form_recompute(f);
}

static void conv_close(tool_page_t *tp)
{
    form_t *f = tp->form;
    conv_t *c = tp->data;
    if (!f || !c) return;
    double v, r;
    if (!form_get(f, c->in, &v) || !units_convert(c->cat, c->from, c->to, v, &r)) return;
    const unit_cat_t *cat = units_cat(c->cat);
    char a[32], b[32], text[64];
    fmt_num(v, 10, true, a, sizeof a);
    fmt_num(r, 8, true, b, sizeof b);
    snprintf(text, sizeof text, "%s %s = %s %s", a, cat->units[c->from].sym, b, cat->units[c->to].sym);
    recent_add(tp->def->id, text, c->from, c->to, form_text(f, c->in));
}

void conv_open(tool_page_t *tp)
{
    conv_t *c = calloc(1, sizeof *c);
    if (!c) return;
    tp->data = c;
    tp->on_close = conv_close;
    c->cat = (units_cat_t)tp->def->arg;
    const unit_cat_t *cat = units_cat(c->cat);
    form_t *f = form_create(tp, compute, KP_NUM);
    if (!f) return;
    form_set_user(f, c);
    c->from = form_int(f, 0, cat->def_from);
    c->to = form_int(f, 1, cat->def_to);
    if (c->from < 0 || c->from >= cat->count) c->from = cat->def_from;
    if (c->to < 0 || c->to >= cat->count) c->to = cat->def_to;
    c->in = form_input(f, "From", "", F_NUM, "1");
    c->out = form_output(f, "To");

    lv_obj_t *x = form_extra(f);
    lv_obj_t *bar = ui_box(x, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(bar, LV_PCT(100));
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, T5_DP(8), 0);
    lv_obj_t *sw = md_button_create(bar, "Swap", MD_BUTTON_OUTLINED);
    lv_obj_add_event_cb(sw, swap_clicked, LV_EVENT_CLICKED, f);
    lv_obj_t *hint = ui_note(bar, "Tap a unit to convert to it, hold to convert from it.");
    lv_obj_set_width(hint, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(hint, 1);

    const md_scheme_t *sc = md_scheme();
    for (int i = 0; i < cat->count && i < CONV_MAX; i++) {
        lv_obj_t *r = ui_box(x, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(r, LV_PCT(100));
        lv_obj_set_style_pad_hor(r, T5_DP(12), 0);
        lv_obj_set_style_pad_ver(r, T5_DP(8), 0);
        lv_obj_set_style_radius(r, T5_DP(8), 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(r, md_color(sc->c.primary), 0);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r, T5_DP(8), 0);
        lv_obj_set_clickable(r, true);
        lv_obj_add_event_cb(r, unit_tapped, LV_EVENT_SHORT_CLICKED, f);
        lv_obj_add_event_cb(r, unit_tapped, LV_EVENT_LONG_PRESSED, f);
        c->names[i] = md_label_create(r, cat->units[i].name, MD_TYPE_BODY2);
        lv_obj_set_style_text_opa(c->names[i], sc->on_surface.medium, 0);
        lv_obj_set_flex_grow(c->names[i], 1);
        lv_label_set_long_mode(c->names[i], LV_LABEL_LONG_MODE_DOTS);
        c->vals[i] = ui_label(r, "", 32, LV_OPA_COVER);
        c->rows[i] = r;
    }
    ui_note(x, c->cat == UC_TIME ? "A month is 30.44 days and a year 365.2425 days, the Gregorian averages."
               : c->cat == UC_SPEED ? "Mach 1 is the speed of sound at sea level, 15 " DEG "C: 340.3 m/s."
               : c->cat == UC_DATA ? "kB, MB, GB are powers of 1000; KiB, MiB, GiB powers of 1024."
               : c->cat == UC_COOKING ? "Volumes. tsp, tbsp, cup and fl oz are US measures unless marked."
               : c->cat == UC_POWER ? "dBm is decibels relative to 1 mW: 0 dBm = 1 mW, 30 dBm = 1 W."
               : "");
    form_ready(f);
}
