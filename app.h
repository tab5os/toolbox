/* Toolbox: what the screens share. The maths is in engine/, the UI here. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <t5/app.h>
#include <t5/shell.h>
#include <t5/storage.h>
#include <t5/sys.h>
#include <t5/ui.h>
#include "engine/fmt.h"
#include "engine/units.h"

/* ---- tools ------------------------------------------------------------------ */
typedef enum { CAT_RECENT, CAT_CONVERT, CAT_NUMBERS, CAT_MONEY, CAT_ELEC, CAT_TIME, CAT_COLOUR, CAT_MEASURE,
               CAT_COUNT } tool_cat_t;

typedef struct tool_page tool_page_t;
typedef struct {
    const char *id;            /* stable, saved: "conv.length" */
    const char *name;
    const char *icon;          /* MD_ICON_* */
    uint8_t cat;
    void (*open)(tool_page_t *tp);
    int arg;                   /* the converter's category, and so on */
} tool_def_t;

extern const tool_def_t g_tools[];
extern const int g_tool_count;
const tool_def_t *tool_find(const char *id);
const char *cat_name(tool_cat_t c);
const char *cat_icon(tool_cat_t c);

/* The open tool. One at a time, as an overlay over the home screen. */
struct tool_page {
    const tool_def_t *def;
    lv_obj_t *page, *bar, *body;
    void *data;                              /* the tool's own state */
    struct form *form;                       /* set by form_create */
    void (*on_close)(tool_page_t *tp);       /* stop timers and tasks; data is freed after */
    void (*on_rotate)(tool_page_t *tp);
    void (*on_suspend)(tool_page_t *tp);
    void (*on_resume)(tool_page_t *tp);
    bool no_free;                            /* a task still owns data: leave it */
};

void tool_open(const tool_def_t *def);
void tool_close(void);
tool_page_t *tool_current(void);
bool ui_portrait(void);
void ui_set_fullscreen(bool on);

/* ---- saved settings --------------------------------------------------------- */
#define TB_ID_LEN 20
#define TB_VALS 6
#define TB_VAL_LEN 24
#define TB_SLOTS 48
#define TB_RECENT 8
#define TB_MRU 6

typedef struct {
    char tool[TB_ID_LEN];
    char vals[TB_VALS][TB_VAL_LEN];
    int32_t ints[4];                          /* chips, modes */
} tb_slot_t;

typedef struct {
    char tool[TB_ID_LEN];
    char text[64];                            /* "12 in = 30.48 cm" */
    uint8_t from, to;
    char value[TB_VAL_LEN];
} tb_recent_t;

typedef struct {
    uint32_t magic, version;
    char open_tool[TB_ID_LEN];                /* the tool on screen at exit, "" for home */
    uint8_t home_cat;
    char mru[TB_MRU][TB_ID_LEN];              /* most recently used tools */
    tb_recent_t recent[TB_RECENT];
    float ruler_px_mm;                        /* 0: the default */
    float sound_offset_db;                    /* dB SPL = dBFS + this */
    float level_zero_x, level_zero_y, level_zero_edge;
    tb_slot_t slots[TB_SLOTS];
} tb_settings_t;

extern tb_settings_t g_set;
void settings_changed(void);                  /* save soon, debounced */
void settings_save_now(void);
tb_slot_t *slot_get(const char *tool, bool create);
void recent_add(const char *tool, const char *text, int from, int to, const char *value);

/* ---- look ------------------------------------------------------------------- */
/* Fonts that add Omega, minus, approximately, <= and >= to the OS's Roboto:
 * 24 (caption), 28 (body2), 32 (body1), 48 (headline5) or 68 (headline4) px. */
const lv_font_t *ui_font(int px);
#define OHM "\xCE\xA9"
#define MINUS "\xE2\x88\x92"
#define APPROX "\xE2\x89\x88"
#define DEG "\xC2\xB0"
#define MICRO "\xC2\xB5"

lv_obj_t *ui_box(lv_obj_t *parent, lv_flex_flow_t flow);
lv_obj_t *ui_label(lv_obj_t *parent, const char *text, int px, lv_opa_t opa);
void ui_set_text(lv_obj_t *label, const char *text);      /* only when it changed */
lv_obj_t *ui_note(lv_obj_t *parent, const char *text);    /* small print, wraps */
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, md_button_variant_t v, lv_event_cb_t cb, void *ud);
/* A button whose text can change later (*label). */
lv_obj_t *ui_text_button(lv_obj_t *parent, const char *text, bool filled, lv_obj_t **label);
lv_obj_t *ui_icon_button(lv_obj_t *parent, const char *icon, lv_event_cb_t cb, void *ud);
/* A row of choice chips; cb gets LV_EVENT_VALUE_CHANGED with the chip's
 * index in the user data's ->chip_index (see ui_chip_index). */
