/* Toolbox: a digital Swiss Army knife. Many small tools, each on its own
 * page: unit converters, number and money helpers, electronics, dates and
 * timers, colour, and measuring with the Tab5's own screen, accelerometer
 * and microphone.
 *
 * The home screen lists categories (a rail on the left in landscape, a
 * strip on top in portrait) and the tools of the chosen one as tiles. A tool
 * opens as a page over the home screen, with a back arrow. The tool on
 * screen, the recently used tools, recent conversions and every tool's
 * inputs are saved in the app's data folder, so the app comes back as it was
 * left. The maths lives in engine/ and is tested on a computer
 * (tests/run.sh). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

#define SET_MAGIC 0x54424F58u     /* "TBOX" */
#define SET_VERSION 1

tb_settings_t g_set;

static struct {
    t5_app_t *app;
    lv_obj_t *root, *home, *body, *rail, *content;
    lv_obj_t *cat_items[CAT_COUNT];
    tool_page_t *tool;
    lv_timer_t *save_timer;
    char path[200];
} s;

/* ---- the tools ---------------------------------------------------------------- */
#define CONV(id, name, icon, uc) { "conv." id, name, icon, CAT_CONVERT, conv_open, uc }
const tool_def_t g_tools[] = {
    CONV("length", "Length", MD_ICON_SWAP_HORIZ, UC_LENGTH),
    CONV("mass", "Mass", MD_ICON_DOWNLOAD, UC_MASS),
    CONV("temp", "Temperature", MD_ICON_THERMOSTAT, UC_TEMP),
    CONV("volume", "Volume", MD_ICON_WATER_DROP, UC_VOLUME),
    CONV("area", "Area", MD_ICON_FULLSCREEN, UC_AREA),
    CONV("speed", "Speed", MD_ICON_SPEED, UC_SPEED),
    CONV("pressure", "Pressure", MD_ICON_AIR, UC_PRESSURE),
    CONV("energy", "Energy", MD_ICON_BOLT, UC_ENERGY),
    CONV("power", "Power", MD_ICON_POWER_SETTINGS_NEW, UC_POWER),
    CONV("data", "Data size", MD_ICON_STORAGE, UC_DATA),
    CONV("time", "Time", MD_ICON_SCHEDULE, UC_TIME),
    CONV("angle", "Angle", MD_ICON_EXPLORE, UC_ANGLE),
    CONV("fuel", "Fuel economy", MD_ICON_LOCATION_ON, UC_FUEL),
    CONV("cooking", "Cooking", MD_ICON_HOME, UC_COOKING),
    { "num.base", "Number bases", MD_ICON_CODE, CAT_NUMBERS, base_open, 0 },
    { "num.pct", "Percentage", MD_ICON_TUNE, CAT_NUMBERS, pct_open, 0 },
    { "num.ratio", "Ratio", MD_ICON_IMAGE, CAT_NUMBERS, ratio_open, 0 },
    { "num.random", "Random", MD_ICON_VIDEOGAME_ASSET, CAT_NUMBERS, random_open, 0 },
    { "num.password", "Password", MD_ICON_LOCK, CAT_NUMBERS, password_open, 0 },
    { "money.tip", "Tip and split", MD_ICON_SHARE, CAT_MONEY, tip_open, 0 },
    { "money.discount", "Discount", MD_ICON_SAVE, CAT_MONEY, discount_open, 0 },
    { "money.unit", "Unit price", MD_ICON_SEARCH, CAT_MONEY, unitprice_open, 0 },
    { "elec.ohm", "Ohm's law", MD_ICON_BOLT, CAT_ELEC, ohm_open, 0 },
    { "elec.resistor", "Resistor colours", MD_ICON_DEVELOPER_BOARD, CAT_ELEC, resistor_open, 0 },
    { "elec.led", "LED resistor", MD_ICON_BRIGHTNESS_HIGH, CAT_ELEC, led_open, 0 },
    { "elec.divider", "Voltage divider", MD_ICON_HUB, CAT_ELEC, divider_open, 0 },
    { "elec.battery", "Battery life", MD_ICON_BATTERY_STD, CAT_ELEC, battery_open, 0 },
    { "date.diff", "Days between", MD_ICON_CALENDAR_TODAY, CAT_TIME, datediff_open, 0 },
    { "date.add", "Add days", MD_ICON_ADD, CAT_TIME, dateadd_open, 0 },
    { "date.weekday", "Day of week", MD_ICON_HISTORY, CAT_TIME, weekday_open, 0 },
    { "date.age", "Age", MD_ICON_ACCOUNT_CIRCLE, CAT_TIME, age_open, 0 },
    { "time.stopwatch", "Stopwatch", MD_ICON_TIMER, CAT_TIME, stopwatch_open, 0 },
    { "time.countdown", "Timer", MD_ICON_ALARM, CAT_TIME, countdown_open, 0 },
    { "colour.convert", "Colour", MD_ICON_PALETTE, CAT_COLOUR, colour_open, 0 },
    { "measure.ruler", "Ruler", MD_ICON_DRAG_HANDLE, CAT_MEASURE, ruler_open, 0 },
    { "measure.level", "Level", MD_ICON_SENSORS, CAT_MEASURE, level_open, 0 },
    { "measure.protractor", "Protractor", MD_ICON_SCREEN_ROTATION, CAT_MEASURE, protractor_open, 0 },
    { "measure.sound", "Sound level", MD_ICON_GRAPHIC_EQ, CAT_MEASURE, sound_open, 0 },
};
const int g_tool_count = (int)(sizeof g_tools / sizeof g_tools[0]);

