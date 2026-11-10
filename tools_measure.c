/* Measuring with the Tab5 itself: an on-screen ruler, a spirit level and a
 * protractor from the accelerometer, and a sound level meter from the
 * microphone (asked for when it opens; the rest of the app works without
 * it). The maths is in engine/measure.c. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <t5/audio.h>
#include "app.h"
#include "engine/measure.h"

static int rotation_deg(void)
{
    return (int)t5_ui_rotation() * 90;
}

static void draw_line(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t w, lv_color_t c,
                      lv_opa_t opa)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1 = (lv_point_precise_t){ x1, y1 };
    d.p2 = (lv_point_precise_t){ x2, y2 };
    d.width = w;
    d.color = c;
    d.opa = opa;
    d.round_start = d.round_end = 1;
    lv_draw_line(layer, &d);
}

static void draw_text(lv_layer_t *layer, const char *text, int32_t cx, int32_t cy, const lv_font_t *font,
                      lv_color_t c)
{
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = text;
    d.text_local = 1;                     /* copied: the buffer is on the stack */
    d.font = font;
    d.color = c;
    d.align = LV_TEXT_ALIGN_CENTER;
    lv_point_t sz;
    lv_text_get_size(&sz, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_area_t a = { cx - sz.x / 2 - 2, cy - sz.y / 2, cx + sz.x / 2 + 2, cy + sz.y / 2 };
    lv_draw_label(layer, &d, &a);
}

static void draw_circle(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t r, int32_t w, lv_color_t c, lv_opa_t opa,
                        bool fill)
{
    if (fill) {
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.radius = LV_RADIUS_CIRCLE;
        d.bg_color = c;
        d.bg_opa = opa;
        lv_area_t a = { cx - r, cy - r, cx + r, cy + r };
        lv_draw_rect(layer, &d, &a);
        return;
    }
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center = (lv_point_t){ cx, cy };
    d.radius = (uint16_t)r;
    d.start_angle = 0;
    d.end_angle = 360;
    d.width = w;
    d.color = c;
    d.opa = opa;
    lv_draw_arc(layer, &d);
}

/* A plain full-size object that draws itself. */
static lv_obj_t *canvas_obj(lv_obj_t *parent, lv_event_cb_t draw, void *ud)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollable(o, false);
    lv_obj_add_event_cb(o, draw, LV_EVENT_DRAW_MAIN_END, ud);
    return o;
}

/* ---- ruler ------------------------------------------------------------------------ */
/* Full screen, so the scale starts at the panel's edge: millimetres along
 * one long edge, inches along the other. */
typedef struct {
    lv_obj_t *ruler, *panel, *scale_l, *card_btn_l, *adj, *hint;
    bool card;
} ruler_t;

static float ruler_ppm(void)
{
    float p = g_set.ruler_px_mm;
    if (!(p >= MEAS_PX_PER_MM_MIN && p <= MEAS_PX_PER_MM_MAX)) p = (float)meas_default_px_per_mm();
    return p;
}

