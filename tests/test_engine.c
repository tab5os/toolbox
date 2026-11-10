/* Host tests for the engine: tests/run.sh builds and runs them with the
 * computer's own C compiler, with AddressSanitizer and UBSan. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../engine/colour.h"
#include "../engine/datecalc.h"
#include "../engine/elec.h"
#include "../engine/fmt.h"
#include "../engine/measure.h"
#include "../engine/numtools.h"
#include "../engine/units.h"

static int failures, checks;

#define CHECK(cond)                                                                \
    do {                                                                           \
        checks++;                                                                  \
        if (!(cond)) {                                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
            failures++;                                                            \
        }                                                                          \
    } while (0)
#define NEAR(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) <= (tol))
#define REL(a, b, rel) CHECK(fabs((double)(a) - (double)(b)) <= (rel) * fabs((double)(b)))
#define STREQ(a, b)                                                                \
    do {                                                                           \
        checks++;                                                                  \
        if (strcmp((a), (b)) != 0) {                                               \
            printf("FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
            failures++;                                                            \
        }                                                                          \
    } while (0)

static void test_fmt(void)
{
    char b[64];
    fmt_num(0.1 + 0.2, 10, false, b, sizeof b); STREQ(b, "0.3");
    fmt_num(1234567.5, 10, true, b, sizeof b); STREQ(b, "1,234,567.5");
    fmt_num(-1234.5, 10, true, b, sizeof b); STREQ(b, "-1,234.5");
    fmt_num(999, 10, true, b, sizeof b); STREQ(b, "999");
    fmt_num(1000, 10, true, b, sizeof b); STREQ(b, "1,000");
    fmt_num(30.48, 10, false, b, sizeof b); STREQ(b, "30.48");
    fmt_num(2.0 / 3.0, 6, false, b, sizeof b); STREQ(b, "0.666667");
    fmt_num(1.5e-9, 10, false, b, sizeof b); STREQ(b, "1.5e-9");
    fmt_num(6.02214076e23, 6, false, b, sizeof b); STREQ(b, "6.02214e23");
    fmt_num(0.000012, 10, false, b, sizeof b); STREQ(b, "0.000012");
    fmt_num(0, 10, false, b, sizeof b); STREQ(b, "0");
    fmt_num(-0.0, 10, false, b, sizeof b); STREQ(b, "0");
    fmt_num(-1e-20 * 0, 10, false, b, sizeof b); STREQ(b, "0");
    CHECK(!fmt_num(NAN, 10, false, b, sizeof b)); STREQ(b, "-");
    CHECK(!fmt_num(INFINITY, 10, false, b, sizeof b));
    fmt_num(99999.99999, 6, true, b, sizeof b); STREQ(b, "100,000");
    /* A tiny buffer truncates rather than overflowing. */
    char t[5];
    fmt_num(123456.0, 10, true, t, sizeof t); CHECK(strlen(t) == 4);

    fmt_si(4700, 3, "\xCE\xA9", b, sizeof b); STREQ(b, "4.7 k\xCE\xA9");
    fmt_si(0.0015, 3, "A", b, sizeof b); STREQ(b, "1.5 mA");
    fmt_si(220e-6, 3, "F", b, sizeof b); STREQ(b, "220 \xC2\xB5" "F");
    fmt_si(999999, 3, "W", b, sizeof b); STREQ(b, "1 MW");
    fmt_si(0, 3, "V", b, sizeof b); STREQ(b, "0 V");
    fmt_si(12, 3, "V", b, sizeof b); STREQ(b, "12 V");
    fmt_si(-3.3e-3, 3, "V", b, sizeof b); STREQ(b, "-3.3 mV");

    fmt_duration(12.34, b, sizeof b); STREQ(b, "12.3 s");
    fmt_duration(245, b, sizeof b); STREQ(b, "4 min 05 s");
    fmt_duration(5000, b, sizeof b); STREQ(b, "1 h 23 min");
    fmt_duration(3 * 86400 + 4 * 3600 + 50, b, sizeof b); STREQ(b, "3 d 4 h");
    fmt_clock(12345, b, sizeof b); STREQ(b, "02:03.45");
    fmt_clock(372345, b, sizeof b); STREQ(b, "1:02:03.45");
}