const tool_def_t *tool_find(const char *id)
{
    if (!id || !*id) return NULL;
    for (int i = 0; i < g_tool_count; i++)
        if (strcmp(g_tools[i].id, id) == 0) return &g_tools[i];
    return NULL;
}

const char *cat_name(tool_cat_t c)
{
    static const char *const n[CAT_COUNT] = { "Recent", "Convert", "Numbers", "Money", "Electronics",
                                              "Date & time", "Colour", "Measure" };
    return (unsigned)c < CAT_COUNT ? n[c] : "";
}

const char *cat_icon(tool_cat_t c)
{
    static const char *const n[CAT_COUNT] = { MD_ICON_HISTORY, MD_ICON_SWAP_HORIZ, MD_ICON_CODE, MD_ICON_SAVE,
                                              MD_ICON_BOLT, MD_ICON_SCHEDULE, MD_ICON_PALETTE, MD_ICON_SENSORS };
    return (unsigned)c < CAT_COUNT ? n[c] : MD_ICON_APPS;
}

bool ui_portrait(void)
{
    return lv_display_get_horizontal_resolution(NULL) < lv_display_get_vertical_resolution(NULL);
}

void ui_set_fullscreen(bool on)
{
    if (t5_shell_fullscreen() != on) T5_IGNORE(t5_shell_set_fullscreen(on));   /* cosmetic if it fails */
}

/* ---- settings ------------------------------------------------------------------ */
static void settings_defaults(void)
{
    memset(&g_set, 0, sizeof g_set);
    g_set.magic = SET_MAGIC;
    g_set.version = SET_VERSION;
    g_set.home_cat = CAT_CONVERT;
}

/* Strings from the file are ended where they must be, whatever it held. */
static void settings_sanitise(void)
{
#define TERM(a) ((a)[sizeof(a) - 1] = '\0')
    TERM(g_set.open_tool);
    for (int i = 0; i < TB_MRU; i++) TERM(g_set.mru[i]);
    for (int i = 0; i < TB_RECENT; i++) {
        TERM(g_set.recent[i].tool);
        TERM(g_set.recent[i].text);
        TERM(g_set.recent[i].value);
    }
    for (int i = 0; i < TB_SLOTS; i++) {
        TERM(g_set.slots[i].tool);
        for (int k = 0; k < TB_VALS; k++) TERM(g_set.slots[i].vals[k]);
    }
#undef TERM
    if (g_set.home_cat >= CAT_COUNT) g_set.home_cat = CAT_CONVERT;
}