static void ruler_draw(lv_event_t *e)
{
    ruler_t *r = lv_event_get_user_data(e);
    lv_obj_t *o = lv_event_get_current_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const md_scheme_t *sc = md_scheme();
    lv_color_t ink = md_color(sc->c.on_surface), acc = md_color(sc->c.secondary);
    const lv_font_t *font = md_font(MD_TYPE_BODY2);
    float ppm = ruler_ppm();
    int32_t w = lv_area_get_width(&a), h = lv_area_get_height(&a);
    bool horiz = w >= h;
    int32_t len = horiz ? w : h;
    char num[16];
    /* Millimetres from the start of the top (or left) edge. */
    for (int mm = 0; mm * ppm < len; mm++) {
        int32_t p = (int32_t)lroundf(mm * ppm);
        float t = mm % 10 == 0 ? 7.0f : mm % 5 == 0 ? 5.0f : 3.0f;
        int32_t tl = (int32_t)lroundf(t * ppm);
        if (horiz) draw_line(layer, a.x1 + p, a.y1, a.x1 + p, a.y1 + tl, mm % 10 ? 2 : 3, ink, LV_OPA_COVER);
        else draw_line(layer, a.x1, a.y1 + p, a.x1 + tl, a.y1 + p, mm % 10 ? 2 : 3, ink, LV_OPA_COVER);
        if (mm % 10 == 0 && mm) {
            snprintf(num, sizeof num, "%d", mm / 10);
            int32_t off = (int32_t)lroundf(10.5f * ppm);
            if (horiz) draw_text(layer, num, a.x1 + p, a.y1 + off, font, ink);
            else draw_text(layer, num, a.x1 + off, a.y1 + p, font, ink);
        }
    }
    if (horiz) draw_text(layer, "cm", a.x1 + (int32_t)lroundf(4 * ppm), a.y1 + (int32_t)lroundf(10.5f * ppm), font, acc);
    else draw_text(layer, "cm", a.x1 + (int32_t)lroundf(10.5f * ppm), a.y1 + (int32_t)lroundf(4 * ppm), font, acc);
    /* Sixteenths of an inch along the opposite edge. */
    float ppi = ppm * 25.4f;
    for (int s = 0; s * ppi / 16 < len; s++) {
        int32_t p = (int32_t)lroundf(s * ppi / 16);
        float t = s % 16 == 0 ? 7.0f : s % 8 == 0 ? 5.5f : s % 4 == 0 ? 4.0f : s % 2 == 0 ? 3.0f : 2.0f;
        int32_t tl = (int32_t)lroundf(t * ppm);
        int wd = s % 16 ? 2 : 3;
        if (horiz) draw_line(layer, a.x1 + p, a.y2, a.x1 + p, a.y2 - tl, wd, ink, LV_OPA_COVER);
        else draw_line(layer, a.x2, a.y1 + p, a.x2 - tl, a.y1 + p, wd, ink, LV_OPA_COVER);
        if (s % 16 == 0 && s) {
            snprintf(num, sizeof num, "%d", s / 16);
            int32_t off = (int32_t)lroundf(10.5f * ppm);
            if (horiz) draw_text(layer, num, a.x1 + p, a.y2 - off, font, ink);
            else draw_text(layer, num, a.x2 - off, a.y1 + p, font, ink);
        }
    }
    if (horiz) draw_text(layer, "in", a.x1 + (int32_t)lroundf(4 * ppm), a.y2 - (int32_t)lroundf(10.5f * ppm), font, acc);
    else draw_text(layer, "in", a.x2 - (int32_t)lroundf(10.5f * ppm), a.y1 + (int32_t)lroundf(4 * ppm), font, acc);
    if (r->card) {
        /* A bank card's outline from the start corner, to calibrate against. */
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_opa = LV_OPA_TRANSP;
        d.border_color = acc;
        d.border_width = 3;
        d.border_opa = LV_OPA_COVER;
        d.radius = (int32_t)lroundf(3.18f * ppm);
        int32_t cw = (int32_t)lroundf((float)MEAS_CARD_W_MM * ppm), ch = (int32_t)lroundf((float)MEAS_CARD_H_MM * ppm);
        lv_area_t c;
        if (horiz) c = (lv_area_t){ a.x1, a.y1 + (h - ch) / 2, a.x1 + cw - 1, a.y1 + (h - ch) / 2 + ch - 1 };
        else c = (lv_area_t){ a.x1 + (w - ch) / 2, a.y1, a.x1 + (w - ch) / 2 + ch - 1, a.y1 + cw - 1 };
        lv_draw_rect(layer, &d, &c);
    }
}

static void ruler_update(ruler_t *r)
{
    char s[64];
    float ppm = ruler_ppm();
    snprintf(s, sizeof s, "%.3f px/mm  (%.1f ppi)%s", (double)ppm, (double)(ppm * 25.4f),
             g_set.ruler_px_mm == 0 ? "" : ", calibrated");
    ui_set_text(r->scale_l, s);
    ui_set_text(r->card_btn_l, r->card ? "Hide card" : "Calibrate");
    lv_obj_invalidate(r->ruler);
}

static void ruler_adjust(lv_event_t *e)
{
    ruler_t *r = lv_obj_get_user_data(lv_event_get_current_target_obj(e));
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    if (step == 0) g_set.ruler_px_mm = 0;
    else {
        float p = ruler_ppm() * (1.0f + step * 0.0005f);    /* 0.05 % a tap: 0.05 mm over 10 cm */
        if (p < MEAS_PX_PER_MM_MIN) p = MEAS_PX_PER_MM_MIN;
        if (p > MEAS_PX_PER_MM_MAX) p = MEAS_PX_PER_MM_MAX;
        g_set.ruler_px_mm = p;
    }
    settings_changed();
    ruler_update(r);
}