static void test_entry(void)
{
    entry_t e;
    double v;
    entry_init(&e, true, true, false);
    CHECK(!entry_value(&e, &v));
    entry_key(&e, ENTRY_DIGIT, '0');
    CHECK(!entry_key(&e, ENTRY_DIGIT, '0'));              /* no "00" */
    entry_key(&e, ENTRY_DIGIT, '5');
    STREQ(e.text, "5");
    entry_key(&e, ENTRY_POINT, 0);
    CHECK(!entry_key(&e, ENTRY_POINT, 0));                /* one point */
    entry_key(&e, ENTRY_DIGIT, '2');
    STREQ(e.text, "5.2");
    entry_key(&e, ENTRY_SIGN, 0);
    STREQ(e.text, "-5.2");
    CHECK(entry_value(&e, &v) && v == -5.2);
    entry_key(&e, ENTRY_SIGN, 0);
    STREQ(e.text, "5.2");
    entry_key(&e, ENTRY_BACK, 0);
    entry_key(&e, ENTRY_BACK, 0);
    STREQ(e.text, "5");
    entry_key(&e, ENTRY_CLEAR, 0);
    STREQ(e.text, "");
    entry_key(&e, ENTRY_POINT, 0);
    STREQ(e.text, "0.");
    CHECK(entry_value(&e, &v) && v == 0);
    entry_key(&e, ENTRY_CLEAR, 0);
    entry_key(&e, ENTRY_SIGN, 0);
    STREQ(e.text, "-");
    CHECK(!entry_value(&e, &v));
    entry_key(&e, ENTRY_POINT, 0);
    STREQ(e.text, "-0.");
    CHECK(!entry_key(&e, ENTRY_DIGIT, 'A'));              /* not hex */
    /* Length limit. */
    entry_init(&e, false, false, false);
    e.max_len = 3;
    for (int i = 0; i < 6; i++) entry_key(&e, ENTRY_DIGIT, '7');
    STREQ(e.text, "777");
    CHECK(!entry_key(&e, ENTRY_SIGN, 0));
    CHECK(!entry_key(&e, ENTRY_POINT, 0));
    /* The default limit stops at ENTRY_MAX - 1 digits. */
    entry_init(&e, true, true, true);
    for (int i = 0; i < 40; i++) entry_key(&e, ENTRY_DIGIT, 'F');
    CHECK(strlen(e.text) == ENTRY_MAX - 1);
    entry_key(&e, ENTRY_SIGN, 0);
    CHECK(strlen(e.text) == ENTRY_MAX);
    CHECK(!parse_num("1.2.3", &v));
    CHECK(!parse_num("", &v));
    CHECK(parse_num("1e3", &v) && v == 1000);
}

static double conv(units_cat_t c, const char *from, const char *to, double v)
{
    double out;
    int f = units_find(c, from), t = units_find(c, to);
    if (f < 0 || t < 0) {
        printf("no unit %s or %s\n", from, to);
        failures++;
        return NAN;
    }
    if (!units_convert(c, f, t, v, &out)) return NAN;
    return out;
}

static void test_units(void)
{
    REL(conv(UC_LENGTH, "in", "cm", 12), 30.48, 1e-12);
    REL(conv(UC_LENGTH, "mi", "km", 1), 1.609344, 1e-12);
    REL(conv(UC_LENGTH, "ft", "m", 1), 0.3048, 1e-12);
    REL(conv(UC_LENGTH, "nmi", "m", 1), 1852, 1e-12);
    REL(conv(UC_MASS, "lb", "kg", 1), 0.45359237, 1e-12);
    REL(conv(UC_MASS, "st", "lb", 1), 14, 1e-9);
    REL(conv(UC_MASS, "oz", "g", 16), 453.59237, 1e-12);
    NEAR(conv(UC_TEMP, "\xC2\xB0" "C", "\xC2\xB0" "F", 100), 212, 1e-9);
    NEAR(conv(UC_TEMP, "\xC2\xB0" "F", "\xC2\xB0" "C", -40), -40, 1e-9);
    NEAR(conv(UC_TEMP, "\xC2\xB0" "C", "K", 0), 273.15, 1e-9);
    NEAR(conv(UC_TEMP, "\xC2\xB0" "F", "\xC2\xB0" "R", 0), 459.67, 1e-9);
    NEAR(conv(UC_TEMP, "K", "\xC2\xB0" "F", 0), -459.67, 1e-9);
    CHECK(isnan(conv(UC_TEMP, "K", "\xC2\xB0" "C", -1)));       /* below absolute zero */
    REL(conv(UC_VOLUME, "gal (US)", "L", 1), 3.785411784, 1e-12);
    REL(conv(UC_VOLUME, "gal (UK)", "L", 1), 4.54609, 1e-12);
    REL(conv(UC_VOLUME, "cup (US)", "fl oz (US)", 1), 8, 1e-12);
    REL(conv(UC_VOLUME, "gal (US)", "in\xC2\xB3", 1), 231, 1e-12);
    REL(conv(UC_AREA, "ac", "ft\xC2\xB2", 1), 43560, 1e-12);
    REL(conv(UC_AREA, "ha", "ac", 1), 2.4710538146716, 1e-12);
    REL(conv(UC_AREA, "mi\xC2\xB2", "ac", 1), 640, 1e-12);
    REL(conv(UC_SPEED, "km/h", "m/s", 36), 10, 1e-12);
    REL(conv(UC_SPEED, "mph", "km/h", 60), 96.56064, 1e-12);
    REL(conv(UC_SPEED, "kn", "km/h", 1), 1.852, 1e-12);
    REL(conv(UC_PRESSURE, "atm", "psi", 1), 14.6959487755, 1e-9);
    REL(conv(UC_PRESSURE, "atm", "mmHg", 1), 760, 1e-6);
    REL(conv(UC_PRESSURE, "bar", "kPa", 1), 100, 1e-12);
    REL(conv(UC_PRESSURE, "Torr", "Pa", 760), 101325, 1e-12);
    REL(conv(UC_ENERGY, "kWh", "MJ", 1), 3.6, 1e-12);
    REL(conv(UC_ENERGY, "kcal", "kJ", 1), 4.184, 1e-12);
    REL(conv(UC_ENERGY, "BTU", "J", 1), 1055.05585262, 1e-12);
    REL(conv(UC_POWER, "hp", "W", 1), 745.69987158, 1e-9);
    REL(conv(UC_POWER, "PS", "W", 1), 735.49875, 1e-12);
    NEAR(conv(UC_POWER, "dBm", "mW", 0), 1, 1e-12);
    NEAR(conv(UC_POWER, "dBm", "W", 30), 1, 1e-12);
    NEAR(conv(UC_POWER, "W", "dBm", 0.1), 20, 1e-9);
    NEAR(conv(UC_POWER, "mW", "dBm", 0.001), -30, 1e-9);
    CHECK(isnan(conv(UC_POWER, "W", "dBm", 0)));
    REL(conv(UC_DATA, "GiB", "MiB", 1), 1024, 1e-12);
    REL(conv(UC_DATA, "GB", "GiB", 1), 0.93132257461548, 1e-12);
    REL(conv(UC_DATA, "Mbit", "kB", 8), 1000, 1e-12);
    REL(conv(UC_DATA, "B", "bit", 1), 8, 1e-12);
    REL(conv(UC_TIME, "d", "h", 1), 24, 1e-12);
    REL(conv(UC_TIME, "yr", "d", 1), 365.2425, 1e-12);
    REL(conv(UC_TIME, "wk", "min", 1), 10080, 1e-12);
    REL(conv(UC_ANGLE, "rad", "\xC2\xB0", 3.14159265358979323846), 180, 1e-12);
    REL(conv(UC_ANGLE, "turn", "grad", 1), 400, 1e-12);
    REL(conv(UC_ANGLE, "\xC2\xB0", "'", 1), 60, 1e-12);
    REL(conv(UC_ANGLE, "mil", "\xC2\xB0", 6400), 360, 1e-12);
    REL(conv(UC_FUEL, "km/L", "L/100km", 20), 5, 1e-12);
    REL(conv(UC_FUEL, "mpg (US)", "L/100km", 30), 7.84048611, 1e-8);
    REL(conv(UC_FUEL, "mpg (UK)", "mpg (US)", 1), 0.832674185, 1e-8);
    REL(conv(UC_FUEL, "L/100km", "km/L", 8), 12.5, 1e-12);
    CHECK(isnan(conv(UC_FUEL, "km/L", "L/100km", 0)));
    REL(conv(UC_COOKING, "cup", "tbsp", 1), 16, 1e-12);
    REL(conv(UC_COOKING, "tbsp", "tsp", 1), 3, 1e-12);
    REL(conv(UC_COOKING, "cup (metric)", "mL", 1), 250, 1e-12);
    /* Every unit round-trips through every other. */
    for (int c = 0; c < UC_COUNT; c++) {
        const unit_cat_t *cat = units_cat((units_cat_t)c);
        CHECK(cat && cat->count > 1 && cat->count <= 16);
        CHECK(cat->def_from < cat->count && cat->def_to < cat->count);
        for (int i = 0; i < cat->count; i++)
            for (int j = 0; j < cat->count; j++) {
                double a, b;
                double v = c == UC_TEMP ? 300 : 7.25;
                bool ok = units_convert((units_cat_t)c, i, j, v, &a) && units_convert((units_cat_t)c, j, i, a, &b);
                CHECK(ok);
                if (ok) REL(b, v, 1e-9);
            }
    }
    double out;
    CHECK(!units_convert(UC_COUNT, 0, 0, 1, &out));
    CHECK(!units_convert(UC_LENGTH, 0, 99, 1, &out));
    CHECK(!units_convert(UC_LENGTH, 0, 1, NAN, &out));
}