static void settings_load(void)
{
    settings_defaults();
    FILE *f = fopen(s.path, "rb");
    if (!f) return;
    static tb_settings_t tmp;                      /* 10 KB: not on the UI task's stack */
    size_t n = fread(&tmp, 1, sizeof tmp, f);
    fclose(f);
    if (n != sizeof tmp || tmp.magic != SET_MAGIC || tmp.version != SET_VERSION) return;  /* start fresh */
    g_set = tmp;
    settings_sanitise();
}

void settings_save_now(void)
{
    if (s.save_timer) {
        lv_timer_delete(s.save_timer);
        s.save_timer = NULL;
    }
    if (!s.path[0]) return;
    t5_err_t e = t5_storage_write_atomic(s.path, &g_set, sizeof g_set);
    if (e != T5_OK) printf("toolbox: saving settings failed: %s\n", t5_err_str(e));
}

static void save_tick(lv_timer_t *t)
{
    (void)t;
    s.save_timer = NULL;                           /* a one-shot timer deletes itself */
    settings_save_now();
}

void settings_changed(void)
{
    if (s.save_timer) {
        lv_timer_reset(s.save_timer);
        return;
    }
    s.save_timer = lv_timer_create(save_tick, 1500, NULL);
    if (s.save_timer) lv_timer_set_repeat_count(s.save_timer, 1);
}

tb_slot_t *slot_get(const char *tool, bool create)
{
    for (int i = 0; i < TB_SLOTS; i++)
        if (strcmp(g_set.slots[i].tool, tool) == 0) return &g_set.slots[i];
    if (!create) return NULL;
    for (int i = 0; i < TB_SLOTS; i++) {
        if (!g_set.slots[i].tool[0]) {
            snprintf(g_set.slots[i].tool, TB_ID_LEN, "%s", tool);
            return &g_set.slots[i];
        }
    }
    /* Full (more tools than slots, from an older version): reuse the last. */
    tb_slot_t *sl = &g_set.slots[TB_SLOTS - 1];
    memset(sl, 0, sizeof *sl);
    snprintf(sl->tool, TB_ID_LEN, "%s", tool);
    return sl;
}

void recent_add(const char *tool, const char *text, int from, int to, const char *value)
{
    int at = TB_RECENT - 1;
    for (int i = 0; i < TB_RECENT; i++)              /* the same tool and units: replace it */
        if (strcmp(g_set.recent[i].tool, tool) == 0 && g_set.recent[i].from == from && g_set.recent[i].to == to) {
            at = i;
            break;
        }
    memmove(&g_set.recent[1], &g_set.recent[0], sizeof g_set.recent[0] * (size_t)at);
    tb_recent_t *r = &g_set.recent[0];
    memset(r, 0, sizeof *r);
    snprintf(r->tool, sizeof r->tool, "%s", tool);
    snprintf(r->text, sizeof r->text, "%s", text);
    snprintf(r->value, sizeof r->value, "%s", value);
    r->from = (uint8_t)from;
    r->to = (uint8_t)to;
    settings_changed();
}

static void mru_add(const char *id)
{
    int at = TB_MRU - 1;
    for (int i = 0; i < TB_MRU; i++)
        if (strcmp(g_set.mru[i], id) == 0) {
            at = i;
            break;
        }
    memmove(&g_set.mru[1], &g_set.mru[0], sizeof g_set.mru[0] * (size_t)at);
    snprintf(g_set.mru[0], TB_ID_LEN, "%s", id);
}

/* ---- tool pages ----------------------------------------------------------------- */
static void home_show(tool_cat_t c);

tool_page_t *tool_current(void) { return s.tool; }

static void tool_back(lv_event_t *e)
{
    (void)e;
    tool_close();
}

