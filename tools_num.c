/* Number and money tools: bases, percentages, ratios, tip and split,
 * discounts, unit prices, random numbers and passwords. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <t5/sys.h>
#include "app.h"
#include "engine/numtools.h"

/* Money: two decimals and group separators, "1,234.50". */
static void money(double v, char *out, size_t n)
{
    if (!isfinite(v)) {
        snprintf(out, n, "-");
        return;
    }
    char tmp[48];
    snprintf(tmp, sizeof tmp, "%.2f", fabs(v) < 0.005 ? 0.0 : v);
    char *dot = strchr(tmp, '.');
    char *start = tmp + (tmp[0] == '-');
    int ilen = (int)(dot - start);
    char res[64];
    size_t o = 0;
    if (tmp[0] == '-') res[o++] = '-';
    for (int i = 0; i < ilen && o < sizeof res - 1; i++) {
        if (i && (ilen - i) % 3 == 0) res[o++] = ',';
        res[o++] = start[i];
    }
    res[o] = '\0';
    snprintf(out, n, "%s%s", res, dot);
}

static void num(double v, char *out, size_t n)
{
    fmt_num(v, 10, true, out, n);
}

static void chip_int_changed(lv_event_t *e);

/* Chips that store their choice in the form's int k and recompute. */
typedef struct {
    form_t *f;
    int k;
} chipctx_t;

static void free_ud(lv_event_t *e)
{
    free(lv_event_get_user_data(e));
}

static lv_obj_t *form_chips(form_t *f, lv_obj_t *parent, int k, const char *const *labels, int n, int def)
{
    chipctx_t *cc = malloc(sizeof *cc);
    int sel = form_int(f, k, def);
    if (sel < 0 || sel >= n) sel = def;
    lv_obj_t *row = ui_chips(parent, labels, n, sel, cc ? chip_int_changed : NULL, cc);
    if (cc) {
        cc->f = f;
        cc->k = k;
        lv_obj_add_event_cb(row, free_ud, LV_EVENT_DELETE, cc);
    }
    return row;
}

static void chip_int_changed(lv_event_t *e)
{
    lv_obj_t *chip = lv_event_get_target_obj(e);
    chipctx_t *cc = lv_event_get_user_data(e);
    if (!md_chip_selected(chip) || !cc) return;
    form_set_int(cc->f, cc->k, ui_chip_index(chip));
    form_recompute(cc->f);
}

/* ---- number bases ---------------------------------------------------------------- */
enum { B_DEC, B_HEX, B_OCT, B_BIN };
static const int s_base_of[4] = { 10, 16, 8, 2 };
static const int s_widths[4] = { 8, 16, 32, 64 };

typedef struct {
    int fld[4], sig;
    lv_obj_t *bits, *chips;
    const char *map[64 + 4 + 1];
    char pos[64][3];
    int map_bits;
    int force_src;          /* -1, or the field a bit tap set */
} base_t;

static int base_width(form_t *f)
{
    int w = form_int(f, 0, 2);
    return s_widths[w >= 0 && w < 4 ? w : 2];
}

static void bits_map(base_t *b, int width)
{
    if (b->map_bits == width) return;
    int per_row = width == 8 ? 8 : 16, k = 0;
    for (int i = 0; i < width; i++) {
        int bit = width - 1 - i;
        snprintf(b->pos[i], sizeof b->pos[i], "%d", bit);
        if (i && i % per_row == 0) b->map[k++] = "\n";
        b->map[k++] = b->pos[i];
    }
    b->map[k] = "";
    lv_buttonmatrix_set_map(b->bits, b->map);
    lv_buttonmatrix_set_button_ctrl_all(b->bits, LV_BUTTONMATRIX_CTRL_CHECKABLE);
    lv_obj_set_height(b->bits, T5_DP(34) * (width / per_row) + T5_DP(4));
    b->map_bits = width;
}