static void test_bases(void)
{
    uint64_t v;
    char b[96];
    CHECK(nt_parse_base("FF", 16, &v) && v == 255);
    CHECK(nt_parse_base("ff", 16, &v) && v == 255);
    CHECK(nt_parse_base("1010 0101", 2, &v) && v == 0xA5);
    CHECK(nt_parse_base("777", 8, &v) && v == 511);
    CHECK(nt_parse_base("18446744073709551615", 10, &v) && v == UINT64_MAX);
    CHECK(!nt_parse_base("18446744073709551616", 10, &v));
    CHECK(!nt_parse_base("12", 2, &v));
    CHECK(!nt_parse_base("", 10, &v));
    CHECK(!nt_parse_base("8", 8, &v));
    CHECK(!nt_parse_base("1", 7, &v));
    nt_format_base(0, 2, true, b, sizeof b); STREQ(b, "0");
    nt_format_base(0xA5, 2, true, b, sizeof b); STREQ(b, "1010 0101");
    nt_format_base(0x1A5, 2, true, b, sizeof b); STREQ(b, "1 1010 0101");
    nt_format_base(0xDEADBEEF, 16, true, b, sizeof b); STREQ(b, "DEAD BEEF");
    nt_format_base(0xDEADBEEF, 16, false, b, sizeof b); STREQ(b, "DEADBEEF");
    nt_format_base(1234567, 10, true, b, sizeof b); STREQ(b, "1,234,567");
    nt_format_base(UINT64_MAX, 2, true, b, sizeof b); CHECK(strlen(b) == 64 + 15);
    nt_format_base(8, 8, false, b, sizeof b); STREQ(b, "10");
    CHECK(nt_signed(0xFF, 8) == -1);
    CHECK(nt_signed(0x7F, 8) == 127);
    CHECK(nt_signed(0x8000, 16) == -32768);
    CHECK(nt_signed(UINT64_MAX, 64) == -1);
    CHECK(nt_mask(8) == 0xFF && nt_mask(64) == UINT64_MAX);
}

