/* Shared UI: fonts, small helpers, the number keypad and forms. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

extern lv_font_t tb_sym_24, tb_sym_28, tb_sym_32, tb_sym_48, tb_sym_68;

static uint64_t s_entropy = 0x6A09E667F3BCC909ull;

uint64_t ui_entropy(void)
{
    s_entropy = (s_entropy ^ (uint64_t)t5_time_us()) * 0x9E3779B97F4A7C15ull;
    s_entropy ^= s_entropy >> 29;
    return s_entropy;
}

/* ---- fonts ------------------------------------------------------------------- */
const lv_font_t *ui_font(int px)
{
    lv_font_t *f;
    md_type_role_t role;
    if (px <= 24) f = &tb_sym_24, role = MD_TYPE_CAPTION;
    else if (px <= 28) f = &tb_sym_28, role = MD_TYPE_BODY2;
    else if (px <= 32) f = &tb_sym_32, role = MD_TYPE_BODY1;
    else if (px <= 48) f = &tb_sym_48, role = MD_TYPE_HEADLINE5;
    else f = &tb_sym_68, role = MD_TYPE_HEADLINE4;
    if (!f->fallback) {
        /* Ours has only a few symbols: everything else, and the line
         * metrics, come from the OS font of the same size. */
        const lv_font_t *fb = md_font(role);
        f->fallback = fb;
        f->line_height = fb->line_height;
        f->base_line = fb->base_line;
    }
    return f;
}

/* ---- helpers ------------------------------------------------------------------- */
lv_obj_t *ui_box(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return o;
}

lv_obj_t *ui_label(lv_obj_t *parent, const char *text, int px, lv_opa_t opa)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, ui_font(px), 0);
    lv_obj_set_style_text_color(l, md_color(md_scheme()->c.on_surface), 0);
    lv_obj_set_style_text_opa(l, opa, 0);
    lv_label_set_text(l, text ? text : "");
    return l;
}

void ui_set_text(lv_obj_t *label, const char *text)
{
    if (!label) return;
    const char *now = lv_label_get_text(label);
    if (now && strcmp(now, text) == 0) return;
    lv_label_set_text(label, text);
}

lv_obj_t *ui_note(lv_obj_t *parent, const char *text)
{
    lv_obj_t *l = md_label_create(parent, text, MD_TYPE_CAPTION);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_opa(l, md_scheme()->on_surface.medium, 0);
    return l;
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, md_button_variant_t v, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = md_button_create(parent, text, v);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

lv_obj_t *ui_icon_button(lv_obj_t *parent, const char *icon, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, T5_DP(48), T5_DP(48));
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, md_color(md_scheme()->c.on_surface), 0);
    lv_obj_set_style_bg_opa(b, md_scheme()->states.pressed, LV_STATE_PRESSED);
    lv_obj_set_scrollable(b, false);
    lv_obj_t *i = md_icon_create(b, icon, 24);
    lv_obj_center(i);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

lv_obj_t *ui_text_button(lv_obj_t *parent, const char *text, bool filled, lv_obj_t **label)
{
    const md_scheme_t *sc = md_scheme();
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, LV_SIZE_CONTENT, T5_DP(48));
    lv_obj_set_style_min_width(b, T5_DP(64), 0);
    lv_obj_set_style_radius(b, T5_DP(8), 0);
    lv_obj_set_style_pad_hor(b, T5_DP(16), 0);
    lv_obj_set_scrollable(b, false);
    lv_obj_set_clickable(b, true);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (filled) {
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, md_color(sc->c.primary), 0);
        lv_obj_set_style_bg_color(b, md_color(md_mix(sc->c.on_primary, sc->c.primary, sc->states.pressed)),
                                  LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, md_color(sc->c.on_surface), 0);
        lv_obj_set_style_border_opa(b, 0x60, 0);
        lv_obj_set_style_bg_color(b, md_color(sc->c.primary), 0);
        lv_obj_set_style_bg_opa(b, sc->states.pressed, LV_STATE_PRESSED);
    }
    lv_obj_t *l = md_label_create(b, text, MD_TYPE_BUTTON);
    lv_obj_set_style_text_color(l, md_color(filled ? sc->c.on_primary : sc->c.primary), 0);
    if (label) *label = l;
    return b;
}

void ui_card_style(lv_obj_t *o)
{
    md_set_elevation(o, 1, false);
    lv_obj_set_style_radius(o, T5_DP(8), 0);
    lv_obj_set_style_border_width(o, 0, 0);
}

