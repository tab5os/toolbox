/* Colour: HEX, RGB, HSL and HSV with a swatch, and WCAG contrast against
 * white and black. Type a hex code on the keypad or drag the sliders. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "engine/colour.h"

enum { SL_R, SL_G, SL_B, SL_H, SL_S, SL_L, SL_N };

typedef struct {
    form_t *f;
    int hex, rgb, hsl, hsv, contrast;
    col_rgb_t c;
    col_hsl_t hsl_v;              /* kept, so hue survives grey and black */
    lv_obj_t *swatch, *swatch_text, *sl[SL_N], *val[SL_N];
    bool busy;
} col_t;

static void show(col_t *k, bool set_hex, bool set_sliders)
{
    char s[64];
    form_t *f = k->f;
    k->busy = true;
    if (set_hex) {
        char h[8];
        col_format_hex(k->c, h, sizeof h);
        form_set_text(f, k->hex, h + 1);
        form_set_error(f, k->hex, false);
    }
    lv_obj_set_style_bg_color(k->swatch, lv_color_make(k->c.r, k->c.g, k->c.b), 0);
    double cw = col_contrast(k->c, (col_rgb_t){ 255, 255, 255 }), cb = col_contrast(k->c, (col_rgb_t){ 0, 0, 0 });
    lv_obj_set_style_text_color(k->swatch_text, cw >= cb ? lv_color_white() : lv_color_black(), 0);
    col_format_hex(k->c, s, sizeof s);
    ui_set_text(k->swatch_text, s);
    snprintf(s, sizeof s, "rgb(%d, %d, %d)", k->c.r, k->c.g, k->c.b);
    form_set_out(f, k->rgb, s);
    snprintf(s, sizeof s, "hsl(%.0f, %.0f%%, %.0f%%)", k->hsl_v.h, k->hsl_v.s, k->hsl_v.l);
    form_set_out(f, k->hsl, s);
    col_hsv_t v = col_rgb_to_hsv(k->c);
    snprintf(s, sizeof s, "hsv(%.0f, %.0f%%, %.0f%%)", v.h, v.s, v.v);
    form_set_out(f, k->hsv, s);
    snprintf(s, sizeof s, "%.1f:1 on white %s,  %.1f:1 on black %s", cw, cw >= 4.5 ? "(AA)" : "", cb,
             cb >= 4.5 ? "(AA)" : "");
    form_set_out(f, k->contrast, s);
    int32_t vals[SL_N] = { k->c.r, k->c.g, k->c.b, (int32_t)(k->hsl_v.h + 0.5), (int32_t)(k->hsl_v.s + 0.5),
                           (int32_t)(k->hsl_v.l + 0.5) };
    for (int i = 0; i < SL_N; i++) {
        if (set_sliders && lv_slider_get_value(k->sl[i]) != vals[i]) lv_slider_set_value(k->sl[i], vals[i], LV_ANIM_OFF);
        snprintf(s, sizeof s, "%d", (int)vals[i]);
        ui_set_text(k->val[i], s);
    }
    form_set_int(f, 0, (int32_t)col_to_u32(k->c));
    k->busy = false;
}

static void compute(form_t *f)
{
    col_t *k = form_user(f);
    if (!k || k->busy || !k->swatch) return;
    col_rgb_t c;
    const char *t = form_text(f, k->hex);
    if (col_parse_hex(t, &c)) {
        form_set_error(f, k->hex, false);
        if (col_to_u32(c) != col_to_u32(k->c)) {
            k->c = c;
            k->hsl_v = col_rgb_to_hsl(c);
        }
        show(k, false, true);
    } else {
        form_set_error(f, k->hex, strlen(t) > 0 && strlen(t) != 3);
    }
}