static void test_percent_ratio(void)
{
    double v, a, b;
    CHECK(nt_pct_of(15, 80, &v) && v == 12);
    CHECK(nt_pct_ratio(12, 80, &v) && v == 15);
    CHECK(!nt_pct_ratio(1, 0, &v));
    CHECK(nt_pct_change(80, 100, &v) && v == 25);
    CHECK(nt_pct_change(-50, -25, &v) && v == 50);
    CHECK(!nt_pct_change(0, 5, &v));
    CHECK(nt_pct_add(200, -10, &v) && v == 180);
    CHECK(nt_gcd(1920, 1080) == 120);
    CHECK(nt_ratio_simplify(1920, 1080, &a, &b) && a == 16 && b == 9);
    CHECK(nt_ratio_simplify(1.5, 2, &a, &b) && a == 3 && b == 4);
    CHECK(nt_ratio_simplify(2.35, 1, &a, &b) && a == 47 && b == 20);
    CHECK(!nt_ratio_simplify(0, 5, &a, &b));
    CHECK(!nt_ratio_simplify(-1, 5, &a, &b));
    CHECK(nt_ratio_scale(16, 9, 1280, &v) && v == 720);
    CHECK(!nt_ratio_scale(0, 9, 1280, &v));
}

static void test_money(void)
{
    nt_tip_t t;
    CHECK(nt_tip(100, 15, 4, false, &t));
    NEAR(t.tip, 15, 1e-9); NEAR(t.total, 115, 1e-9); NEAR(t.per_person, 28.75, 1e-9); NEAR(t.tip_per_person, 3.75, 1e-9);
    CHECK(nt_tip(100, 15, 4, true, &t));
    NEAR(t.per_person, 29, 1e-9); NEAR(t.total, 116, 1e-9); NEAR(t.tip, 16, 1e-9);
    CHECK(nt_tip(100, 20, 4, true, &t));                 /* already whole: unchanged */
    NEAR(t.per_person, 30, 1e-9);
    CHECK(!nt_tip(100, 15, 0, false, &t));
    CHECK(!nt_tip(-1, 15, 1, false, &t));
    nt_discount_t d;
    CHECK(nt_discount(80, 25, 0, 0, &d));
    NEAR(d.final, 60, 1e-9); NEAR(d.saved, 20, 1e-9); NEAR(d.with_tax, 60, 1e-9);
    CHECK(nt_discount(100, 20, 10, 8, &d));
    NEAR(d.final, 72, 1e-9); NEAR(d.saved, 28, 1e-9); NEAR(d.with_tax, 77.76, 1e-9);
    nt_unitprice_t u;
    CHECK(nt_unit_price(3, 500, 5, 1000, &u));
    NEAR(u.unit[0], 0.006, 1e-12); NEAR(u.unit[1], 0.005, 1e-12);
    CHECK(u.best == 1); NEAR(u.saving_pct, 100.0 / 6.0, 1e-9);
    CHECK(nt_unit_price(2, 1, 4, 2, &u) && u.best == -1);
    CHECK(!nt_unit_price(2, 0, 4, 2, &u));
}

static void test_random(void)
{
    nt_rng_t r, r2;
    nt_rng_seed(&r, 42);
    nt_rng_seed(&r2, 42);
    CHECK(nt_rng_next(&r) == nt_rng_next(&r2));
    nt_rng_mix(&r2, 1);
    CHECK(nt_rng_next(&r) != nt_rng_next(&r2));
    /* Range: inside, both ends reached, fair-ish. */
    int counts[6] = { 0 };
    for (int i = 0; i < 60000; i++) {
        int64_t x = nt_rng_range(&r, 1, 6);
        CHECK(x >= 1 && x <= 6);
        if (x >= 1 && x <= 6) counts[x - 1]++;
    }
    for (int i = 0; i < 6; i++) CHECK(counts[i] > 9400 && counts[i] < 10600);
    CHECK(nt_rng_range(&r, 5, 5) == 5);
    int64_t x = nt_rng_range(&r, 10, -10);
    CHECK(x >= -10 && x <= 10);
    (void)nt_rng_range(&r, INT64_MIN, INT64_MAX);
    /* Passwords. */
    char pw[NT_PW_MAX + 1];
    double bits = nt_password(&r, 16, NT_PW_LOWER | NT_PW_UPPER | NT_PW_DIGITS | NT_PW_SYMBOLS, pw, sizeof pw);
    CHECK(strlen(pw) == 16);
    CHECK(bits > 16 * 6.0);
    for (int k = 0; k < 200; k++) {
        nt_password(&r, 8, NT_PW_LOWER | NT_PW_DIGITS | NT_PW_NO_AMBIGUOUS, pw, sizeof pw);
        bool lower = false, digit = false, bad = false;
        for (char *c = pw; *c; c++) {
            if (*c >= 'a' && *c <= 'z') lower = true;
            else if (*c >= '0' && *c <= '9') digit = true;
            else bad = true;
            if (strchr("0Oo1lI|", *c)) bad = true;
        }
        CHECK(lower && digit && !bad && strlen(pw) == 8);
    }
    CHECK(nt_password(&r, 10, 0, pw, sizeof pw) == 0);
    CHECK(nt_password(&r, 10, NT_PW_LOWER, pw, 5) == 0);
    nt_password(&r, 200, NT_PW_DIGITS, pw, sizeof pw);
    CHECK(strlen(pw) == NT_PW_MAX);
    nt_password(&r, 1, NT_PW_DIGITS, pw, sizeof pw);
    CHECK(strlen(pw) == 4);
}

