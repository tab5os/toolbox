/* Electronics: Ohm's law, resistor colour codes, LED series resistors,
 * voltage dividers and battery life. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "engine/elec.h"

static void plain(double v, char *out, size_t n)
{
    fmt_num(v, 6, false, out, n);
}

/* Keep the `keep` most recently typed of the inputs fld[0..n-1]: clear the
 * older ones, which then show computed values. Returns the known mask. */
static unsigned keep_latest(form_t *f, const int *fld, int n, int keep)
{
    for (;;) {
        int count = 0, oldest = -1;
        uint32_t oldest_seq = UINT32_MAX;
        for (int i = 0; i < n; i++) {
            uint32_t s = form_seq(f, fld[i]);
            if (!s) continue;
            count++;
            if (s < oldest_seq) oldest_seq = s, oldest = i;
        }
        if (count <= keep) break;
        form_set_text(f, fld[oldest], "");
    }
    unsigned known = 0;
    double v;
    for (int i = 0; i < n; i++)
        if (form_get(f, fld[i], &v)) known |= 1u << i;
    return known;
}

/* ---- Ohm's law -------------------------------------------------------------------- */
typedef struct {
    int fld[4], summary;
} ohm_t;

static void ohm_compute(form_t *f)
{
    ohm_t *o = form_user(f);
    unsigned known = keep_latest(f, o->fld, 4, 2);
    double x[4] = { 0 };
    for (int i = 0; i < 4; i++) form_get(f, o->fld[i], &x[i]);
    bool ok = elec_ohm(x, known);
    char s[48], line[128];
    for (int i = 0; i < 4; i++) {
        if (known & (1u << i)) continue;
        if (ok) plain(x[i], s, sizeof s);
        else s[0] = '\0';
        form_set_hint(f, o->fld[i], s);
    }
    if (!ok) {
        form_set_out(f, o->summary, __builtin_popcount(known) < 2 ? "Type any two values" : "No answer");
        return;
    }
    static const char *const units[4] = { "V", "A", OHM, "W" };
    line[0] = '\0';
    for (int i = 0; i < 4; i++) {
        fmt_si(x[i], 4, units[i], s, sizeof s);
        strncat(line, s, sizeof line - strlen(line) - 1);
        if (i < 3) strncat(line, ",  ", sizeof line - strlen(line) - 1);
    }
    form_set_out(f, o->summary, line);
}

void ohm_open(tool_page_t *tp)
{
    ohm_t *o = calloc(1, sizeof *o);
    if (!o) return;
    tp->data = o;
    form_t *f = form_create(tp, ohm_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, o);
    o->fld[OHM_V] = form_input(f, "Voltage", "V", F_NUM, "12");
    o->fld[OHM_I] = form_input(f, "Current", "A", F_NUM, "0.5");
    o->fld[OHM_R] = form_input(f, "Resistance", OHM, F_NUM, NULL);
    o->fld[OHM_P] = form_input(f, "Power", "W", F_NUM, NULL);
    o->summary = form_output(f, "All four");
    form_set_value_font(f, o->summary, 32, true);
    ui_note(form_extra(f), "Type any two: the other two are worked out. Typing a third replaces the oldest. "
                           "V = I R,  P = V I.");
    form_ready(f);
}

/* ---- resistor colour codes ----------------------------------------------------------- */
typedef struct {
    int value, tol, names;
    int nbands, sel;
    int bands[6];
    lv_obj_t *body, *band[6], *palette, *swatch[RC_COUNT], *chips;
    bool from_bands;      /* the bands were tapped last: they decide the value */
} res_t;