void tool_close(void)
{
    tool_page_t *tp = s.tool;
    if (!tp) return;
    s.tool = NULL;
    if (tp->on_close) tp->on_close(tp);
    lv_obj_delete(tp->page);                       /* the form, if any, frees itself */
    if (!tp->no_free) free(tp->data);
    free(tp);
    ui_set_fullscreen(false);
    g_set.open_tool[0] = '\0';
    settings_changed();
    if (g_set.home_cat == CAT_RECENT) home_show(CAT_RECENT);   /* show the new order */
}

void tool_open(const tool_def_t *def)
{
    if (!def) return;
    if (s.tool) tool_close();
    tool_page_t *tp = calloc(1, sizeof *tp);
    if (!tp) {
        ui_toast("Not enough memory");
        return;
    }
    tp->def = def;
    tp->page = md_page_create(s.root, def->name, tool_back, NULL, &tp->bar);
    lv_obj_set_floating(tp->page, true);
    lv_obj_set_pos(tp->page, 0, 0);
    tp->body = md_page_body(tp->page);
    s.tool = tp;
    def->open(tp);
    mru_add(def->id);
    snprintf(g_set.open_tool, TB_ID_LEN, "%s", def->id);
    settings_changed();
}

/* ---- home ------------------------------------------------------------------------ */
static void tile_clicked(lv_event_t *e)
{
    ui_entropy();
    tool_open(lv_event_get_user_data(e));
}

static lv_obj_t *tile_create(lv_obj_t *parent, const tool_def_t *d)
{
    const md_scheme_t *sc = md_scheme();
    lv_obj_t *t = ui_box(parent, LV_FLEX_FLOW_COLUMN);
    ui_card_style(t);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(t, T5_DP(10), 0);
    lv_obj_set_style_pad_row(t, T5_DP(6), 0);
    lv_obj_set_height(t, T5_DP(84));
    lv_obj_set_clickable(t, true);
    md_ripple_attach(t);
    lv_obj_add_event_cb(t, tile_clicked, LV_EVENT_CLICKED, (void *)d);
    lv_obj_t *i = md_icon_create(t, d->icon, 24);
    lv_obj_set_style_text_color(i, md_color(sc->c.secondary), 0);
    lv_obj_t *l = md_label_create(t, d->name, MD_TYPE_SUBTITLE1);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    return t;
}