void ui_toast(const char *text)
{
    md_snackbar_show(text, NULL, NULL, NULL);
}

lv_obj_t *ui_chips(lv_obj_t *parent, const char *const *labels, int n, int sel, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *row = ui_box(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_pad_column(row, T5_DP(8), 0);
    lv_obj_set_style_pad_row(row, T5_DP(4), 0);
    for (int i = 0; i < n; i++) {
        lv_obj_t *c = md_chip_create(row, labels[i], NULL, MD_CHIP_CHOICE);
        if (i == sel) md_chip_set_selected(c, true);
        if (cb) lv_obj_add_event_cb(c, cb, LV_EVENT_VALUE_CHANGED, ud);
    }
    return row;
}

int ui_chip_index(lv_obj_t *chip)
{
    return (int)lv_obj_get_index(chip);
}

int ui_chips_selected(lv_obj_t *row)
{
    uint32_t n = lv_obj_get_child_count(row);
    for (uint32_t i = 0; i < n; i++)
        if (md_chip_selected(lv_obj_get_child(row, (int32_t)i))) return (int)i;
    return -1;
}

/* ---- forms ------------------------------------------------------------------- */
#define FORM_MAX 12

typedef struct {
    lv_obj_t *row, *label, *value, *unit;
    entry_t e;
    field_kind_t kind;
    uint32_t seq;
    bool error, hidden;
    char hint[64];
    int save_idx;              /* which saved value it uses, -1 none */
} field_t;

typedef enum { K_DIGIT, K_POINT, K_SIGN, K_BACK, K_CLEAR, K_NEXT } key_op_t;
typedef struct {
    const char *label;          /* NULL: icon */
    const char *icon;
    uint8_t op;
    char arg;
    uint8_t grow;
} kp_key_t;

struct form {
    tool_page_t *tp;
    lv_obj_t *split, *fields, *extra, *keypad;
    lv_obj_t *k_point, *k_sign, *k_hex[6];
    keypad_kind_t kp;
    field_t f[FORM_MAX];
    int nf, active, ninputs;
    uint32_t seq;
    form_cb_t compute;
    void *user;
    bool ready;
};

static const kp_key_t s_keys[4][4] = {
    { { "7", 0, K_DIGIT, '7', 1 }, { "8", 0, K_DIGIT, '8', 1 }, { "9", 0, K_DIGIT, '9', 1 },
      { NULL, MD_ICON_BACKSPACE, K_BACK, 0, 1 } },
    { { "4", 0, K_DIGIT, '4', 1 }, { "5", 0, K_DIGIT, '5', 1 }, { "6", 0, K_DIGIT, '6', 1 },
      { "C", 0, K_CLEAR, 0, 1 } },
    { { "1", 0, K_DIGIT, '1', 1 }, { "2", 0, K_DIGIT, '2', 1 }, { "3", 0, K_DIGIT, '3', 1 },
      { "\xC2\xB1", 0, K_SIGN, 0, 1 } },
    { { "0", 0, K_DIGIT, '0', 1 }, { ".", 0, K_POINT, 0, 1 }, { NULL, MD_ICON_KEYBOARD_TAB, K_NEXT, 0, 2 },
      { NULL, NULL, 0, 0, 0 } },
};
static const kp_key_t s_hex_keys[6] = {
    { "A", 0, K_DIGIT, 'A', 1 }, { "B", 0, K_DIGIT, 'B', 1 }, { "C", 0, K_DIGIT, 'C', 1 },
    { "D", 0, K_DIGIT, 'D', 1 }, { "E", 0, K_DIGIT, 'E', 1 }, { "F", 0, K_DIGIT, 'F', 1 },
};

static bool is_input(const field_t *fl) { return fl->kind != F_OUT; }
static field_t *fld(form_t *f, int i);

static void field_show(form_t *f, int i)
{
    field_t *fl = &f->f[i];
    const md_scheme_t *sc = md_scheme();
    char buf[96];
    lv_opa_t opa = sc->on_surface.high;
    md_rgb_t col = sc->c.on_surface;
    if (fl->kind == F_OUT) {
        snprintf(buf, sizeof buf, "%s", fl->hint[0] ? fl->hint : "-");
        col = fl->error ? sc->c.error : sc->c.secondary;
    } else if (fl->kind == F_DATE && fl->e.text[0]) {
        /* Typed digits into YYYY-MM-DD, the rest as underscores. */
        const char *mask = "____-__-__";
        size_t d = 0, len = strlen(fl->e.text);
        for (size_t k = 0; mask[k]; k++) buf[k] = mask[k] == '-' ? '-' : (d < len ? fl->e.text[d++] : '_');
        buf[10] = '\0';
    } else if (fl->e.text[0]) {
        snprintf(buf, sizeof buf, "%s", fl->e.text);
        if (fl->e.text[0] == '-') snprintf(buf, sizeof buf, MINUS "%s", fl->e.text + 1);
    } else {
        /* Empty: the computed value if there is one, dimmed. */
        snprintf(buf, sizeof buf, "%s", fl->hint[0] ? fl->hint : fl->kind == F_DATE ? "YYYY-MM-DD" : "");
        opa = sc->on_surface.disabled;
        if (fl->hint[0] && fl->kind != F_DATE) opa = sc->on_surface.medium;
    }
    if (fl->error && fl->kind != F_OUT) col = sc->c.error;
    ui_set_text(fl->value, buf);
    lv_obj_set_style_text_color(fl->value, md_color(col), 0);
    lv_obj_set_style_text_opa(fl->value, opa, 0);
    if (is_input(fl)) {
        bool act = i == f->active;
        lv_obj_set_style_border_width(fl->row, act ? T5_DP(2) : 0, 0);
    }
}

static void keypad_update(form_t *f)
{
    if (!f->keypad) return;
    field_t *fl = f->active >= 0 ? &f->f[f->active] : NULL;
    bool point = fl && fl->e.allow_point, sign = fl && fl->e.allow_sign, hex = fl && fl->e.hex;
    lv_obj_t *ks[2] = { f->k_point, f->k_sign };
    bool on[2] = { point, sign };
    for (int k = 0; k < 2; k++) {
        if (!ks[k]) continue;
        lv_obj_set_state(ks[k], LV_STATE_DISABLED, !on[k]);
        lv_obj_set_style_opa(ks[k], on[k] ? LV_OPA_COVER : LV_OPA_40, 0);
    }
    for (int k = 0; k < 6; k++) {
        if (!f->k_hex[k]) continue;
        lv_obj_set_state(f->k_hex[k], LV_STATE_DISABLED, !hex);
        lv_obj_set_style_opa(f->k_hex[k], hex ? LV_OPA_COVER : LV_OPA_40, 0);
    }
}

static void save_inputs(form_t *f)
{
    if (!f->ready || !f->tp || !f->tp->def) return;
    tb_slot_t *sl = slot_get(f->tp->def->id, true);
    if (!sl) return;
    for (int i = 0; i < f->nf; i++)
        if (f->f[i].save_idx >= 0) snprintf(sl->vals[f->f[i].save_idx], TB_VAL_LEN, "%s", f->f[i].e.text);
    settings_changed();
}

void form_recompute(form_t *f)
{
    if (f->compute) f->compute(f);
    for (int i = 0; i < f->nf; i++) field_show(f, i);
}

static void set_active(form_t *f, int i)
{
    int old = f->active;
    f->active = i;
    if (old >= 0) field_show(f, old);
    if (i >= 0) {
        field_show(f, i);
        lv_obj_scroll_to_view(f->f[i].row, LV_ANIM_ON);
    }
    keypad_update(f);
}

static void next_input(form_t *f)
{
    for (int k = 1; k <= f->nf; k++) {
        int i = (f->active + k) % f->nf;
        if (is_input(&f->f[i]) && !f->f[i].hidden) {
            set_active(f, i);
            return;
        }
    }
}

static void key_clicked(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    const kp_key_t *k = lv_obj_get_user_data(lv_event_get_current_target_obj(e));
    if (!f || !k) return;
    ui_entropy();
    if (k->op == K_NEXT) {
        next_input(f);
        return;
    }
    if (f->active < 0) return;
    field_t *fl = &f->f[f->active];
    static const entry_op_t map[] = { ENTRY_DIGIT, ENTRY_POINT, ENTRY_SIGN, ENTRY_BACK, ENTRY_CLEAR };
    if (!entry_key(&fl->e, map[k->op], k->arg)) return;
    fl->seq = ++f->seq;
    save_inputs(f);
    form_recompute(f);
}

static lv_obj_t *key_create(form_t *f, lv_obj_t *row, const kp_key_t *k, bool accent)
{
    const md_scheme_t *sc = md_scheme();
    md_rgb_t base = sc->c.background;
    md_rgb_t bg = accent ? md_mix(sc->c.primary, base, 0x38) : md_mix(sc->c.on_surface, base, 0x1f);
    md_rgb_t ink = accent ? sc->c.primary : sc->c.on_surface;
    lv_obj_t *b = lv_obj_create(row);
    lv_obj_remove_style_all(b);
    lv_obj_set_flex_grow(b, k->grow);
    lv_obj_set_height(b, LV_PCT(100));
    lv_obj_set_style_radius(b, T5_DP(10), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, md_color(bg), 0);
    lv_obj_set_style_bg_color(b, md_color(md_mix(ink, bg, sc->states.pressed + 0x18)), LV_STATE_PRESSED);
    lv_obj_set_scrollable(b, false);
    lv_obj_set_user_data(b, (void *)k);
    lv_obj_add_event_cb(b, key_clicked, LV_EVENT_CLICKED, f);
    lv_obj_t *l = k->label ? ui_label(b, k->label, 48, LV_OPA_COVER) : md_icon_create(b, k->icon, 24);
    lv_obj_set_style_text_color(l, md_color(ink), 0);
    lv_obj_center(l);
    return b;
}

static lv_obj_t *key_row(lv_obj_t *parent)
{
    lv_obj_t *r = ui_box(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(r, LV_PCT(100));
    lv_obj_set_flex_grow(r, 1);
    lv_obj_set_style_pad_column(r, T5_DP(6), 0);
    return r;
}

static void keypad_create(form_t *f)
{
    f->keypad = ui_box(f->split, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f->keypad, T5_DP(6), 0);
    if (f->kp == KP_HEX) {
        lv_obj_t *r = key_row(f->keypad);
        for (int c = 0; c < 6; c++) f->k_hex[c] = key_create(f, r, &s_hex_keys[c], false);
    }
    for (int rr = 0; rr < 4; rr++) {
        lv_obj_t *r = key_row(f->keypad);
        for (int c = 0; c < 4; c++) {
            const kp_key_t *k = &s_keys[rr][c];
            if (!k->grow) continue;
            bool accent = k->op == K_BACK || k->op == K_CLEAR || k->op == K_NEXT;
            lv_obj_t *b = key_create(f, r, k, accent);
            if (k->op == K_POINT) f->k_point = b;
            if (k->op == K_SIGN) f->k_sign = b;
        }
    }
}

void form_layout(form_t *f)
{
    bool portrait = ui_portrait();
    lv_obj_set_flex_flow(f->split, portrait ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    if (portrait) {
        lv_obj_set_size(f->fields, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(f->fields, 1);
        lv_obj_set_size(f->keypad, LV_PCT(100), T5_DP(f->kp == KP_HEX ? 290 : 236));
    } else {
        lv_obj_set_size(f->fields, LV_SIZE_CONTENT, LV_PCT(100));
        lv_obj_set_flex_grow(f->fields, 1);
        lv_obj_set_size(f->keypad, T5_DP(f->kp == KP_HEX ? 260 : 224), LV_PCT(100));
    }
}

static void form_deleted(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    if (f && f->tp && f->tp->form == f) f->tp->form = NULL;
    free(f);
}

form_t *form_create(tool_page_t *tp, form_cb_t compute, keypad_kind_t kp)
{
    form_t *f = calloc(1, sizeof *f);
    if (!f) return NULL;
    f->tp = tp;
    f->compute = compute;
    f->kp = kp;
    f->active = -1;
    tp->form = f;
    lv_obj_set_scrollable(tp->body, false);
    lv_obj_set_style_pad_bottom(tp->body, 0, 0);
    f->split = ui_box(tp->body, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(f->split, LV_PCT(100));
    lv_obj_set_flex_grow(f->split, 1);
    lv_obj_set_style_pad_all(f->split, T5_DP(8), 0);
    lv_obj_set_style_pad_gap(f->split, T5_DP(8), 0);
    lv_obj_add_event_cb(f->split, form_deleted, LV_EVENT_DELETE, f);

    f->fields = ui_box(f->split, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(f->fields, true);
    lv_obj_set_scroll_dir(f->fields, LV_DIR_VER);
    lv_obj_set_style_pad_row(f->fields, T5_DP(6), 0);
    lv_obj_set_style_pad_right(f->fields, T5_DP(4), 0);
    keypad_create(f);
    form_layout(f);
    return f;
}

static void field_clicked(lv_event_t *e)
{
    form_t *f = lv_event_get_user_data(e);
    lv_obj_t *row = lv_event_get_current_target_obj(e);
    for (int i = 0; i < f->nf; i++)
        if (f->f[i].row == row) set_active(f, i);
}

static int field_add(form_t *f, const char *label, const char *unit, field_kind_t kind)
{
    if (f->nf >= FORM_MAX) return -1;
    int i = f->nf++;
    field_t *fl = &f->f[i];
    const md_scheme_t *sc = md_scheme();
    fl->kind = kind;
    fl->save_idx = -1;
    fl->row = ui_box(f->fields, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(fl->row, LV_PCT(100));
    lv_obj_set_style_pad_hor(fl->row, T5_DP(12), 0);
    lv_obj_set_style_pad_ver(fl->row, T5_DP(4), 0);
    lv_obj_set_style_radius(fl->row, T5_DP(8), 0);
    if (kind != F_OUT) {
        lv_obj_set_style_bg_opa(fl->row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(fl->row, md_color(md_mix(sc->c.on_surface, sc->c.background, 0x14)), 0);
        lv_obj_set_style_border_color(fl->row, md_color(sc->c.primary), 0);
        lv_obj_set_clickable(fl->row, true);
        lv_obj_add_event_cb(fl->row, field_clicked, LV_EVENT_CLICKED, f);
    }
    fl->label = ui_label(fl->row, label, 24, sc->on_surface.medium);
    lv_obj_t *line = ui_box(fl->row, LV_FLEX_FLOW_ROW);
    lv_obj_set_width(line, LV_PCT(100));
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(line, T5_DP(6), 0);
    fl->value = ui_label(line, "", 48, LV_OPA_COVER);
    lv_obj_set_style_max_width(fl->value, LV_PCT(85), 0);
    lv_label_set_long_mode(fl->value, kind == F_OUT ? LV_LABEL_LONG_MODE_WRAP : LV_LABEL_LONG_MODE_DOTS);
    fl->unit = ui_label(line, unit ? unit : "", 32, md_scheme()->on_surface.medium);
    lv_obj_set_style_pad_bottom(fl->unit, T5_DP(3), 0);
    return i;
}

int form_input(form_t *f, const char *label, const char *unit, field_kind_t kind, const char *initial)
{
    int i = field_add(f, label, unit, kind);
    if (i < 0) return i;
    field_t *fl = &f->f[i];
    entry_init(&fl->e, kind == F_NUM, kind == F_NUM || kind == F_INT, kind == F_HEX);
    if (kind == F_DATE) fl->e.max_len = 8;
    if (kind == F_HEX) fl->e.max_len = 16;
    if (kind == F_UNSIGNED || kind == F_INT) fl->e.max_len = 15;
    if (initial) entry_set(&fl->e, initial);
    if (f->ninputs < TB_VALS) fl->save_idx = f->ninputs;
    f->ninputs++;
    return i;
}

int form_output(form_t *f, const char *label)
{
    return field_add(f, label, "", F_OUT);
}

void form_set_nosave(form_t *f, int i)
{
    field_t *fl = fld(f, i);
    if (!fl || fl->save_idx < 0) return;
    /* Only the last input added can give its slot back. */
    if (fl->save_idx == f->ninputs - 1) f->ninputs--;
    fl->save_idx = -1;
}

void form_set_value_font(form_t *f, int i, int px, bool wrap)
{
    field_t *fl = fld(f, i);
    if (!fl) return;
    lv_obj_set_style_text_font(fl->value, ui_font(px), 0);
    lv_label_set_long_mode(fl->value, wrap ? LV_LABEL_LONG_MODE_WRAP : LV_LABEL_LONG_MODE_DOTS);
    if (wrap) lv_obj_set_width(fl->value, LV_PCT(85));
}

void form_set_max_len(form_t *f, int i, int len)
{
    field_t *fl = fld(f, i);
    if (fl) fl->e.max_len = len;
}

lv_obj_t *form_extra(form_t *f)
{
    lv_obj_t *b = ui_box(f->fields, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_style_pad_row(b, T5_DP(6), 0);
    lv_obj_set_style_pad_ver(b, T5_DP(2), 0);
    return b;
}

lv_obj_t *form_fields(form_t *f) { return f->fields; }

void form_ready(form_t *f)
{
    tb_slot_t *sl = slot_get(f->tp->def->id, false);
    if (sl) {
        for (int i = 0; i < f->nf; i++) {
            field_t *fl = &f->f[i];
            if (fl->save_idx < 0) continue;
            entry_t tmp = fl->e;
            tmp.text[0] = '\0';
            /* Typed again through the rules, so a damaged file can't put
             * anything odd in a field. */
            const char *s = sl->vals[fl->save_idx];
            for (size_t k = 0; s[k] && k < TB_VAL_LEN; k++) {
                char c = s[k];
                if (c == '-' && k == 0) entry_key(&tmp, ENTRY_SIGN, 0);
                else if (c == '.') entry_key(&tmp, ENTRY_POINT, 0);
                else entry_key(&tmp, ENTRY_DIGIT, c);
            }
            fl->e = tmp;
            if (fl->e.text[0]) fl->seq = ++f->seq;
        }
    }
    f->ready = true;
    for (int i = 0; i < f->nf; i++)
        if (is_input(&f->f[i]) && !f->f[i].hidden) {
            f->active = i;
            break;
        }
    keypad_update(f);
    form_recompute(f);
}

static field_t *fld(form_t *f, int i) { return f && i >= 0 && i < f->nf ? &f->f[i] : NULL; }

bool form_get(form_t *f, int i, double *out)
{
    field_t *fl = fld(f, i);
    if (!fl || fl->kind == F_OUT || fl->kind == F_HEX || fl->kind == F_DATE) return false;
    return parse_num(fl->e.text, out);
}

const char *form_text(form_t *f, int i)
{
    field_t *fl = fld(f, i);
    return fl ? fl->e.text : "";
}

void form_set_text(form_t *f, int i, const char *text)
{
    field_t *fl = fld(f, i);
    if (!fl || fl->kind == F_OUT) return;
    entry_set(&fl->e, text);
    if (!text || !*text) fl->seq = 0;
    save_inputs(f);
    field_show(f, i);
}

void form_set_out(form_t *f, int i, const char *text)
{
    field_t *fl = fld(f, i);
    if (!fl) return;
    snprintf(fl->hint, sizeof fl->hint, "%s", text ? text : "");
    field_show(f, i);
}

void form_set_hint(form_t *f, int i, const char *text) { form_set_out(f, i, text); }

void form_set_label(form_t *f, int i, const char *text)
{
    field_t *fl = fld(f, i);
    if (fl) ui_set_text(fl->label, text);
}

void form_set_unit(form_t *f, int i, const char *text)
{
    field_t *fl = fld(f, i);
    if (fl) ui_set_text(fl->unit, text);
}

void form_set_hidden(form_t *f, int i, bool hidden)
{
    field_t *fl = fld(f, i);
    if (!fl || fl->hidden == hidden) return;
    fl->hidden = hidden;
    lv_obj_set_hidden(fl->row, hidden);
    if (hidden && f->active == i) next_input(f);
}

uint32_t form_seq(form_t *f, int i)
{
    field_t *fl = fld(f, i);
    return fl && fl->e.text[0] ? fl->seq : 0;
}

void form_set_error(form_t *f, int i, bool error)
{
    field_t *fl = fld(f, i);
    if (!fl || fl->error == error) return;
    fl->error = error;
    field_show(f, i);
}

int form_active(form_t *f) { return f->active; }

bool form_is_date(form_t *f, int i)
{
    field_t *fl = fld(f, i);
    return fl && fl->kind == F_DATE;
}

void *form_user(form_t *f) { return f->user; }
void form_set_user(form_t *f, void *user) { f->user = user; }
tool_page_t *form_page(form_t *f) { return f->tp; }

int32_t form_int(form_t *f, int k, int32_t def)
{
    tb_slot_t *sl = slot_get(f->tp->def->id, false);
    if (!sl || k < 0 || k >= 4) return def;
    return sl->ints[k] ? sl->ints[k] - 1 : def;        /* stored +1, so 0 means unset */
}

void form_set_int(form_t *f, int k, int32_t v)
{
    tb_slot_t *sl = slot_get(f->tp->def->id, true);
    if (!sl || k < 0 || k >= 4 || sl->ints[k] == v + 1) return;
    sl->ints[k] = v + 1;
    settings_changed();
}