static void base_compute(form_t *f)
{
    base_t *b = form_user(f);
    int width = base_width(f);
    uint64_t mask = nt_mask(width);
    static const int maxlen[4][4] = { { 3, 2, 3, 8 }, { 5, 4, 6, 16 }, { 10, 8, 11, 32 }, { 20, 16, 22, 64 } };
    int wi = width == 8 ? 0 : width == 16 ? 1 : width == 32 ? 2 : 3;
    for (int i = 0; i < 4; i++) form_set_max_len(f, b->fld[i], maxlen[wi][i]);
    bits_map(b, width);

    int src = B_DEC;
    uint32_t best = 0;
    for (int i = 0; i < 4; i++)
        if (form_seq(f, b->fld[i]) > best) best = form_seq(f, b->fld[i]), src = i;
    if (b->force_src >= 0) src = b->force_src;
    b->force_src = -1;
    for (int i = 0; i < 4; i++) form_set_error(f, b->fld[i], false);
    const char *t = form_text(f, b->fld[src]);
    uint64_t v = 0;
    char buf[96];
    if (!t[0]) {
        for (int i = 0; i < 4; i++)
            if (i != src) form_set_text(f, b->fld[i], "");
        form_set_out(f, b->sig, "-");
        lv_buttonmatrix_clear_button_ctrl_all(b->bits, LV_BUTTONMATRIX_CTRL_CHECKED);
        return;
    }
    if (!nt_parse_base(t, s_base_of[src], &v) || v > mask) {
        form_set_error(f, b->fld[src], true);
        snprintf(buf, sizeof buf, "Not a %d-bit %s number", width,
                 src == B_DEC ? "decimal" : src == B_HEX ? "hex" : src == B_OCT ? "octal" : "binary");
        form_set_out(f, b->sig, buf);
        return;
    }
    for (int i = 0; i < 4; i++) {
        if (i == src) continue;
        nt_format_base(v, s_base_of[i], false, buf, sizeof buf);
        form_set_text(f, b->fld[i], buf);
    }
    char s[40];
    snprintf(s, sizeof s, "%lld", (long long)nt_signed(v, width));
    form_set_out(f, b->sig, s);
    int n = width;
    for (int i = 0; i < n; i++) {
        bool on = (v >> (width - 1 - i)) & 1;
        if (on) lv_buttonmatrix_set_button_ctrl(b->bits, (uint32_t)i, LV_BUTTONMATRIX_CTRL_CHECKED);
        else lv_buttonmatrix_clear_button_ctrl(b->bits, (uint32_t)i, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
}

static void bit_tapped(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    base_t *b = form_user(f);
    uint32_t i = lv_buttonmatrix_get_selected_button(b->bits);
    int width = base_width(f);
    if (i == LV_BUTTONMATRIX_BUTTON_NONE || (int)i >= width) return;
    uint64_t v = 0;
    nt_parse_base(form_text(f, b->fld[B_DEC]), 10, &v);   /* empty is 0 */
    v ^= (uint64_t)1 << (width - 1 - (int)i);
    char buf[32];
    nt_format_base(v & nt_mask(width), 10, false, buf, sizeof buf);
    form_set_text(f, b->fld[B_DEC], buf);
    b->force_src = B_DEC;
    form_recompute(f);
}

static void width_changed(lv_event_t *e)
{
    lv_obj_t *chip = lv_event_get_target_obj(e);
    form_t *f = lv_event_get_user_data(e);
    base_t *b = form_user(f);
    if (!md_chip_selected(chip)) return;
    uint64_t v = 0;
    int w = ui_chip_index(chip);
    form_set_int(f, 0, w);
    /* Keep the low bits that fit. */
    if (nt_parse_base(form_text(f, b->fld[B_DEC]), 10, &v)) {
        char buf[32];
        nt_format_base(v & nt_mask(s_widths[w]), 10, false, buf, sizeof buf);
        form_set_text(f, b->fld[B_DEC], buf);
    }
    b->force_src = B_DEC;
    form_recompute(f);
}

void base_open(tool_page_t *tp)
{
    base_t *b = calloc(1, sizeof *b);
    if (!b) return;
    tp->data = b;
    b->force_src = -1;
    form_t *f = form_create(tp, base_compute, KP_HEX);
    if (!f) return;
    form_set_user(f, b);
    b->fld[B_DEC] = form_input(f, "Decimal", "", F_UNSIGNED, "255");
    b->fld[B_HEX] = form_input(f, "Hexadecimal", "", F_HEX, NULL);
    form_set_nosave(f, b->fld[B_HEX]);
    b->fld[B_OCT] = form_input(f, "Octal", "", F_UNSIGNED, NULL);
    form_set_nosave(f, b->fld[B_OCT]);
    b->fld[B_BIN] = form_input(f, "Binary", "", F_UNSIGNED, NULL);
    form_set_nosave(f, b->fld[B_BIN]);
    form_set_value_font(f, b->fld[B_BIN], 32, true);
    b->sig = form_output(f, "Signed (two's complement)");

    lv_obj_t *x = form_extra(f);
    static const char *const widths[] = { "8-bit", "16-bit", "32-bit", "64-bit" };
    int w = form_int(f, 0, 2);
    b->chips = ui_chips(x, widths, 4, w >= 0 && w < 4 ? w : 2, width_changed, f);
    ui_note(x, "Bits: tap one to flip it.");
    b->bits = lv_buttonmatrix_create(x);
    lv_obj_set_width(b->bits, LV_PCT(100));
    const md_scheme_t *sc = md_scheme();
    lv_obj_set_style_bg_opa(b->bits, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(b->bits, 0, 0);
    lv_obj_set_style_pad_all(b->bits, 0, 0);
    lv_obj_set_style_pad_gap(b->bits, T5_DP(2), 0);
    lv_obj_set_style_radius(b->bits, T5_DP(4), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(b->bits, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(b->bits, md_color(md_mix(sc->c.on_surface, sc->c.background, 0x18)), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(b->bits, md_color(sc->c.primary), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(b->bits, md_color(sc->c.on_surface), LV_PART_ITEMS);
    lv_obj_set_style_text_color(b->bits, md_color(sc->c.on_primary), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_font(b->bits, md_font(MD_TYPE_CAPTION), LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(b->bits, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(b->bits, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(b->bits, bit_tapped, LV_EVENT_VALUE_CHANGED, f);
    form_ready(f);
}

/* ---- percentage ------------------------------------------------------------------ */
typedef struct {
    int a, b, out, out2;
} pct_t;

static void pct_compute(form_t *f)
{
    pct_t *p = form_user(f);
    int mode = form_int(f, 0, 0);
    static const char *const la[4] = { "Percent", "Part", "From", "Value" };
    static const char *const lb[4] = { "Of", "Whole", "To", "Plus or minus" };
    static const char *const ua[4] = { "%", "", "", "" };
    static const char *const ub[4] = { "", "", "", "%" };
    static const char *const lo[4] = { "Result", "Percentage", "Change", "Result" };
    static const char *const lo2[4] = { "Left over", "Remainder", "Difference", "Amount added" };
    if (mode < 0 || mode > 3) mode = 0;
    form_set_label(f, p->a, la[mode]);
    form_set_label(f, p->b, lb[mode]);
    form_set_unit(f, p->a, ua[mode]);
    form_set_unit(f, p->b, ub[mode]);
    form_set_label(f, p->out, lo[mode]);
    form_set_label(f, p->out2, lo2[mode]);
    double a, b, r = NAN, r2 = NAN;
    char s1[48] = "-", s2[48] = "-", n[40];
    bool ok = form_get(f, p->a, &a) && form_get(f, p->b, &b);
    if (ok) switch (mode) {
        case 0:
            if (nt_pct_of(a, b, &r)) num(r, s1, sizeof s1), num(b - r, s2, sizeof s2);
            break;
        case 1:
            if (nt_pct_ratio(a, b, &r)) {
                num(r, n, sizeof n);
                snprintf(s1, sizeof s1, "%s %%", n);
                num(b - a, s2, sizeof s2);
            } else snprintf(s1, sizeof s1, "No answer");
            break;
        case 2:
            if (nt_pct_change(a, b, &r)) {
                num(r, n, sizeof n);
                snprintf(s1, sizeof s1, "%s%s %%", r > 0 ? "+" : "", n);
            } else snprintf(s1, sizeof s1, "No answer");
            r2 = b - a;
            num(r2, n, sizeof n);
            snprintf(s2, sizeof s2, "%s%s", r2 > 0 ? "+" : "", n);
            break;
        default:
            if (nt_pct_add(a, b, &r)) num(r, s1, sizeof s1), num(r - a, s2, sizeof s2);
            break;
        }
    form_set_out(f, p->out, s1);
    form_set_out(f, p->out2, s2);
}

void pct_open(tool_page_t *tp)
{
    pct_t *p = calloc(1, sizeof *p);
    if (!p) return;
    tp->data = p;
    form_t *f = form_create(tp, pct_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, p);
    static const char *const modes[] = { "x% of y", "x is ?% of y", "% change", "Add %" };
    lv_obj_t *x = form_extra(f);
    form_chips(f, x, 0, modes, 4, 0);
    p->a = form_input(f, "Percent", "%", F_NUM, "15");
    p->b = form_input(f, "Of", "", F_NUM, "80");
    p->out = form_output(f, "Result");
    p->out2 = form_output(f, "Left over");
    form_ready(f);
}

/* ---- ratio ---------------------------------------------------------------------- */
typedef struct {
    int a, b, na, nb, simp, dec;
} ratio_t;

static void ratio_compute(form_t *f)
{
    ratio_t *r = form_user(f);
    double a, b, na, nb, x, y;
    char s[64], n1[32], n2[32];
    bool ab = form_get(f, r->a, &a) && form_get(f, r->b, &b);
    if (ab && nt_ratio_simplify(a, b, &x, &y)) {
        fmt_num(x, 15, false, n1, sizeof n1);
        fmt_num(y, 15, false, n2, sizeof n2);
        snprintf(s, sizeof s, "%s : %s", n1, n2);
        form_set_out(f, r->simp, s);
    } else form_set_out(f, r->simp, "-");
    if (ab && b != 0) {
        num(a / b, n1, sizeof n1);
        snprintf(s, sizeof s, "%s : 1", n1);
        form_set_out(f, r->dec, s);
    } else form_set_out(f, r->dec, "-");
    /* Whichever new side was typed last decides the other. */
    bool use_a = form_seq(f, r->na) >= form_seq(f, r->nb);
    int src = use_a ? r->na : r->nb, dst = use_a ? r->nb : r->na;
    if (form_text(f, dst)[0] && form_seq(f, src)) form_set_text(f, dst, "");
    form_set_hint(f, r->na, "");
    form_set_hint(f, r->nb, "");
    double v;
    if (ab && use_a && form_get(f, r->na, &na) && nt_ratio_scale(a, b, na, &v)) {
        num(v, n1, sizeof n1);
        form_set_hint(f, r->nb, n1);
    } else if (ab && !use_a && form_get(f, r->nb, &nb) && nt_ratio_scale(b, a, nb, &v)) {
        num(v, n1, sizeof n1);
        form_set_hint(f, r->na, n1);
    }
}

void ratio_open(tool_page_t *tp)
{
    ratio_t *r = calloc(1, sizeof *r);
    if (!r) return;
    tp->data = r;
    form_t *f = form_create(tp, ratio_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, r);
    r->a = form_input(f, "A (width)", "", F_NUM, "1920");
    r->b = form_input(f, "B (height)", "", F_NUM, "1080");
    r->simp = form_output(f, "Simplest form");
    r->dec = form_output(f, "As a decimal");
    r->na = form_input(f, "New A", "", F_NUM, "1280");
    r->nb = form_input(f, "New B", "", F_NUM, NULL);
    ui_note(form_extra(f), "Type a new A or a new B to scale the ratio: the other is worked out.");
    form_ready(f);
}

/* ---- tip and split -------------------------------------------------------------- */
typedef struct {
    int bill, pct, people, tip, total, each, tip_each;
    lv_obj_t *round_sw;
} tip_t;

static void tip_compute(form_t *f)
{
    tip_t *t = form_user(f);
    double bill, pct, people;
    nt_tip_t r;
    char s[48];
    bool ok = form_get(f, t->bill, &bill) && form_get(f, t->pct, &pct) && form_get(f, t->people, &people) &&
              people >= 1 && people <= 1000 &&
              nt_tip(bill, pct, (int)people, form_int(f, 0, 0) == 1, &r);
    int outs[4] = { t->tip, t->total, t->each, t->tip_each };
    double vals[4] = { r.tip, r.total, r.per_person, r.tip_per_person };
    for (int i = 0; i < 4; i++) {
        if (ok) money(vals[i], s, sizeof s);
        else snprintf(s, sizeof s, "-");
        form_set_out(f, outs[i], s);
    }
    form_set_hidden(f, t->tip_each, !ok || (int)people == 1);
}

static void tip_preset(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    tip_t *t = form_user(f);
    lv_obj_t *chip = lv_event_get_target_obj(e);
    static const char *const v[] = { "10", "15", "18", "20", "25" };
    int i = ui_chip_index(chip);
    if (i < 0 || i > 4) return;
    md_chip_set_selected(chip, false);
    form_set_text(f, t->pct, v[i]);
    form_recompute(f);
}

static void tip_round(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    form_set_int(f, 0, lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED) ? 1 : 0);
    form_recompute(f);
}

void tip_open(tool_page_t *tp)
{
    tip_t *t = calloc(1, sizeof *t);
    if (!t) return;
    tp->data = t;
    form_t *f = form_create(tp, tip_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, t);
    t->bill = form_input(f, "Bill", "", F_NUM, "64.50");
    t->pct = form_input(f, "Tip", "%", F_NUM, "15");
    lv_obj_t *x = form_extra(f);
    static const char *const presets[] = { "10%", "15%", "18%", "20%", "25%" };
    lv_obj_t *row = ui_chips(x, presets, 5, -1, NULL, NULL);
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); i++)
        lv_obj_add_event_cb(lv_obj_get_child(row, (int32_t)i), tip_preset, LV_EVENT_CLICKED, f);
    t->people = form_input(f, "People", "", F_UNSIGNED, "2");
    form_set_max_len(f, t->people, 4);
    lv_obj_t *sw_row = ui_box(form_extra(f), LV_FLEX_FLOW_ROW);
    lv_obj_set_width(sw_row, LV_PCT(100));
    lv_obj_set_flex_align(sw_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *l = md_label_create(sw_row, "Round each share up", MD_TYPE_BODY1);
    lv_obj_set_flex_grow(l, 1);
    t->round_sw = md_switch_create(sw_row, form_int(f, 0, 0) == 1);
    lv_obj_add_event_cb(t->round_sw, tip_round, LV_EVENT_VALUE_CHANGED, f);
    t->tip = form_output(f, "Tip");
    t->total = form_output(f, "Total");
    t->each = form_output(f, "Each person pays");
    t->tip_each = form_output(f, "Tip each");
    form_ready(f);
}

/* ---- discount ------------------------------------------------------------------- */
typedef struct {
    int price, pct, extra, tax, saved, final, with_tax;
} disc_t;

static void disc_compute(form_t *f)
{
    disc_t *d = form_user(f);
    double price, pct = 0, extra = 0, tax = 0;
    nt_discount_t r;
    char s[48];
    form_get(f, d->pct, &pct);                   /* empty counts as 0 */
    form_get(f, d->extra, &extra);
    form_get(f, d->tax, &tax);
    bool ok = form_get(f, d->price, &price) && nt_discount(price, pct, extra, tax, &r);
    money(ok ? r.final : NAN, s, sizeof s);
    form_set_out(f, d->final, s);
    money(ok ? r.saved : NAN, s, sizeof s);
    if (ok && price > 0) {
        char t[32];
        fmt_num(r.saved / price * 100, 4, false, t, sizeof t);
        snprintf(s + strlen(s), sizeof s - strlen(s), "  (%s %%)", t);
    }
    form_set_out(f, d->saved, s);
    money(ok ? r.with_tax : NAN, s, sizeof s);
    form_set_out(f, d->with_tax, s);
    form_set_hidden(f, d->with_tax, !(tax != 0));
}

void discount_open(tool_page_t *tp)
{
    disc_t *d = calloc(1, sizeof *d);
    if (!d) return;
    tp->data = d;
    form_t *f = form_create(tp, disc_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, d);
    d->price = form_input(f, "Price", "", F_NUM, "80");
    d->pct = form_input(f, "Discount", "%", F_NUM, "25");
    d->final = form_output(f, "You pay");
    d->saved = form_output(f, "You save");
    d->extra = form_input(f, "Extra discount (optional)", "%", F_NUM, NULL);
    d->tax = form_input(f, "Sales tax (optional)", "%", F_NUM, NULL);
    d->with_tax = form_output(f, "With tax");
    form_ready(f);
}

/* ---- unit price ----------------------------------------------------------------- */
typedef struct {
    int pa, qa, pb, qb, ua, ub, verdict;
} up_t;

static void up_compute(form_t *f)
{
    up_t *u = form_user(f);
    double pa, qa, pb, qb;
    nt_unitprice_t r;
    char s[64], n[32];
    bool ok = form_get(f, u->pa, &pa) && form_get(f, u->qa, &qa) && form_get(f, u->pb, &pb) &&
              form_get(f, u->qb, &qb) && nt_unit_price(pa, qa, pb, qb, &r);
    for (int i = 0; i < 2; i++) {
        if (ok) {
            fmt_num(r.unit[i], 4, true, n, sizeof n);
            snprintf(s, sizeof s, "%s per unit", n);
        } else snprintf(s, sizeof s, "-");
        form_set_out(f, i ? u->ub : u->ua, s);
    }
    if (!ok) snprintf(s, sizeof s, "Enter both prices and amounts");
    else if (r.best < 0) snprintf(s, sizeof s, "The same value");
    else {
        fmt_num(r.saving_pct, 3, false, n, sizeof n);
        snprintf(s, sizeof s, "%s is cheaper by %s %%", r.best == 0 ? "A" : "B", n);
    }
    form_set_out(f, u->verdict, s);
}

void unitprice_open(tool_page_t *tp)
{
    up_t *u = calloc(1, sizeof *u);
    if (!u) return;
    tp->data = u;
    form_t *f = form_create(tp, up_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, u);
    u->verdict = form_output(f, "Better buy");
    u->pa = form_input(f, "A: price", "", F_NUM, "3.20");
    u->qa = form_input(f, "A: amount (g, mL, items...)", "", F_NUM, "500");
    u->ua = form_output(f, "A");
    u->pb = form_input(f, "B: price", "", F_NUM, "5.50");
    u->qb = form_input(f, "B: amount, in the same unit", "", F_NUM, "1000");
    u->ub = form_output(f, "B");
    form_ready(f);
}

/* ---- random ---------------------------------------------------------------------- */
static nt_rng_t s_rng;
static bool s_rng_seeded;

/* Fresh entropy each time: the clock, touch timing and accelerometer noise. */
static void rng_stir(void)
{
    if (!s_rng_seeded) {
        nt_rng_seed(&s_rng, ui_entropy() ^ (uint64_t)t5_time_us() ^ (uint64_t)time(NULL) << 32);
        s_rng_seeded = true;
    }
    t5_imu_accel_t a;
    uint64_t noise = 0;
    if (t5_imu_status() == T5_IMU_READY && t5_imu_read_accel(&a) == T5_OK)
        noise = ((uint64_t)(uint16_t)a.x << 32) | ((uint64_t)(uint16_t)a.y << 16) | (uint16_t)a.z;
    nt_rng_mix(&s_rng, ui_entropy() ^ noise);
}

typedef struct {
    int a, b, res, hist;
    lv_obj_t *roll, *flip;
    char history[160];
    int heads, tails;
} rnd_t;

static void rnd_compute(form_t *f)
{
    rnd_t *r = form_user(f);
    int mode = form_int(f, 0, 0);
    form_set_hidden(f, r->a, mode == 2);
    form_set_hidden(f, r->b, mode == 2);
    form_set_label(f, r->a, mode == 1 ? "Dice" : "From");
    form_set_label(f, r->b, mode == 1 ? "Sides" : "To");
    lv_obj_set_hidden(r->roll, mode == 2);
    lv_obj_set_hidden(r->flip, mode != 2);
}

static void rnd_go(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    rnd_t *r = form_user(f);
    int mode = form_int(f, 0, 0);
    char s[160], item[48];
    rng_stir();
    if (mode == 2) {
        bool heads = nt_rng_range(&s_rng, 0, 1) == 1;
        heads ? r->heads++ : r->tails++;
        form_set_out(f, r->res, heads ? "Heads" : "Tails");
        snprintf(s, sizeof s, "Heads %d, tails %d", r->heads, r->tails);
        form_set_out(f, r->hist, s);
        return;
    }
    double a, b;
    if (mode == 1) {
        double n = 1, sides = 6;
        form_get(f, r->a, &n);
        form_get(f, r->b, &sides);
        if (n < 1 || n > 20 || sides < 2 || sides > 1000) {
            form_set_out(f, r->res, "1 to 20 dice, 2 to 1000 sides");
            return;
        }
        long long sum = 0;
        s[0] = '\0';
        for (int i = 0; i < (int)n; i++) {
            int64_t v = nt_rng_range(&s_rng, 1, (int64_t)sides);
            sum += v;
            snprintf(item, sizeof item, "%s%lld", i ? " + " : "", (long long)v);
            strncat(s, item, sizeof s - strlen(s) - 1);
        }
        if ((int)n > 1) {
            snprintf(item, sizeof item, " = %lld", sum);
            strncat(s, item, sizeof s - strlen(s) - 1);
        }
        form_set_out(f, r->res, s);
    } else {
        if (!form_get(f, r->a, &a) || !form_get(f, r->b, &b) || fabs(a) > 9e15 || fabs(b) > 9e15) {
            form_set_out(f, r->res, "Enter a range");
            return;
        }
        int64_t v = nt_rng_range(&s_rng, (int64_t)ceil(a < b ? a : b), (int64_t)floor(a < b ? b : a));
        snprintf(s, sizeof s, "%lld", (long long)v);
        form_set_out(f, r->res, s);
    }
    /* The last few results, newest first. */
    snprintf(item, sizeof item, "%s", s);
    char old[160];
    snprintf(old, sizeof old, "%s", r->history);
    snprintf(r->history, sizeof r->history, "%.40s%s%s", item, old[0] ? ",  " : "", old);
    form_set_out(f, r->hist, r->history);
}

void random_open(tool_page_t *tp)
{
    rnd_t *r = calloc(1, sizeof *r);
    if (!r) return;
    tp->data = r;
    form_t *f = form_create(tp, rnd_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, r);
    lv_obj_t *x = form_extra(f);
    static const char *const modes[] = { "Number", "Dice", "Coin" };
    form_chips(f, x, 0, modes, 3, 0);
    r->a = form_input(f, "From", "", F_INT, "1");
    r->b = form_input(f, "To", "", F_INT, "100");
    lv_obj_t *y = form_extra(f);
    r->roll = md_button_create(y, "Roll", MD_BUTTON_CONTAINED);
    r->flip = md_button_create(y, "Flip", MD_BUTTON_CONTAINED);
    lv_obj_t *bs[2] = { r->roll, r->flip };
    for (int i = 0; i < 2; i++) {
        lv_obj_set_width(bs[i], LV_PCT(100));
        lv_obj_add_event_cb(bs[i], rnd_go, LV_EVENT_CLICKED, f);
    }
    r->res = form_output(f, "Result");
    form_set_value_font(f, r->res, 68, true);
    r->hist = form_output(f, "Earlier");
    form_set_value_font(f, r->hist, 28, true);
    form_ready(f);
}

/* ---- password ------------------------------------------------------------------- */
typedef struct {
    int len, out, strength;
    lv_obj_t *sw[5];
} pw_t;

static const char *const s_pw_names[5] = { "Lowercase a-z", "Uppercase A-Z", "Digits 0-9", "Symbols !#$...",
                                           "Avoid look-alikes (0 O 1 l I)" };

static unsigned pw_sets(form_t *f)
{
    return (unsigned)form_int(f, 0, (int32_t)(NT_PW_LOWER | NT_PW_UPPER | NT_PW_DIGITS | NT_PW_SYMBOLS));
}

static void pw_generate(form_t *f)
{
    pw_t *p = form_user(f);
    double len = 16;
    char out[NT_PW_MAX + 1], s[64];
    form_get(f, p->len, &len);
    if (len < 4) len = 4;
    if (len > NT_PW_MAX) len = NT_PW_MAX;
    rng_stir();
    double bits = nt_password(&s_rng, (int)len, pw_sets(f), out, sizeof out);
    if (bits <= 0) {
        form_set_out(f, p->out, "Choose at least one set");
        form_set_out(f, p->strength, "-");
        return;
    }
    form_set_out(f, p->out, out);
    snprintf(s, sizeof s, "%d bits: %s", (int)bits,
             bits < 40 ? "weak" : bits < 64 ? "fair" : bits < 100 ? "strong" : "very strong");
    form_set_out(f, p->strength, s);
}

static void pw_compute(form_t *f) { (void)f; }

static void pw_go(lv_event_t *e) { pw_generate(lv_event_get_user_data(e)); }

static void pw_switch(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    pw_t *p = form_user(f);
    unsigned sets = 0;
    for (int i = 0; i < 5; i++)
        if (lv_obj_has_state(p->sw[i], LV_STATE_CHECKED)) sets |= 1u << i;
    form_set_int(f, 0, (int32_t)sets);
    pw_generate(f);
}

void password_open(tool_page_t *tp)
{
    pw_t *p = calloc(1, sizeof *p);
    if (!p) return;
    tp->data = p;
    form_t *f = form_create(tp, pw_compute, KP_NUM);
    if (!f) return;
    form_set_user(f, p);
    p->out = form_output(f, "Password");
    form_set_value_font(f, p->out, 48, true);
    p->strength = form_output(f, "Strength");
    lv_obj_t *x = form_extra(f);
    lv_obj_t *go = md_button_create(x, "New password", MD_BUTTON_CONTAINED);
    lv_obj_set_width(go, LV_PCT(100));
    lv_obj_add_event_cb(go, pw_go, LV_EVENT_CLICKED, f);
    p->len = form_input(f, "Length", "characters", F_UNSIGNED, "16");
    form_set_max_len(f, p->len, 2);
    lv_obj_t *y = form_extra(f);
    unsigned sets = pw_sets(f);
    for (int i = 0; i < 5; i++) {
        lv_obj_t *row = ui_box(y, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *l = md_label_create(row, s_pw_names[i], MD_TYPE_BODY1);
        lv_obj_set_flex_grow(l, 1);
        p->sw[i] = md_switch_create(row, (sets >> i) & 1);
        lv_obj_add_event_cb(p->sw[i], pw_switch, LV_EVENT_VALUE_CHANGED, f);
    }
    ui_note(y, "Made on this device from a xoshiro256** generator, stirred with touch timing, the clock and "
               "accelerometer noise. Nothing is stored or sent.");
    form_ready(f);
    pw_generate(f);
}