static void ruler_card(lv_event_t *e)
{
    ruler_t *r = lv_event_get_user_data(e);
    r->card = !r->card;
    lv_obj_set_hidden(r->adj, !r->card);
    lv_obj_set_hidden(r->hint, !r->card);
    ruler_update(r);
}

static void ruler_back(lv_event_t *e)
{
    (void)e;
    tool_close();
}

static void ruler_rotate(tool_page_t *tp)
{
    ruler_t *r = tp->data;
    lv_obj_invalidate(r->ruler);
}

void ruler_open(tool_page_t *tp)
{
    ruler_t *r = calloc(1, sizeof *r);
    if (!r) return;
    tp->data = r;
    tp->on_rotate = ruler_rotate;
    ui_set_fullscreen(true);
    lv_obj_set_hidden(tp->bar, true);
    lv_obj_set_style_pad_all(tp->body, 0, 0);
    lv_obj_set_scrollable(tp->body, false);
    r->ruler = canvas_obj(tp->body, ruler_draw, r);
    lv_obj_set_flex_grow(r->ruler, 1);

    /* Controls in the middle, clear of both scales. */
    r->panel = ui_box(r->ruler, LV_FLEX_FLOW_COLUMN);
    lv_obj_center(r->panel);
    lv_obj_set_flex_align(r->panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(r->panel, T5_DP(6), 0);
    lv_obj_t *top = ui_box(r->panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(top, T5_DP(8), 0);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_icon_button(top, MD_ICON_ARROW_BACK, ruler_back, NULL);
    lv_obj_t *cb = ui_text_button(top, "Calibrate", false, &r->card_btn_l);
    lv_obj_add_event_cb(cb, ruler_card, LV_EVENT_CLICKED, r);
    r->scale_l = md_label_create(r->panel, "", MD_TYPE_CAPTION);
    lv_obj_set_style_text_opa(r->scale_l, md_scheme()->on_surface.medium, 0);
    lv_obj_t *adj = ui_box(r->panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(adj, T5_DP(8), 0);
    static const struct { const char *t; int step; } b[] = { { "\xE2\x88\x92" "1%", -20 }, { MINUS, -1 },
                                                             { "Reset", 0 }, { "+", 1 }, { "+1%", 20 } };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *bt = ui_text_button(adj, b[i].t, false, NULL);
        lv_obj_set_style_text_font(lv_obj_get_child(bt, 0), ui_font(28), 0);
        lv_obj_set_user_data(bt, r);
        lv_obj_add_event_cb(bt, ruler_adjust, LV_EVENT_CLICKED, (void *)(intptr_t)b[i].step);
    }
    lv_obj_set_hidden(adj, true);
    lv_obj_t *hint = ui_note(r->panel, "Lay a bank card in the outline, corner to the screen's corner, and adjust "
                                       "until they match.");
    lv_obj_set_width(hint, T5_DP(300));
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_hidden(hint, true);
    r->adj = adj;
    r->hint = hint;
    ruler_update(r);
}

/* ---- level and protractor ------------------------------------------------------------ */
typedef struct {
    bool protractor;
    lv_obj_t *view, *big, *sub, *hold_l;
    lv_timer_t *tick;
    bool have, held, imu_ok;
    meas_vec_t v;                 /* smoothed, screen axes */
    float zero_edge;              /* protractor: the reference angle */
    float shown;                  /* protractor: the angle on the display */
} level_t;

static float wrap180(float a) { return meas_angle_diff(a, 0); }

static void level_draw(lv_event_t *e)
{
    level_t *l = lv_event_get_user_data(e);
    lv_obj_t *o = lv_event_get_current_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    if (!l->have) return;
    const md_scheme_t *sc = md_scheme();
    lv_color_t ink = md_color(sc->c.on_surface), acc = md_color(sc->c.secondary), pri = md_color(sc->c.primary);
    int32_t cx = (a.x1 + a.x2) / 2, cy = (a.y1 + a.y2) / 2;
    int32_t R = LV_MIN(lv_area_get_width(&a), lv_area_get_height(&a)) / 2 - T5_DP(8);
    meas_tilt_t t = meas_tilt(l->v);
    if (!l->protractor && t.flat) {
        /* Bubble: moves towards the raised side, full scale at 10 degrees. */
        float tx = t.tilt_x - g_set.level_zero_x, ty = t.tilt_y - g_set.level_zero_y;
        float dist = sqrtf(tx * tx + ty * ty);
        bool level = dist < 0.3f;
        draw_circle(layer, cx, cy, R, 3, ink, 0x60, false);
        draw_circle(layer, cx, cy, R / 2, 2, ink, 0x40, false);
        draw_line(layer, cx - R, cy, cx + R, cy, 1, ink, 0x40);
        draw_line(layer, cx, cy - R, cx, cy + R, 1, ink, 0x40);
        float k = dist > 10 ? 10 / dist : 1;
        int32_t bx = cx + (int32_t)(tx * k / 10 * (R - T5_DP(24)));
        int32_t by = cy - (int32_t)(ty * k / 10 * (R - T5_DP(24)));
        draw_circle(layer, bx, by, T5_DP(24), 0, level ? acc : pri, 0xC0, true);
        draw_circle(layer, cx, cy, T5_DP(28), 3, ink, 0xA0, false);
        return;
    }
    /* Upright: a horizon and a plumb line, turned with the device. */
    float edge = t.edge;
    float ang = l->protractor ? l->shown : wrap180(edge - g_set.level_zero_edge);
    float rad = edge * 3.14159265f / 180.0f;
    /* "Up" in the world, drawn in screen pixels (y grows down). */
    float ux = sinf(rad), uy = -cosf(rad);
    draw_circle(layer, cx, cy, R, 3, ink, 0x60, false);
    for (int d = 0; d < 360; d += 10) {
        float r2 = d * 3.14159265f / 180.0f;
        int32_t len = d % 90 == 0 ? T5_DP(16) : d % 30 == 0 ? T5_DP(10) : T5_DP(6);
        draw_line(layer, cx + (int32_t)(sinf(r2) * R), cy - (int32_t)(cosf(r2) * R),
                  cx + (int32_t)(sinf(r2) * (R - len)), cy - (int32_t)(cosf(r2) * (R - len)), 2, ink, 0x80);
    }
    /* The world's horizontal through the centre. */
    draw_line(layer, cx - (int32_t)(uy * R), cy + (int32_t)(ux * R), cx + (int32_t)(uy * R), cy - (int32_t)(ux * R), 4,
              fabsf(meas_angle_diff(ang, roundf(ang / 90) * 90)) < 0.3f ? acc : pri, LV_OPA_COVER);
    /* The plumb line, down from the centre. */
    draw_line(layer, cx, cy, cx - (int32_t)(ux * R), cy - (int32_t)(uy * R), 2, ink, 0xA0);
    draw_circle(layer, cx, cy, T5_DP(5), 0, ink, LV_OPA_COVER, true);
}

static void level_text(level_t *l)
{
    char a[48], b[96];
    if (!l->imu_ok) {
        ui_set_text(l->big, "-");
        ui_set_text(l->sub, "The accelerometer isn't ready");
        return;
    }
    meas_tilt_t t = meas_tilt(l->v);
    if (l->protractor) {
        snprintf(a, sizeof a, "%.1f" DEG, (double)l->shown);
        if (l->held) snprintf(b, sizeof b, "Held. Tap Hold again to measure");
        else if (t.flat) snprintf(b, sizeof b, "Stand the Tab5 on its edge to measure");
        else if (l->zero_edge != 0) snprintf(b, sizeof b, "From your zero. Absolute %.1f" DEG, (double)wrap180(t.edge));
        else snprintf(b, sizeof b, "The bottom edge against level");
    } else if (t.flat) {
        float tx = t.tilt_x - g_set.level_zero_x, ty = t.tilt_y - g_set.level_zero_y;
        snprintf(a, sizeof a, "%.1f" DEG, (double)sqrtf(tx * tx + ty * ty));
        snprintf(b, sizeof b, "Left-right %.1f" DEG ",  front-back %.1f" DEG, (double)tx, (double)ty);
    } else {
        float e = wrap180(t.edge - g_set.level_zero_edge);
        float off = meas_angle_diff(e, roundf(e / 90) * 90);
        snprintf(a, sizeof a, "%.1f" DEG, (double)fabsf(off));
        snprintf(b, sizeof b, "Edge at %.1f" DEG ". Lay flat for the bubble", (double)e);
    }
    ui_set_text(l->big, a);
    ui_set_text(l->sub, b);
}

static void level_tick(lv_timer_t *tm)
{
    level_t *l = lv_timer_get_user_data(tm);
    t5_imu_accel_t acc;
    l->imu_ok = t5_imu_status() == T5_IMU_READY && t5_imu_read_accel(&acc) == T5_OK;
    if (l->imu_ok) {
        meas_vec_t v = meas_to_screen(rotation_deg(), acc.x, acc.y, acc.z);
        float k = l->have ? 0.2f : 1.0f;
        l->v.right = meas_smooth(l->v.right, v.right, k);
        l->v.up = meas_smooth(l->v.up, v.up, k);
        l->v.out = meas_smooth(l->v.out, v.out, k);
        l->have = true;
        if (l->protractor && !l->held) {
            meas_tilt_t t = meas_tilt(l->v);
            if (!t.flat) l->shown = wrap180(t.edge - l->zero_edge);
        }
    }
    level_text(l);
    lv_obj_invalidate(l->view);
}

static void level_zero(lv_event_t *e)
{
    level_t *l = lv_event_get_user_data(e);
    if (!l->have) return;
    meas_tilt_t t = meas_tilt(l->v);
    if (l->protractor) {
        l->zero_edge = t.flat ? 0 : t.edge;
        l->held = false;
        ui_set_text(l->hold_l, "Hold");
        ui_toast(l->zero_edge ? "Zero set: angles are from here" : "Zero cleared");
        return;
    }
    if (t.flat) {
        g_set.level_zero_x = t.tilt_x;
        g_set.level_zero_y = t.tilt_y;
    } else {
        float e2 = wrap180(t.edge);
        g_set.level_zero_edge = meas_angle_diff(e2, roundf(e2 / 90) * 90);
    }
    settings_changed();
    ui_toast("Calibrated: this is now level");
}

static void level_clear(lv_event_t *e)
{
    level_t *l = lv_event_get_user_data(e);
    if (l->protractor) {
        l->held = !l->held;
        ui_set_text(l->hold_l, l->held ? "Release" : "Hold");
        return;
    }
    g_set.level_zero_x = g_set.level_zero_y = g_set.level_zero_edge = 0;
    settings_changed();
    ui_toast("Calibration cleared");
}

static void level_layout(tool_page_t *tp)
{
    level_t *l = tp->data;
    lv_obj_set_flex_flow(tp->body, ui_portrait() ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    lv_obj_invalidate(l->view);
}

static void level_close(tool_page_t *tp)
{
    level_t *l = tp->data;
    if (l && l->tick) lv_timer_delete(l->tick);
}

static void level_common(tool_page_t *tp, bool protractor)
{
    level_t *l = calloc(1, sizeof *l);
    if (!l) return;
    tp->data = l;
    l->protractor = protractor;
    tp->on_close = level_close;
    tp->on_rotate = level_layout;
    lv_obj_t *body = tp->body;
    lv_obj_set_scrollable(body, false);
    lv_obj_set_style_pad_all(body, T5_DP(12), 0);
    lv_obj_set_style_pad_gap(body, T5_DP(12), 0);
    l->view = canvas_obj(body, level_draw, l);
    lv_obj_set_size(l->view, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(l->view, 1);
    lv_obj_set_style_min_width(l->view, T5_DP(200), 0);
    lv_obj_set_style_min_height(l->view, T5_DP(200), 0);
    lv_obj_set_align(l->view, LV_ALIGN_CENTER);
    lv_obj_t *side = ui_box(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(side, T5_DP(8), 0);
    lv_obj_set_flex_align(side, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(side, T5_DP(240));
    l->big = ui_label(side, "-", 68, LV_OPA_COVER);
    l->sub = md_label_create(side, "", MD_TYPE_BODY2);
    lv_obj_set_width(l->sub, LV_PCT(100));
    lv_label_set_long_mode(l->sub, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(l->sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(l->sub, md_scheme()->on_surface.medium, 0);
    lv_obj_t *row = ui_box(side, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, T5_DP(8), 0);
    lv_obj_t *z = ui_text_button(row, protractor ? "Set zero" : "Calibrate", true, NULL);
    lv_obj_add_event_cb(z, level_zero, LV_EVENT_CLICKED, l);
    lv_obj_t *c = ui_text_button(row, protractor ? "Hold" : "Clear", false, &l->hold_l);
    lv_obj_add_event_cb(c, level_clear, LV_EVENT_CLICKED, l);
    ui_note(side, protractor ? "Stand an edge on a surface and Set zero, then turn the Tab5 to the other surface."
                             : "Calibrate on a surface you know is level. The screen may turn as you tilt it: "
                               "readings follow.");
    level_layout(tp);
    l->tick = lv_timer_create(level_tick, 50, l);
    level_tick(l->tick);
}

void level_open(tool_page_t *tp) { level_common(tp, false); }
void protractor_open(tool_page_t *tp) { level_common(tp, true); }

/* ---- sound level ---------------------------------------------------------------- */
/* The reader task's state lives for the app, so the task can finish after
 * the page has gone. */
#define SND_HIST 120

static struct {
    volatile bool stop, alive;
    volatile float db;            /* the latest 100 ms reading, dBFS */
    volatile uint32_t seq;
    volatile int err;
} s_snd;

static void snd_task(void *arg)
{
    (void)arg;
    t5_audio_in_cfg_t cfg = { .rate = 16000, .channels = 1, .aec = false };
    t5_audio_in_t *in = NULL;
    t5_err_t e = t5_audio_in_open(&cfg, &in);
    if (e != T5_OK) {
        s_snd.err = (int)e;
        s_snd.alive = false;
        return;
    }
    static int16_t pcm[1600];                     /* 100 ms */
    size_t have = 0;
    while (!s_snd.stop && t5_app_running(t5_app_self())) {
        int n = t5_audio_in_read(in, (uint8_t *)pcm + have, sizeof pcm - have, 100);
        if (n < 0) {
            s_snd.err = n;
            break;
        }
        have += (size_t)n;
        if (have < sizeof pcm) continue;
        s_snd.db = meas_dbfs(pcm, 1600);
        s_snd.seq++;
        have = 0;
    }
    T5_IGNORE(t5_audio_in_close(in));             /* the OS closes it at exit anyway */
    s_snd.alive = false;
}

typedef struct {
    lv_obj_t *big, *unit, *stats, *bar, *chart, *msg, *allow, *off_l;
    lv_chart_series_t *ser;
    lv_timer_t *tick;
    uint32_t seen;
    meas_stats_t st;
    float shown;
    bool want, granted;
    uint32_t gen;
} snd_t;

static uint32_t s_snd_gen;

static float snd_offset(void)
{
    float o = g_set.sound_offset_db;
    return o == 0 ? 120.0f : o;     /* -26 dBFS at 94 dB SPL: a typical digital MEMS microphone */
}

static void snd_msg(snd_t *p, const char *text, bool show_allow)
{
    ui_set_text(p->msg, text);
    lv_obj_set_hidden(p->msg, !text[0]);
    lv_obj_set_hidden(p->allow, !show_allow);
}

static void snd_start(snd_t *p)
{
    p->want = true;
    if (s_snd.alive) return;                       /* the tick starts it once the old one is gone */
    s_snd.stop = false;
    s_snd.err = 0;
    s_snd.alive = true;
    t5_task_t h;
    if (t5_task_create("tb_sound", snd_task, NULL, 6144, 2, T5_CORE_ANY, &h) != T5_OK) {
        s_snd.alive = false;
        snd_msg(p, "Couldn't start the microphone", false);
    }
}

static void snd_stop(snd_t *p)
{
    p->want = false;
    s_snd.stop = true;
}

static void snd_tick(lv_timer_t *t)
{
    snd_t *p = lv_timer_get_user_data(t);
    if (p->want && !s_snd.alive && s_snd.stop) snd_start(p);       /* the old task has finished */
    if (s_snd.err) {
        char s[64];
        snprintf(s, sizeof s, "Microphone error: %s", t5_err_str((t5_err_t)s_snd.err));
        snd_msg(p, s, false);
        return;
    }
    if (s_snd.seq == p->seen) return;
    int n = (int)(s_snd.seq - p->seen);
    p->seen = s_snd.seq;
    float db = s_snd.db + snd_offset();
    for (int i = 0; i < n && i < 3; i++) meas_stats_add(&p->st, db);
    /* Shown: fast attack, slower release, like a meter's needle. */
    p->shown = db > p->shown ? db : meas_smooth(p->shown, db, 0.35f);
    char s[96];
    snprintf(s, sizeof s, "%.0f", (double)p->shown);
    ui_set_text(p->big, s);
    snprintf(s, sizeof s, "Max %.0f   Average (Leq) %.0f   Min %.0f dB", (double)p->st.max,
             (double)meas_stats_leq(&p->st), (double)p->st.min);
    ui_set_text(p->stats, s);
    int32_t v = (int32_t)p->shown;
    if (lv_bar_get_value(p->bar) != v) lv_bar_set_value(p->bar, v, LV_ANIM_OFF);
    lv_chart_set_next_value(p->chart, p->ser, (int32_t)db);
    snd_msg(p, "", false);
}

static void snd_perm(t5_app_t *app, uint32_t perm, bool granted, void *user)
{
    (void)app;
    (void)perm;
    tool_page_t *tp = tool_current();
    if (!tp || strcmp(tp->def->id, "measure.sound") != 0) return;   /* the page has gone */
    snd_t *p = tp->data;
    if (!p || p->gen != (uint32_t)(uintptr_t)user) return;
    p->granted = granted;
    if (granted) snd_start(p);
    else snd_msg(p, "Microphone access was refused. Allow it in App Manager to use this tool.", false);
}

static void snd_ask(snd_t *p)
{
    t5_app_t *app = t5_app_self();
    if (t5_app_has_permission(app, T5_PERM_MIC)) {
        p->granted = true;
        snd_start(p);
        return;
    }
    snd_msg(p, "The sound meter needs the microphone. Nothing is recorded or stored.", true);
    t5_err_t e = t5_app_request_permission(app, T5_PERM_MIC, snd_perm, (void *)(uintptr_t)p->gen);
    if (e != T5_OK) snd_msg(p, "Couldn't ask for the microphone", true);
}

static void snd_allow(lv_event_t *e) { snd_ask(lv_event_get_user_data(e)); }

static void snd_reset(lv_event_t *e)
{
    snd_t *p = lv_event_get_user_data(e);
    meas_stats_reset(&p->st);
}

static void snd_offset_l(snd_t *p)
{
    char s[48];
    snprintf(s, sizeof s, "Offset %+.0f dB", (double)snd_offset() - 120.0);
    ui_set_text(p->off_l, s);
}

static void snd_adjust(lv_event_t *e)
{
    snd_t *p = lv_obj_get_user_data(lv_event_get_current_target_obj(e));
    int step = (int)(intptr_t)lv_event_get_user_data(e);
    float o = snd_offset() + (float)step;
    if (o < 60) o = 60;
    if (o > 180) o = 180;
    g_set.sound_offset_db = o == 120.0f ? 0 : o;
    settings_changed();
    snd_offset_l(p);
}

static void snd_close(tool_page_t *tp)
{
    snd_t *p = tp->data;
    if (!p) return;
    snd_stop(p);
    if (p->tick) lv_timer_delete(p->tick);
}

static void snd_suspend(tool_page_t *tp)
{
    snd_t *p = tp->data;
    if (p) {
        bool want = p->want;
        snd_stop(p);
        p->want = false;
        p->granted = want;                          /* remember to restart */
    }
}

static void snd_resume(tool_page_t *tp)
{
    snd_t *p = tp->data;
    if (p && p->granted && t5_app_has_permission(t5_app_self(), T5_PERM_MIC)) snd_start(p);
}

void sound_open(tool_page_t *tp)
{
    snd_t *p = calloc(1, sizeof *p);
    if (!p) return;
    tp->data = p;
    tp->on_close = snd_close;
    tp->on_suspend = snd_suspend;
    tp->on_resume = snd_resume;
    p->gen = ++s_snd_gen;
    meas_stats_reset(&p->st);
    lv_obj_t *body = tp->body;
    lv_obj_set_style_pad_all(body, T5_DP(16), 0);
    lv_obj_set_style_pad_row(body, T5_DP(8), 0);
    const md_scheme_t *sc = md_scheme();

    lv_obj_t *top = ui_box(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(top, T5_DP(8), 0);
    p->big = ui_label(top, "-", 68, LV_OPA_COVER);
    p->unit = md_label_create(top, "dB (estimate)", MD_TYPE_SUBTITLE1);
    lv_obj_set_style_text_opa(p->unit, sc->on_surface.medium, 0);
    lv_obj_set_style_pad_bottom(p->unit, T5_DP(10), 0);
    lv_obj_t *sp = lv_obj_create(top);
    lv_obj_remove_style_all(sp);
    lv_obj_set_flex_grow(sp, 1);
    lv_obj_t *rb = ui_text_button(top, "Reset", false, NULL);
    lv_obj_add_event_cb(rb, snd_reset, LV_EVENT_CLICKED, p);

    p->bar = lv_bar_create(body);
    lv_bar_set_range(p->bar, 20, 120);
    lv_obj_set_size(p->bar, LV_PCT(100), T5_DP(10));
    lv_obj_set_style_bg_color(p->bar, md_color(sc->c.secondary), LV_PART_INDICATOR);
    p->stats = md_label_create(body, "Max -   Average (Leq) -   Min - dB", MD_TYPE_BODY2);
    p->msg = md_label_create(body, "", MD_TYPE_BODY1);
    lv_obj_set_width(p->msg, LV_PCT(100));
    lv_label_set_long_mode(p->msg, LV_LABEL_LONG_MODE_WRAP);
    p->allow = ui_text_button(body, "Allow the microphone", true, NULL);
    lv_obj_add_event_cb(p->allow, snd_allow, LV_EVENT_CLICKED, p);

    p->chart = lv_chart_create(body);
    lv_obj_set_size(p->chart, LV_PCT(100), T5_DP(120));
    lv_chart_set_type(p->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(p->chart, SND_HIST);
    lv_chart_set_axis_range(p->chart, LV_CHART_AXIS_PRIMARY_Y, 20, 120);
    lv_chart_set_update_mode(p->chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(p->chart, 5, 0);
    lv_obj_set_style_bg_opa(p->chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p->chart, 1, 0);
    lv_obj_set_style_border_color(p->chart, md_color(sc->c.on_surface), 0);
    lv_obj_set_style_border_opa(p->chart, 0x30, 0);
    lv_obj_set_style_line_color(p->chart, md_color(sc->c.on_surface), LV_PART_MAIN);
    lv_obj_set_style_line_opa(p->chart, 0x20, LV_PART_MAIN);
    lv_obj_set_style_size(p->chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(p->chart, 2, LV_PART_ITEMS);
    p->ser = lv_chart_add_series(p->chart, md_color(sc->c.secondary), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(p->chart, p->ser, LV_CHART_POINT_NONE);
    md_label_create(body, "The last 12 seconds, 20 to 120 dB", MD_TYPE_CAPTION);

    lv_obj_t *cal = ui_box(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cal, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cal, T5_DP(8), 0);
    p->off_l = md_label_create(cal, "", MD_TYPE_BODY2);
    static const struct { const char *t; int step; } b[] = { { MINUS "5", -5 }, { MINUS "1", -1 }, { "+1", 1 },
                                                             { "+5", 5 } };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *bt = ui_text_button(cal, b[i].t, false, NULL);
        lv_obj_set_style_text_font(lv_obj_get_child(bt, 0), ui_font(28), 0);
        lv_obj_set_user_data(bt, p);
        lv_obj_add_event_cb(bt, snd_adjust, LV_EVENT_CLICKED, (void *)(intptr_t)b[i].step);
    }
    snd_offset_l(p);
    ui_note(body, "Unweighted (Z), 100 ms readings. The microphone isn't calibrated, so levels are estimates from a "
                  "typical sensitivity: set the offset against a reference meter if you have one. Holding the "
                  "Tab5 or speaking near it changes the reading.");
    snd_msg(p, "", false);
    p->tick = lv_timer_create(snd_tick, 100, p);
    snd_ask(p);
}