static void test_elec(void)
{
    double x[4];
    x[OHM_V] = 12; x[OHM_I] = 0.5;
    CHECK(elec_ohm(x, (1u << OHM_V) | (1u << OHM_I)));
    NEAR(x[OHM_R], 24, 1e-12); NEAR(x[OHM_P], 6, 1e-12);
    x[OHM_R] = 100; x[OHM_P] = 0.25;
    CHECK(elec_ohm(x, (1u << OHM_R) | (1u << OHM_P)));
    NEAR(x[OHM_V], 5, 1e-12); NEAR(x[OHM_I], 0.05, 1e-12);
    x[OHM_V] = 5; x[OHM_P] = 2.5;
    CHECK(elec_ohm(x, (1u << OHM_V) | (1u << OHM_P)));
    NEAR(x[OHM_I], 0.5, 1e-12); NEAR(x[OHM_R], 10, 1e-12);
    x[OHM_I] = 2; x[OHM_P] = 8;
    CHECK(elec_ohm(x, (1u << OHM_I) | (1u << OHM_P)));
    NEAR(x[OHM_V], 4, 1e-12); NEAR(x[OHM_R], 2, 1e-12);
    x[OHM_I] = 0.1; x[OHM_R] = 47;
    CHECK(elec_ohm(x, (1u << OHM_I) | (1u << OHM_R)));
    NEAR(x[OHM_V], 4.7, 1e-12); NEAR(x[OHM_P], 0.47, 1e-12);
    x[OHM_V] = 3; x[OHM_R] = 0;
    CHECK(!elec_ohm(x, (1u << OHM_V) | (1u << OHM_R)));
    CHECK(!elec_ohm(x, 1u << OHM_V));
    CHECK(!elec_ohm(x, 0x7));

    elec_resistor_t r;
    int b4[4] = { RC_YELLOW, RC_VIOLET, RC_RED, RC_GOLD };
    CHECK(elec_bands_decode(b4, 4, &r)); NEAR(r.ohms, 4700, 1e-9); NEAR(r.tol_pct, 5, 0);
    int b5[5] = { RC_BROWN, RC_BLACK, RC_BLACK, RC_BROWN, RC_BROWN };
    CHECK(elec_bands_decode(b5, 5, &r)); NEAR(r.ohms, 1000, 1e-9); NEAR(r.tol_pct, 1, 0);
    int b6[6] = { RC_RED, RC_RED, RC_BLACK, RC_GOLD, RC_GREEN, RC_RED };
    CHECK(elec_bands_decode(b6, 6, &r)); NEAR(r.ohms, 22, 1e-9); NEAR(r.tol_pct, 0.5, 0); CHECK(r.tempco_ppm == 50);
    int bad[4] = { RC_BLACK, RC_VIOLET, RC_RED, RC_GOLD };
    CHECK(!elec_bands_decode(bad, 4, &r));
    int bad2[4] = { RC_RED, RC_GOLD, RC_RED, RC_GOLD };
    CHECK(!elec_bands_decode(bad2, 4, &r));
    int bad3[4] = { RC_RED, RC_RED, RC_RED, RC_WHITE };   /* white isn't a tolerance */
    CHECK(!elec_bands_decode(bad3, 4, &r));
    int e[6];
    CHECK(elec_bands_encode(4700, 4, e) && e[0] == RC_YELLOW && e[1] == RC_VIOLET && e[2] == RC_RED && e[3] == RC_GOLD);
    CHECK(elec_bands_encode(0.47, 4, e) && e[0] == RC_YELLOW && e[1] == RC_VIOLET && e[2] == RC_SILVER);
    CHECK(elec_bands_encode(10, 4, e) && e[0] == RC_BROWN && e[1] == RC_BLACK && e[2] == RC_BLACK);
    CHECK(elec_bands_encode(1, 4, e) && e[0] == RC_BROWN && e[1] == RC_BLACK && e[2] == RC_GOLD);
    CHECK(elec_bands_encode(4990, 5, e) && e[0] == RC_YELLOW && e[1] == RC_WHITE && e[2] == RC_WHITE &&
          e[3] == RC_BROWN && e[4] == RC_BROWN);
    CHECK(!elec_bands_encode(4990, 4, e));                /* needs three digits */
    CHECK(!elec_bands_encode(0.01, 4, e));
    CHECK(!elec_bands_encode(1e12, 4, e));
    /* Every 4-band and 5-band value round-trips. */
    for (int d1 = 1; d1 <= 9; d1++)
        for (int d2 = 0; d2 <= 9; d2++)
            for (int m = 0; m <= RC_SILVER; m++) {
                if (m == RC_GREY || m == RC_WHITE) continue;   /* past 99.9 G */
                int in[4] = { d1, d2, m, RC_GOLD }, out[4];
                CHECK(elec_bands_decode(in, 4, &r));
                CHECK(elec_bands_encode(r.ohms, 4, out) && out[0] == d1 && out[1] == d2 && out[2] == m);
            }
    for (int c = 0; c < RC_COUNT; c++) CHECK(elec_colour_name(c)[0] != '?');

    NEAR(elec_e_nearest(12, 5000, false), 4700, 1e-9);
    NEAR(elec_e_nearest(12, 5000, true), 5600, 1e-9);
    NEAR(elec_e_nearest(24, 5000, false), 5100, 1e-9);
    NEAR(elec_e_nearest(12, 9500, false), 10000, 1e-9);
    NEAR(elec_e_nearest(12, 9500, true), 10000, 1e-9);
    NEAR(elec_e_nearest(12, 4700, true), 4700, 1e-9);
    NEAR(elec_e_nearest(96, 1000, false), 1000, 1e-9);
    NEAR(elec_e_nearest(96, 4990, false), 4990, 1e-9);
    NEAR(elec_e_nearest(48, 4990, false), 5110, 1e-9);
    NEAR(elec_e_nearest(96, 0.33, false), 0.332, 1e-12);
    CHECK(isnan(elec_e_nearest(12, 0, false)));

    elec_led_t l;
    CHECK(elec_led(5, 2, 0.02, 1, &l));
    NEAR(l.r_exact, 150, 1e-9); NEAR(l.r_e12, 150, 1e-9); NEAR(l.p_resistor, 0.06, 1e-9);
    CHECK(elec_led(12, 3.2, 0.02, 3, &l));
    NEAR(l.r_exact, 120, 1e-9);
    CHECK(elec_led(9, 2, 0.02, 1, &l));
    NEAR(l.r_exact, 350, 1e-9); NEAR(l.r_e12, 390, 1e-9);
    CHECK(!elec_led(3, 3.2, 0.02, 1, &l));

    double d[4] = { 5, 10000, 10000, 0 };
    CHECK(elec_divider(d, 0x7)); NEAR(d[DIV_VOUT], 2.5, 1e-12);
    double d2[4] = { 12, 0, 10000, 3.3 };
    CHECK(elec_divider(d2, (1u << DIV_VIN) | (1u << DIV_R2) | (1u << DIV_VOUT)));
    NEAR(d2[DIV_R1], 26363.636363636, 1e-6);
    double d3[4] = { 12, 10000, 0, 3.3 };
    CHECK(elec_divider(d3, (1u << DIV_VIN) | (1u << DIV_R1) | (1u << DIV_VOUT)));
    NEAR(d3[DIV_R2], 3793.103448276, 1e-6);
    double d4[4] = { 0, 1000, 2000, 2 };
    CHECK(elec_divider(d4, 0xE)); NEAR(d4[DIV_VIN], 3, 1e-12);
    double d5[4] = { 3, 0, 1000, 5 };
    CHECK(!elec_divider(d5, 0xD));                      /* Vout above Vin */
    CHECK(!elec_divider(d5, 0x3));

    double h;
    CHECK(elec_battery_hours(2000, 100, 0.8, &h)); NEAR(h, 16, 1e-12);
    CHECK(!elec_battery_hours(2000, 0, 0.8, &h));
    CHECK(!elec_battery_hours(2000, 10, 1.5, &h));
}