lv_obj_t *ui_chips(lv_obj_t *parent, const char *const *labels, int n, int selected, lv_event_cb_t cb, void *ud);
int ui_chip_index(lv_obj_t *chip);            /* the index ui_chips gave it */
int ui_chips_selected(lv_obj_t *row);
void ui_card_style(lv_obj_t *o);
void ui_toast(const char *text);

/* ---- the keypad and forms ----------------------------------------------------
 * A form is a column of fields beside (landscape) or above (portrait) a
 * number keypad. Inputs are typed on the keypad; outputs show results. The
 * tool's compute callback runs after every change. Inputs are saved per
 * tool and come back next time. */
typedef enum { F_NUM, F_UNSIGNED, F_INT, F_DATE, F_HEX, F_OUT } field_kind_t;
typedef enum { KP_NUM, KP_HEX } keypad_kind_t;

typedef struct form form_t;
typedef void (*form_cb_t)(form_t *f);

form_t *form_create(tool_page_t *tp, form_cb_t compute, keypad_kind_t kp);
void form_layout(form_t *f);                  /* after a rotation */
int form_input(form_t *f, const char *label, const char *unit, field_kind_t kind, const char *initial);
int form_output(form_t *f, const char *label);
void form_set_nosave(form_t *f, int i);                     /* the last input added: not saved */
void form_set_value_font(form_t *f, int i, int px, bool wrap);
void form_set_max_len(form_t *f, int i, int len);
/* A container in the field column for the tool's own widgets (chips...). */
lv_obj_t *form_extra(form_t *f);
lv_obj_t *form_fields(form_t *f);
/* Restore saved inputs, pick the first input and compute. Call once, after
 * adding the fields. */
void form_ready(form_t *f);
void form_recompute(form_t *f);
bool form_get(form_t *f, int i, double *out);
const char *form_text(form_t *f, int i);
void form_set_text(form_t *f, int i, const char *text);      /* an input's typed text */
void form_set_out(form_t *f, int i, const char *text);       /* an output's text */
void form_set_hint(form_t *f, int i, const char *text);      /* an empty input's computed value */
void form_set_label(form_t *f, int i, const char *text);
void form_set_unit(form_t *f, int i, const char *text);
void form_set_hidden(form_t *f, int i, bool hidden);
uint32_t form_seq(form_t *f, int i);                         /* when it was last typed in: 0 never */
void form_set_error(form_t *f, int i, bool error);
int form_active(form_t *f);                                 /* the selected input, or -1 */
bool form_is_date(form_t *f, int i);
void *form_user(form_t *f);
void form_set_user(form_t *f, void *user);
tool_page_t *form_page(form_t *f);
/* Ints kept with the form's saved inputs (chips, modes): 4 of them. */
int32_t form_int(form_t *f, int k, int32_t def);
void form_set_int(form_t *f, int k, int32_t v);

/* ---- tool openers ------------------------------------------------------------ */
void conv_open(tool_page_t *tp);
void base_open(tool_page_t *tp);
void pct_open(tool_page_t *tp);
void ratio_open(tool_page_t *tp);
void tip_open(tool_page_t *tp);
void discount_open(tool_page_t *tp);
void unitprice_open(tool_page_t *tp);
void random_open(tool_page_t *tp);
void password_open(tool_page_t *tp);
void ohm_open(tool_page_t *tp);
void resistor_open(tool_page_t *tp);
void led_open(tool_page_t *tp);
void divider_open(tool_page_t *tp);
void battery_open(tool_page_t *tp);
void datediff_open(tool_page_t *tp);
void dateadd_open(tool_page_t *tp);
void weekday_open(tool_page_t *tp);
void age_open(tool_page_t *tp);
void stopwatch_open(tool_page_t *tp);
void countdown_open(tool_page_t *tp);
void colour_open(tool_page_t *tp);
void ruler_open(tool_page_t *tp);
void level_open(tool_page_t *tp);
void protractor_open(tool_page_t *tp);
void sound_open(tool_page_t *tp);

/* Entropy for the random tools: touch timing and the clock. */
uint64_t ui_entropy(void);