static void res_draw(res_t *r)
{
    const md_scheme_t *sc = md_scheme();
    for (int i = 0; i < 6; i++) {
        lv_obj_set_hidden(r->band[i], i >= r->nbands);
        if (i >= r->nbands) continue;
        lv_obj_set_style_bg_color(r->band[i], lv_color_hex(elec_colour_rgb(r->bands[i])), 0);
        lv_obj_set_style_bg_opa(r->band[i], r->bands[i] == RC_NONE ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
        bool s = i == r->sel;
        lv_obj_set_style_outline_width(r->band[i], s ? T5_DP(3) : 0, 0);
        lv_obj_set_style_outline_color(r->band[i], md_color(sc->c.primary), 0);
        lv_obj_set_style_outline_pad(r->band[i], T5_DP(2), 0);
        /* The tolerance band stands apart. */
        int digits = r->nbands == 4 ? 2 : 3;
        lv_obj_set_style_margin_left(r->band[i], i == digits + 1 ? T5_DP(22) : 0, 0);
    }
    for (int c = 0; c < RC_COUNT; c++) {
        bool ok = elec_band_allows(r->nbands, r->sel, c);
        lv_obj_set_hidden(r->swatch[c], !ok);
        lv_obj_set_style_border_width(r->swatch[c], c == r->bands[r->sel] ? T5_DP(3) : 1, 0);
        lv_obj_set_style_border_color(r->swatch[c], md_color(c == r->bands[r->sel] ? sc->c.primary
                                                                                    : sc->c.on_surface), 0);
    }
}

static void res_save_bands(form_t *f, res_t *r)
{
    /* All six bands in one int: 4 bits each, plus the band count. */
    int32_t packed = r->nbands;
    for (int i = 0; i < 6; i++) packed |= (r->bands[i] & 0xF) << (4 + 4 * i);
    form_set_int(f, 0, packed);
}

static void res_compute(form_t *f)
{
    res_t *r = form_user(f);
    char s[96], n[40];
    double ohms;
    /* Typed value -> bands, unless the bands were just tapped. */
    if (!r->from_bands && form_get(f, r->value, &ohms)) {
        int b[6];
        memcpy(b, r->bands, sizeof b);
        if (elec_bands_encode(ohms, r->nbands, b)) {
            int digits = r->nbands == 4 ? 2 : 3;
            for (int i = 0; i <= digits; i++) r->bands[i] = b[i];   /* keep the chosen tolerance */
            form_set_error(f, r->value, false);
            res_save_bands(f, r);
        } else {
            form_set_error(f, r->value, true);
        }
    }
    r->from_bands = false;
    elec_resistor_t res;
    if (elec_bands_decode(r->bands, r->nbands, &res)) {
        fmt_si(res.ohms, 4, OHM, s, sizeof s);
        if (res.tol_pct > 0) {
            fmt_num(res.tol_pct, 3, false, n, sizeof n);
            snprintf(s + strlen(s), sizeof s - strlen(s), "  \xC2\xB1%s %%", n);
        }
        if (res.tempco_ppm) snprintf(s + strlen(s), sizeof s - strlen(s), "  %d ppm/K", res.tempco_ppm);
        form_set_out(f, r->tol, s);
        double lo = res.ohms * (1 - res.tol_pct / 100), hi = res.ohms * (1 + res.tol_pct / 100);
        char a[32], b[32];
        fmt_si(lo, 4, OHM, a, sizeof a);
        fmt_si(hi, 4, OHM, b, sizeof b);
        snprintf(n, sizeof n, "%s to %s", a, b);
        form_set_label(f, r->tol, n);
    } else {
        form_set_out(f, r->tol, "Not a valid code");
        form_set_label(f, r->tol, "Value");
    }
    s[0] = '\0';
    for (int i = 0; i < r->nbands; i++) {
        if (i) strncat(s, ", ", sizeof s - strlen(s) - 1);
        strncat(s, elec_colour_name(r->bands[i]), sizeof s - strlen(s) - 1);
    }
    form_set_out(f, r->names, s);
    res_draw(r);
}

static void res_set_value_from_bands(form_t *f, res_t *r)
{
    elec_resistor_t res;
    char s[32];
    if (elec_bands_decode(r->bands, r->nbands, &res)) {
        fmt_num(res.ohms, 10, false, s, sizeof s);
        form_set_text(f, r->value, s);
        form_set_error(f, r->value, false);
    }
    r->from_bands = true;
    res_save_bands(f, r);
    form_recompute(f);
}

static void band_tapped(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    res_t *r = form_user(f);
    lv_obj_t *t = lv_event_get_current_target_obj(e);
    for (int i = 0; i < r->nbands; i++)
        if (r->band[i] == t) r->sel = i;
    res_draw(r);
}

static void swatch_tapped(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    res_t *r = form_user(f);
    lv_obj_t *t = lv_event_get_current_target_obj(e);
    for (int c = 0; c < RC_COUNT; c++)
        if (r->swatch[c] == t) r->bands[r->sel] = c;
    if (r->sel < r->nbands - 1) r->sel++;              /* on to the next band */
    res_set_value_from_bands(f, r);
}

static void nbands_changed(lv_event_t *e)
{
    lv_obj_t *chip = lv_event_get_target_obj(e);
    form_t *f = lv_event_get_user_data(e);
    res_t *r = form_user(f);
    if (!md_chip_selected(chip)) return;
    int n = 4 + ui_chip_index(chip);
    if (n == r->nbands) return;
    /* Re-encode the same value for the new band count if it fits. */
    elec_resistor_t old;
    bool had = elec_bands_decode(r->bands, r->nbands, &old);
    r->nbands = n;
    int digits = n == 4 ? 2 : 3;
    r->bands[digits + 1] = n == 4 ? RC_GOLD : RC_BROWN;
    if (n == 6) r->bands[5] = RC_BROWN;
    if (!had || !elec_bands_encode(old.ohms, n, r->bands)) {
        int def[6] = { RC_BROWN, RC_BLACK, RC_BLACK, RC_BROWN, RC_BROWN, RC_BROWN };
        if (n == 4) def[2] = RC_RED, def[3] = RC_GOLD;
        memcpy(r->bands, def, sizeof def);
    }
    if (r->sel >= n) r->sel = 0;
    res_set_value_from_bands(f, r);
}

void resistor_open(tool_page_t *tp)
{
    res_t *r = calloc(1, sizeof *r);
    if (!r) return;
    tp->data = r;
    form_t *f = form_create(tp, res_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, r);
    int32_t packed = form_int(f, 0, -1);
    int def[6] = { RC_YELLOW, RC_VIOLET, RC_RED, RC_GOLD, RC_BROWN, RC_BROWN };
    r->nbands = 4;
    memcpy(r->bands, def, sizeof def);
    if (packed >= 0 && (packed & 0xF) >= 4 && (packed & 0xF) <= 6) {
        r->nbands = packed & 0xF;
        for (int i = 0; i < 6; i++) r->bands[i] = (packed >> (4 + 4 * i)) & 0xF;
        for (int i = 0; i < 6; i++)
            if (r->bands[i] >= RC_COUNT) r->bands[i] = RC_BROWN;
    }

    lv_obj_t *x = form_extra(f);
    static const char *const nb[] = { "4 bands", "5 bands", "6 bands" };
    r->chips = ui_chips(x, nb, 3, r->nbands - 4, nbands_changed, f);

    /* The resistor: leads, a beige body, and the bands. */
    lv_obj_t *wrap = ui_box(x, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(wrap, LV_PCT(100));
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *leads[2];
    for (int k = 0; k < 2; k++) {
        leads[k] = lv_obj_create(wrap);
        lv_obj_remove_style_all(leads[k]);
        lv_obj_set_size(leads[k], T5_DP(28), T5_DP(4));
        lv_obj_set_style_bg_opa(leads[k], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(leads[k], lv_color_hex(0xB0BEC5), 0);
        if (k == 0) {
            r->body = ui_box(wrap, LV_FLEX_FLOW_ROW);
            lv_obj_set_size(r->body, LV_SIZE_CONTENT, T5_DP(60));
            lv_obj_set_style_radius(r->body, T5_DP(20), 0);
            lv_obj_set_style_bg_opa(r->body, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(r->body, lv_color_hex(0xD7C49E), 0);
            lv_obj_set_style_pad_hor(r->body, T5_DP(22), 0);
            lv_obj_set_style_pad_column(r->body, T5_DP(12), 0);
            for (int i = 0; i < 6; i++) {
                lv_obj_t *b = lv_obj_create(r->body);
                lv_obj_remove_style_all(b);
                lv_obj_set_size(b, T5_DP(18), LV_PCT(100));
                lv_obj_set_ext_click_area(b, T5_DP(6));
                lv_obj_set_clickable(b, true);
                lv_obj_add_event_cb(b, band_tapped, LV_EVENT_CLICKED, f);
                r->band[i] = b;
            }
        }
    }
    r->palette = ui_box(x, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_width(r->palette, LV_PCT(100));
    lv_obj_set_style_pad_gap(r->palette, T5_DP(6), 0);
    for (int c = 0; c < RC_COUNT; c++) {
        lv_obj_t *s = lv_obj_create(r->palette);
        lv_obj_remove_style_all(s);
        lv_obj_set_size(s, T5_DP(40), T5_DP(40));
        lv_obj_set_style_radius(s, T5_DP(20), 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s, lv_color_hex(elec_colour_rgb(c)), 0);
        lv_obj_set_clickable(s, true);
        lv_obj_add_event_cb(s, swatch_tapped, LV_EVENT_CLICKED, f);
        r->swatch[c] = s;
    }
    ui_note(x, "Tap a band, then its colour. Or type a value to see its bands.");
    r->tol = form_output(f, "Value");
    r->names = form_output(f, "Bands");
    form_set_value_font(f, r->names, 32, true);
    r->value = form_input(f, "Resistance", OHM, F_NUM, NULL);
    form_ready(f);
    if (!form_text(f, r->value)[0]) res_set_value_from_bands(f, r);
}

/* ---- LED resistor ------------------------------------------------------------------ */
typedef struct {
    int vs, vf, ma, n, r_exact, r_std, i_act, p;
} led_t;

static void led_compute(form_t *f)
{
    led_t *l = form_user(f);
    double vs, vf, ma, n = 1;
    elec_led_t r;
    char s[64], a[32];
    form_get(f, l->n, &n);
    bool have = form_get(f, l->vs, &vs) && form_get(f, l->vf, &vf) && form_get(f, l->ma, &ma);
    bool ok = have && n >= 1 && n <= 100 && elec_led(vs, vf, ma / 1000.0, (int)n, &r);
    int outs[4] = { l->r_exact, l->r_std, l->i_act, l->p };
    for (int i = 0; i < 4; i++) form_set_error(f, outs[i], have && !ok);
    if (!ok) {
        form_set_out(f, l->r_exact, have ? "The supply must be above the LEDs' total" : "-");
        for (int i = 1; i < 4; i++) form_set_out(f, outs[i], "-");
        return;
    }
    fmt_si(r.r_exact, 4, OHM, s, sizeof s);
    form_set_out(f, l->r_exact, s);
    fmt_si(r.r_e12, 3, OHM, s, sizeof s);
    form_set_out(f, l->r_std, s);
    fmt_si(r.i_e12, 3, "A", s, sizeof s);
    form_set_out(f, l->i_act, s);
    /* A rating of at least twice the dissipation. */
    static const double ratings[] = { 0.125, 0.25, 0.5, 1, 2, 5 };
    static const char *const rnames[] = { "1/8 W", "1/4 W", "1/2 W", "1 W", "2 W", "5 W" };
    const char *rate = "over 5 W";
    for (int i = 0; i < 6; i++)
        if (r.p_resistor * 2 <= ratings[i]) {
            rate = rnames[i];
            break;
        }
    fmt_si(r.p_resistor, 3, "W", a, sizeof a);
    snprintf(s, sizeof s, "%s: use %s or more", a, rate);
    form_set_out(f, l->p, s);
}

static void led_preset(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    led_t *l = form_user(f);
    lv_obj_t *chip = lv_event_get_target_obj(e);
    static const char *const vf[] = { "2.0", "2.1", "2.2", "3.2", "3.2", "1.5" };
    int i = ui_chip_index(chip);
    md_chip_set_selected(chip, false);
    if (i < 0 || i > 5) return;
    form_set_text(f, l->vf, vf[i]);
    form_recompute(f);
}

void led_open(tool_page_t *tp)
{
    led_t *l = calloc(1, sizeof *l);
    if (!l) return;
    tp->data = l;
    form_t *f = form_create(tp, led_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, l);
    l->vs = form_input(f, "Supply", "V", F_NUM, "5");
    l->vf = form_input(f, "LED forward voltage", "V", F_NUM, "2.0");
    lv_obj_t *x = form_extra(f);
    static const char *const presets[] = { "Red", "Yellow", "Green", "Blue", "White", "IR" };
    lv_obj_t *row = ui_chips(x, presets, 6, -1, NULL, NULL);
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++)
        lv_obj_add_event_cb(lv_obj_get_child(row, (int32_t)i), led_preset, LV_EVENT_CLICKED, f);
    l->ma = form_input(f, "LED current", "mA", F_NUM, "20");
    l->n = form_input(f, "LEDs in series", "", F_UNSIGNED, "1");
    form_set_max_len(f, l->n, 2);
    l->r_std = form_output(f, "Resistor (next E12 value up)");
    l->r_exact = form_output(f, "Exact resistance");
    l->i_act = form_output(f, "Current with the E12 resistor");
    l->p = form_output(f, "Resistor power");
    ui_note(form_extra(f), "Forward voltages are typical: check the LED's datasheet.");
    form_ready(f);
}

/* ---- voltage divider -------------------------------------------------------------- */
typedef struct {
    int fld[4], ratio, current;
} div_t_;

static void div_compute(form_t *f)
{
    div_t_ *d = form_user(f);
    unsigned known = keep_latest(f, d->fld, 4, 3);
    double x[4] = { 0 };
    for (int i = 0; i < 4; i++) form_get(f, d->fld[i], &x[i]);
    bool ok = elec_divider(x, known);
    char s[48];
    for (int i = 0; i < 4; i++) {
        if (known & (1u << i)) continue;
        if (ok) plain(x[i], s, sizeof s);
        else s[0] = '\0';
        form_set_hint(f, d->fld[i], s);
    }
    if (ok && x[DIV_R1] + x[DIV_R2] > 0) {
        fmt_num(x[DIV_R2] / (x[DIV_R1] + x[DIV_R2]), 5, false, s, sizeof s);
        form_set_out(f, d->ratio, s);
        fmt_si(x[DIV_VIN] / (x[DIV_R1] + x[DIV_R2]), 3, "A", s, sizeof s);
        form_set_out(f, d->current, s);
    } else {
        form_set_out(f, d->ratio, __builtin_popcount(known) < 3 ? "Type any three values" : "No answer");
        form_set_out(f, d->current, "-");
    }
}

void divider_open(tool_page_t *tp)
{
    div_t_ *d = calloc(1, sizeof *d);
    if (!d) return;
    tp->data = d;
    form_t *f = form_create(tp, div_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, d);
    d->fld[DIV_VIN] = form_input(f, "Vin", "V", F_NUM, "12");
    d->fld[DIV_R1] = form_input(f, "R1 (top)", OHM, F_NUM, "10000");
    d->fld[DIV_R2] = form_input(f, "R2 (bottom)", OHM, F_NUM, "3300");
    d->fld[DIV_VOUT] = form_input(f, "Vout", "V", F_NUM, NULL);
    d->ratio = form_output(f, "Ratio, Vout / Vin");
    d->current = form_output(f, "Current through the divider");
    ui_note(form_extra(f), "Type any three: the fourth is worked out. Vout = Vin " "\xC3\x97" " R2 / (R1 + R2), "
                           "with no load on the output.");
    form_ready(f);
}

/* ---- battery life ------------------------------------------------------------------ */
typedef struct {
    int mah, ma, usable, hours, wh, v;
} bat_t;

static void bat_compute(form_t *f)
{
    bat_t *b = form_user(f);
    double mah, ma, use = 80, v;
    char s[48], n[32];
    form_get(f, b->usable, &use);
    double h;
    bool ok = form_get(f, b->mah, &mah) && form_get(f, b->ma, &ma) && use > 0 && use <= 100 &&
              elec_battery_hours(mah, ma, use / 100.0, &h);
    if (ok) {
        fmt_duration(h * 3600, s, sizeof s);
        fmt_num(h, 4, true, n, sizeof n);
        snprintf(s + strlen(s), sizeof s - strlen(s), "  (%s h)", n);
    } else snprintf(s, sizeof s, "-");
    form_set_out(f, b->hours, s);
    if (form_get(f, b->v, &v) && form_get(f, b->mah, &mah)) {
        fmt_num(mah * v / 1000.0, 4, true, n, sizeof n);
        snprintf(s, sizeof s, "%s Wh", n);
    } else snprintf(s, sizeof s, "-");
    form_set_out(f, b->wh, s);
}

void battery_open(tool_page_t *tp)
{
    bat_t *b = calloc(1, sizeof *b);
    if (!b) return;
    tp->data = b;
    form_t *f = form_create(tp, bat_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, b);
    b->mah = form_input(f, "Capacity", "mAh", F_NUM, "2000");
    b->ma = form_input(f, "Average load", "mA", F_NUM, "150");
    b->usable = form_input(f, "Usable capacity", "%", F_NUM, "80");
    b->hours = form_output(f, "Runs for about");
    b->v = form_input(f, "Nominal voltage (optional)", "V", F_NUM, "3.7");
    b->wh = form_output(f, "Energy");
    ui_note(form_extra(f), "Usable capacity allows for the cut-off voltage, ageing and converter losses: "
                           "80 % is a fair guess for lithium cells.");
    form_ready(f);
}