static void test_dates(void)
{
    char b[64];
    CHECK(dc_to_days((dc_date_t){ 1970, 1, 1 }) == 0);
    CHECK(dc_to_days((dc_date_t){ 2000, 3, 1 }) == 11017);
    CHECK(dc_weekday((dc_date_t){ 1970, 1, 1 }) == 3);          /* Thursday */
    CHECK(dc_weekday((dc_date_t){ 2026, 11, 10 }) == 1);        /* Tuesday */
    CHECK(dc_weekday((dc_date_t){ 1, 1, 1 }) == 0);             /* Monday */
    for (int64_t z = -800000; z < 2900000; z += 997) {
        dc_date_t d = dc_from_days(z);
        CHECK(dc_valid(d) || d.y < 1);
        CHECK(dc_to_days(d) == z);
    }
    CHECK(dc_leap(2000) && dc_leap(2024) && !dc_leap(1900) && !dc_leap(2026));
    CHECK(!dc_valid((dc_date_t){ 2026, 2, 29 }));
    CHECK(dc_valid((dc_date_t){ 2024, 2, 29 }));
    CHECK(!dc_valid((dc_date_t){ 2026, 13, 1 }));
    dc_date_t d;
    CHECK(dc_parse("20261110", &d) && d.y == 2026 && d.m == 11 && d.d == 10);
    CHECK(dc_parse("2026-11-10", &d));
    CHECK(!dc_parse("2026111", &d));
    CHECK(!dc_parse("20260230", &d));
    CHECK(!dc_parse("2026x110", &d));
    dc_format((dc_date_t){ 2026, 1, 5 }, b, sizeof b); STREQ(b, "2026-01-05");
    dc_format_long((dc_date_t){ 2026, 11, 10 }, b, sizeof b); STREQ(b, "Tuesday 10 November 2026");
    CHECK(dc_day_of_year((dc_date_t){ 2024, 12, 31 }) == 366);
    int iy;
    CHECK(dc_iso_week((dc_date_t){ 2026, 1, 1 }, &iy) == 1 && iy == 2026);
    CHECK(dc_iso_week((dc_date_t){ 2021, 1, 3 }, &iy) == 53 && iy == 2020);
    CHECK(dc_iso_week((dc_date_t){ 2024, 12, 30 }, &iy) == 1 && iy == 2025);
    CHECK(dc_iso_week((dc_date_t){ 2026, 11, 10 }, &iy) == 46);
    int y, m, dd;
    CHECK(dc_age((dc_date_t){ 1990, 5, 15 }, (dc_date_t){ 2026, 11, 10 }, &y, &m, &dd));
    CHECK(y == 36 && m == 5 && dd == 26);
    CHECK(dc_age((dc_date_t){ 2000, 2, 29 }, (dc_date_t){ 2001, 2, 28 }, &y, &m, &dd) && y == 1 && m == 0 && dd == 0);
    CHECK(dc_age((dc_date_t){ 2000, 2, 29 }, (dc_date_t){ 2001, 2, 27 }, &y, &m, &dd) && y == 0 && m == 11 && dd == 29);
    CHECK(dc_age((dc_date_t){ 2026, 1, 31 }, (dc_date_t){ 2026, 3, 1 }, &y, &m, &dd) && y == 0 && m == 1 && dd == 1);
    CHECK(dc_age((dc_date_t){ 2026, 1, 1 }, (dc_date_t){ 2026, 1, 1 }, &y, &m, &dd) && y == 0 && m == 0 && dd == 0);
    CHECK(!dc_age((dc_date_t){ 2026, 1, 2 }, (dc_date_t){ 2026, 1, 1 }, &y, &m, &dd));
    CHECK(dc_workdays((dc_date_t){ 2026, 11, 9 }, (dc_date_t){ 2026, 11, 16 }) == 5);   /* Mon to Mon */
    CHECK(dc_workdays((dc_date_t){ 2026, 11, 14 }, (dc_date_t){ 2026, 11, 16 }) == 0);  /* the weekend */
    CHECK(dc_workdays((dc_date_t){ 2026, 11, 16 }, (dc_date_t){ 2026, 11, 9 }) == -5);
    CHECK(dc_workdays((dc_date_t){ 2026, 1, 1 }, (dc_date_t){ 2027, 1, 1 }) == 261);

    sw_t s;
    sw_reset(&s);
    sw_start(&s, 1000);
    CHECK(sw_elapsed(&s, 5000) == 4000);
    CHECK(sw_lap(&s, 6000) && s.laps[0] == 5000);
    sw_stop(&s, 8000);
    CHECK(!sw_lap(&s, 9000));
    CHECK(sw_elapsed(&s, 99999) == 7000);
    sw_start(&s, 100000);
    CHECK(sw_elapsed(&s, 100500) == 7500);

    cd_t c;
    cd_set(&c, 10000);
    CHECK(cd_left(&c, 0) == 10000 && !cd_done(&c, 0));
    cd_start(&c, 1000);
    CHECK(cd_left(&c, 4000) == 7000);
    cd_pause(&c, 4000);
    CHECK(cd_left(&c, 50000) == 7000);
    cd_start(&c, 50000);
    CHECK(cd_left(&c, 56999) == 1 && !cd_done(&c, 56999));
    CHECK(cd_done(&c, 57000) && cd_left(&c, 90000) == 0);
    cd_set(&c, 0);
    CHECK(!cd_done(&c, 0));
    cd_start(&c, 0);
    CHECK(!c.running);
}