static void slider_changed(lv_event_t *e)
{
    col_t *k = lv_event_get_user_data(e);
    if (k->busy) return;
    lv_obj_t *s = lv_event_get_target_obj(e);
    int i = 0;
    while (i < SL_N && k->sl[i] != s) i++;
    int32_t v = lv_slider_get_value(s);
    if (i < SL_H) {
        uint8_t *ch[3] = { &k->c.r, &k->c.g, &k->c.b };
        *ch[i] = (uint8_t)v;
        col_hsl_t h = col_rgb_to_hsl(k->c);
        if (h.s > 0.5 && h.l > 0.5 && h.l < 99.5) k->hsl_v.h = h.h;   /* keep the hue for greys */
        k->hsl_v.s = h.s;
        k->hsl_v.l = h.l;
    } else {
        double *hs[3] = { &k->hsl_v.h, &k->hsl_v.s, &k->hsl_v.l };
        *hs[i - SL_H] = v;
        k->c = col_hsl_to_rgb(k->hsl_v);
    }
    show(k, true, true);
}

void colour_open(tool_page_t *tp)
{
    col_t *k = calloc(1, sizeof *k);
    if (!k) return;
    tp->data = k;
    form_t *f = form_create(tp, compute, KP_HEX);
    if (!f) return;
    k->f = f;
    form_set_user(f, k);
    uint32_t saved = (uint32_t)form_int(f, 0, 0x1E88E5);
    k->c = col_from_u32(saved & 0xFFFFFF);
    k->hsl_v = col_rgb_to_hsl(k->c);

    lv_obj_t *x = form_extra(f);
    k->swatch = lv_obj_create(x);
    lv_obj_remove_style_all(k->swatch);
    lv_obj_set_size(k->swatch, LV_PCT(100), T5_DP(72));
    lv_obj_set_style_radius(k->swatch, T5_DP(12), 0);
    lv_obj_set_style_bg_opa(k->swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(k->swatch, 1, 0);
    lv_obj_set_style_border_color(k->swatch, md_color(md_scheme()->c.on_surface), 0);
    lv_obj_set_style_border_opa(k->swatch, 0x40, 0);
    k->swatch_text = ui_label(k->swatch, "", 48, LV_OPA_COVER);
    lv_obj_center(k->swatch_text);

    k->hex = form_input(f, "Hex", "", F_HEX, NULL);
    form_set_max_len(f, k->hex, 6);
    form_set_unit(f, k->hex, "RRGGBB or RGB");

    lv_obj_t *y = form_extra(f);
    static const char *const names[SL_N] = { "R", "G", "B", "H", "S", "L" };
    static const int32_t max[SL_N] = { 255, 255, 255, 359, 100, 100 };
    for (int i = 0; i < SL_N; i++) {
        lv_obj_t *row = ui_box(y, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, T5_DP(12), 0);
        lv_obj_t *l = md_label_create(row, names[i], MD_TYPE_SUBTITLE1);
        lv_obj_set_width(l, T5_DP(16));
        k->sl[i] = md_slider_create(row, 0, max[i], 0);
        lv_obj_set_flex_grow(k->sl[i], 1);
        lv_obj_add_event_cb(k->sl[i], slider_changed, LV_EVENT_VALUE_CHANGED, k);
        k->val[i] = md_label_create(row, "0", MD_TYPE_BODY2);
        lv_obj_set_width(k->val[i], T5_DP(32));
        lv_obj_set_style_text_align(k->val[i], LV_TEXT_ALIGN_RIGHT, 0);
        if (i == SL_B) {
            lv_obj_t *gap = lv_obj_create(y);
            lv_obj_remove_style_all(gap);
            lv_obj_set_size(gap, 1, T5_DP(4));
        }
    }
    k->rgb = form_output(f, "RGB");
    k->hsl = form_output(f, "HSL");
    k->hsv = form_output(f, "HSV");
    k->contrast = form_output(f, "Contrast (WCAG)");
    form_set_value_font(f, k->contrast, 28, true);
    k->busy = true;                      /* form_ready computes from the saved hex: show the saved colour */
    form_ready(f);
    k->busy = false;
    show(k, true, true);
}