static lv_obj_t *tile_grid(lv_obj_t *parent)
{
    static int32_t cols_l[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static int32_t cols_p[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static int32_t rows[] = { LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                              LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                              LV_GRID_TEMPLATE_LAST };
    lv_obj_t *g = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_scrollable(g, false);
    lv_obj_set_style_pad_gap(g, T5_DP(8), 0);
    lv_obj_set_grid_dsc_array(g, ui_portrait() ? cols_p : cols_l, rows);
    return g;
}

static void grid_place(lv_obj_t *g, lv_obj_t *t, int i)
{
    int cols = ui_portrait() ? 2 : 3;
    if (i / cols >= 10) {                       /* more rows than the template: drop it */
        lv_obj_set_hidden(t, true);
        return;
    }
    lv_obj_set_grid_cell(t, LV_GRID_ALIGN_STRETCH, i % cols, 1, LV_GRID_ALIGN_START, i / cols, 1);
    (void)g;
}

static void recent_clicked(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    const tool_def_t *d = tool_find(g_set.recent[i].tool);
    if (!d) return;
    /* The converter picks this up as it opens. */
    tb_slot_t *sl = slot_get(d->id, true);
    if (sl) {
        snprintf(sl->vals[0], TB_VAL_LEN, "%s", g_set.recent[i].value);
        sl->ints[0] = g_set.recent[i].from + 1;
        sl->ints[1] = g_set.recent[i].to + 1;
    }
    tool_open(d);
}

static void section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *l = md_label_create(parent, title, MD_TYPE_OVERLINE);
    lv_obj_set_style_text_opa(l, md_scheme()->on_surface.medium, 0);
    lv_obj_set_style_pad_top(l, T5_DP(4), 0);
}

static void show_recent(lv_obj_t *c)
{
    int n = 0;
    lv_obj_t *g = NULL;
    for (int i = 0; i < TB_MRU; i++) {
        const tool_def_t *d = tool_find(g_set.mru[i]);
        if (!d) continue;
        if (!g) {
            section(c, "Recently used");
            g = tile_grid(c);
        }
        grid_place(g, tile_create(g, d), n++);
    }
    bool any = false;
    for (int i = 0; i < TB_RECENT; i++) {
        if (!g_set.recent[i].tool[0] || !tool_find(g_set.recent[i].tool)) continue;
        if (!any) section(c, "Recent conversions");
        any = true;
        lv_obj_t *row = ui_box(c, LV_FLEX_FLOW_ROW);
        lv_obj_set_width(row, LV_PCT(100));
        ui_card_style(row);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor(row, T5_DP(12), 0);
        lv_obj_set_style_pad_ver(row, T5_DP(10), 0);
        lv_obj_set_style_pad_column(row, T5_DP(12), 0);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_clickable(row, true);
        md_ripple_attach(row);
        lv_obj_add_event_cb(row, recent_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        md_icon_create(row, tool_find(g_set.recent[i].tool)->icon, 24);
        lv_obj_t *l = ui_label(row, g_set.recent[i].text, 32, LV_OPA_COVER);
        lv_obj_set_flex_grow(l, 1);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    }
    if (!n && !any) {
        lv_obj_t *l = md_label_create(c, "Tools you use, and your recent conversions, show up here.", MD_TYPE_BODY1);
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_style_text_opa(l, md_scheme()->on_surface.medium, 0);
    }
}

static void cat_style(int i, bool sel)
{
    const md_scheme_t *sc = md_scheme();
    lv_obj_t *it = s.cat_items[i];
    lv_obj_set_style_bg_opa(it, sel ? 0x30 : LV_OPA_TRANSP, 0);
    md_rgb_t ink = sel ? sc->c.primary : sc->c.on_surface;
    uint32_t n = lv_obj_get_child_count(it);
    for (uint32_t k = 0; k < n; k++) {
        lv_obj_t *ch = lv_obj_get_child(it, (int32_t)k);
        lv_obj_set_style_text_color(ch, md_color(ink), 0);
        lv_obj_set_style_text_opa(ch, sel ? LV_OPA_COVER : sc->on_surface.medium, 0);
    }
}

static void home_show(tool_cat_t c)
{
    if ((unsigned)c >= CAT_COUNT) c = CAT_CONVERT;
    g_set.home_cat = (uint8_t)c;
    for (int i = 0; i < CAT_COUNT; i++) cat_style(i, i == (int)c);
    lv_obj_clean(s.content);
    lv_obj_scroll_to_y(s.content, 0, LV_ANIM_OFF);
    if (c == CAT_RECENT) {
        show_recent(s.content);
        return;
    }
    lv_obj_t *g = tile_grid(s.content);
    int n = 0;
    for (int i = 0; i < g_tool_count; i++)
        if (g_tools[i].cat == c) grid_place(g, tile_create(g, &g_tools[i]), n++);
}

static void cat_clicked(lv_event_t *e)
{
    tool_cat_t c = (tool_cat_t)(intptr_t)lv_event_get_user_data(e);
    ui_entropy();
    if (c == g_set.home_cat) return;
    home_show(c);
    settings_changed();
}

static void home_layout(void)
{
    bool p = ui_portrait();
    lv_obj_set_flex_flow(s.body, p ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_flow(s.rail, p ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s.rail, p ? LV_DIR_HOR : LV_DIR_VER);
    if (p) {
        lv_obj_set_size(s.rail, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_size(s.content, LV_PCT(100), LV_SIZE_CONTENT);
    } else {
        lv_obj_set_size(s.rail, T5_DP(172), LV_PCT(100));
        lv_obj_set_size(s.content, LV_SIZE_CONTENT, LV_PCT(100));
    }
    lv_obj_set_flex_grow(s.content, 1);
    for (int i = 0; i < CAT_COUNT; i++) {
        lv_obj_set_width(s.cat_items[i], p ? LV_SIZE_CONTENT : LV_PCT(100));
        lv_obj_set_flex_flow(s.cat_items[i], LV_FLEX_FLOW_ROW);
    }
    home_show((tool_cat_t)g_set.home_cat);           /* the tile grid's columns */
}

static void home_create(void)
{
    s.home = md_page_create(s.root, "Toolbox", NULL, NULL, NULL);
    s.body = md_page_body(s.home);
    lv_obj_set_scrollable(s.body, false);
    lv_obj_set_style_pad_bottom(s.body, 0, 0);

    s.rail = ui_box(s.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(s.rail, true);
    lv_obj_set_scrollbar_mode(s.rail, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(s.rail, T5_DP(8), 0);
    lv_obj_set_style_pad_gap(s.rail, T5_DP(2), 0);
    for (int i = 0; i < CAT_COUNT; i++) {
        lv_obj_t *it = ui_box(s.rail, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_hor(it, T5_DP(12), 0);
        lv_obj_set_style_pad_ver(it, T5_DP(10), 0);
        lv_obj_set_style_pad_column(it, T5_DP(12), 0);
        lv_obj_set_style_radius(it, T5_DP(24), 0);
        lv_obj_set_style_bg_color(it, md_color(md_scheme()->c.primary), 0);
        lv_obj_set_flex_align(it, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_clickable(it, true);
        md_ripple_attach(it);
        lv_obj_add_event_cb(it, cat_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        md_icon_create(it, cat_icon((tool_cat_t)i), 24);
        md_label_create(it, cat_name((tool_cat_t)i), MD_TYPE_SUBTITLE1);
        s.cat_items[i] = it;
    }
    s.content = ui_box(s.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(s.content, true);
    lv_obj_set_scroll_dir(s.content, LV_DIR_VER);
    lv_obj_set_style_pad_all(s.content, T5_DP(8), 0);
    lv_obj_set_style_pad_bottom(s.content, T5_DP(16), 0);
    lv_obj_set_style_pad_row(s.content, T5_DP(8), 0);
    home_layout();
}

/* ---- lifecycle -------------------------------------------------------------------- */
static void on_rotate(t5_app_t *app, int degrees)
{
    (void)app;
    (void)degrees;
    if (!s.home) return;
    home_layout();
    if (s.tool) {
        if (s.tool->form) form_layout(s.tool->form);
        if (s.tool->on_rotate) s.tool->on_rotate(s.tool);
    }
}

static void on_suspend(t5_app_t *app)
{
    (void)app;
    if (s.tool && s.tool->on_suspend) s.tool->on_suspend(s.tool);
    settings_save_now();
}

static void on_resume(t5_app_t *app)
{
    (void)app;
    if (s.tool && s.tool->on_resume) s.tool->on_resume(s.tool);
}

static void on_create(t5_app_t *app)
{
    s.app = app;
    s.root = t5_app_screen(app);
    const char *dir = t5_app_data_dir(app);
    if (dir) snprintf(s.path, sizeof s.path, "%s/settings.bin", dir);
    settings_load();
    ui_entropy();
    home_create();
    /* A tool's id as the launch argument opens it (`app run <id> conv.length`,
     * for tests; "home" for the home screen); otherwise come back to the
     * tool that was open. */
    const char *arg = t5_app_launch_arg(app);
    const tool_def_t *d = tool_find(arg && *arg ? arg : g_set.open_tool);
    if (arg && strcmp(arg, "home") == 0) d = NULL;
    if (d) tool_open(d);
}

T5_APP(.on_create = on_create, .on_resume = on_resume, .on_suspend = on_suspend, .on_rotate = on_rotate);
