/* Number formatting and the typed-number buffer shared by every tool.
 * Plain C, no UI: tests/test_engine.c checks it on a computer. */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/* A number for people: up to sig significant digits (1..15), trailing zeros
 * dropped, "1,234,567.5" with group separators when group is set, and
 * "1.5e-9" style when very large or very small. "-" for NaN or infinity.
 * Returns false (and writes "-") when v isn't finite. */
bool fmt_num(double v, int sig, bool group, char *out, size_t n);

/* With an SI prefix and a unit: "4.7 k\xCE\xA9", "220 \xC2\xB5" "F", "1.5 mA".
 * Prefixes p n u(as micro sign) m - k M G T. */
bool fmt_si(double v, int sig, const char *unit, char *out, size_t n);

/* Seconds as "1 h 23 min", "4 min 05 s", "12.3 s", "3 d 4 h". */
void fmt_duration(double seconds, char *out, size_t n);

/* Stopwatch style: "1:02:03.45" or "02:03.45" (centiseconds). */
void fmt_clock(long long centis, char *out, size_t n);

/* ---- the number being typed ---------------------------------------------
 * A short string of digits with an optional sign and point, edited by the
 * keypad. Hex digits are allowed when hex is set. */
#define ENTRY_MAX 24

typedef enum {
    ENTRY_DIGIT,       /* arg: '0'..'9', 'A'..'F' */
    ENTRY_POINT,
    ENTRY_SIGN,
    ENTRY_BACK,
    ENTRY_CLEAR,
} entry_op_t;

typedef struct {
    char text[ENTRY_MAX + 1];
    bool allow_point, allow_sign, hex;
    int max_len;        /* digits and point, without the sign; 0 = ENTRY_MAX */
} entry_t;

void entry_init(entry_t *e, bool allow_point, bool allow_sign, bool hex);
void entry_set(entry_t *e, const char *text);
/* Apply a key. Returns true if the text changed. */
bool entry_key(entry_t *e, entry_op_t op, char arg);
/* The value, false if empty or only "-" or ".". */
bool entry_value(const entry_t *e, double *out);
/* Parse a typed decimal number: the same rules as entry_value. */
bool parse_num(const char *s, double *out);