static void test_colour(void)
{
    col_rgb_t c;
    char b[16];
    CHECK(col_parse_hex("#1E88E5", &c) && c.r == 0x1E && c.g == 0x88 && c.b == 0xE5);
    CHECK(col_parse_hex("18e", &c) && c.r == 0x11 && c.g == 0x88 && c.b == 0xEE);
    CHECK(!col_parse_hex("#12345", &c));
    CHECK(!col_parse_hex("#GG0000", &c));
    CHECK(!col_parse_hex("#1234567", &c));
    col_format_hex((col_rgb_t){ 255, 0, 10 }, b, sizeof b); STREQ(b, "#FF000A");
    col_hsl_t h = col_rgb_to_hsl((col_rgb_t){ 255, 0, 0 });
    NEAR(h.h, 0, 1e-9); NEAR(h.s, 100, 1e-9); NEAR(h.l, 50, 1e-9);
    h = col_rgb_to_hsl((col_rgb_t){ 0x1E, 0x88, 0xE5 });
    NEAR(h.h, 208.0, 0.5); NEAR(h.s, 79.0, 0.5); NEAR(h.l, 50.8, 0.5);
    h = col_rgb_to_hsl((col_rgb_t){ 128, 128, 128 });
    NEAR(h.s, 0, 1e-9);
    /* Every grey and a spread of colours round-trip through HSL. */
    for (int r = 0; r < 256; r += 15)
        for (int g = 0; g < 256; g += 17)
            for (int bl = 0; bl < 256; bl += 51) {
                col_rgb_t in = { (uint8_t)r, (uint8_t)g, (uint8_t)bl };
                col_rgb_t out = col_hsl_to_rgb(col_rgb_to_hsl(in));
                CHECK(out.r == in.r && out.g == in.g && out.b == in.b);
            }
    c = col_hsl_to_rgb((col_hsl_t){ 120, 100, 25 });
    CHECK(c.r == 0 && c.g == 128 && c.b == 0);
    c = col_hsl_to_rgb((col_hsl_t){ 480, 100, 50 });             /* wraps to 120 */
    CHECK(c.r == 0 && c.g == 255 && c.b == 0);
    col_hsv_t v = col_rgb_to_hsv((col_rgb_t){ 0, 0, 255 });
    NEAR(v.h, 240, 1e-9); NEAR(v.s, 100, 1e-9); NEAR(v.v, 100, 1e-9);
    NEAR(col_contrast((col_rgb_t){ 0, 0, 0 }, (col_rgb_t){ 255, 255, 255 }), 21, 1e-9);
    NEAR(col_contrast((col_rgb_t){ 0x77, 0x77, 0x77 }, (col_rgb_t){ 255, 255, 255 }), 4.48, 0.01);
    CHECK(col_to_u32((col_rgb_t){ 1, 2, 3 }) == 0x010203);
    c = col_from_u32(0xABCDEF);
    CHECK(c.r == 0xAB && c.g == 0xCD && c.b == 0xEF);
}

static void test_measure(void)
{
    NEAR(meas_default_px_per_mm(), 11.564, 0.001);
    /* Upright in portrait: the up vector is the screen's up. */
    meas_vec_t v = meas_to_screen(0, 0, -1000, 0);
    NEAR(v.right, 0, 1e-3); NEAR(v.up, 1000, 1e-3);
    /* Landscape with the panel's right edge up (rotation 90). */
    v = meas_to_screen(90, 1000, 0, 0);
    NEAR(v.right, 0, 1e-3); NEAR(v.up, 1000, 1e-3);
    v = meas_to_screen(180, 0, 1000, 0);
    NEAR(v.up, 1000, 1e-3);
    v = meas_to_screen(270, -1000, 0, 0);
    NEAR(v.up, 1000, 1e-3);
    /* At rotation 90, screen right is the panel's bottom (+y). */
    v = meas_to_screen(90, 0, 1000, 0);
    NEAR(v.right, 1000, 1e-3); NEAR(v.up, 0, 1e-3);
    v = meas_to_screen(0, 1000, 0, 0);
    NEAR(v.right, 1000, 1e-3);

    meas_tilt_t t = meas_tilt((meas_vec_t){ 0, 0, 1000 });
    CHECK(t.flat); NEAR(t.tilt_x, 0, 1e-3); NEAR(t.tilt_y, 0, 1e-3); NEAR(t.total, 0, 1e-2);
    /* Right edge raised 10 degrees. */
    t = meas_tilt((meas_vec_t){ 1000 * sinf(0.174533f), 0, 1000 * cosf(0.174533f) });
    CHECK(t.flat); NEAR(t.tilt_x, 10, 1e-3); NEAR(t.total, 10, 1e-2);
    /* Glass down: still flat, same sense. */
    t = meas_tilt((meas_vec_t){ 1000 * sinf(0.174533f), 0, -1000 * cosf(0.174533f) });
    CHECK(t.flat); NEAR(t.tilt_x, 10, 1e-3);
    /* Upright, turned anticlockwise 5 degrees: right end higher. */
    t = meas_tilt((meas_vec_t){ 1000 * sinf(0.0872665f), 1000 * cosf(0.0872665f), 0 });
    CHECK(!t.flat); NEAR(t.edge, 5, 1e-3);
    t = meas_tilt((meas_vec_t){ -1000, 0, 0 });
    NEAR(t.edge, -90, 1e-3);
    t = meas_tilt((meas_vec_t){ 0, -1000, 0 });
    NEAR(fabsf(t.edge), 180, 1e-3);

    NEAR(meas_angle_diff(10, 350), 20, 1e-4);
    NEAR(meas_angle_diff(350, 10), -20, 1e-4);
    NEAR(meas_angle_diff(180, -180), 0, 1e-4);
    NEAR(meas_smooth(0, 10, 0.5f), 5, 1e-6);
    NEAR(meas_smooth_angle(170, -170, 0.5f), 180, 1e-3);
    NEAR(fabsf(meas_smooth_angle(179, -179, 0.5f)), 180, 1e-3);

    int16_t pcm[1600];
    for (int i = 0; i < 1600; i++) pcm[i] = (int16_t)(32767 * sin(2 * 3.14159265358979 * 1000 * i / 16000.0));
    NEAR(meas_dbfs(pcm, 1600), -3.01, 0.02);
    for (int i = 0; i < 1600; i++) pcm[i] = (int16_t)(pcm[i] / 10);
    NEAR(meas_dbfs(pcm, 1600), -23.01, 0.05);
    for (int i = 0; i < 1600; i++) pcm[i] = 1000;     /* pure DC is silence */
    CHECK(meas_dbfs(pcm, 1600) == MEAS_DB_FLOOR);
    CHECK(meas_dbfs(NULL, 0) == MEAS_DB_FLOOR);
    meas_stats_t s;
    meas_stats_reset(&s);
    CHECK(meas_stats_leq(&s) == MEAS_DB_FLOOR);
    meas_stats_add(&s, -20);
    meas_stats_add(&s, -20);
    NEAR(meas_stats_leq(&s), -20, 1e-4);
    meas_stats_add(&s, -10);
    meas_stats_add(&s, -30);
    CHECK(s.min == -30 && s.max == -10);
    NEAR(meas_stats_leq(&s), 10 * log10((0.01 + 0.01 + 0.1 + 0.001) / 4), 1e-4);
}

int main(void)
{
    test_fmt();
    test_entry();
    test_units();
    test_bases();
    test_percent_ratio();
    test_money();
    test_random();
    test_elec();
    test_dates();
    test_colour();
    test_measure();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
