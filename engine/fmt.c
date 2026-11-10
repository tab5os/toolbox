#include "fmt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void strip_zeros(char *s)
{
    char *dot = strchr(s, '.');
    if (!dot) return;
    char *end = s + strlen(s);
    while (end > dot + 1 && end[-1] == '0') *--end = '\0';
    if (end == dot + 1) *dot = '\0';
}

/* "1234567.5" -> "1,234,567.5", in place (out has room n). */
static void group_int(char *s, size_t n)
{
    char *start = s + (s[0] == '-');
    char *dot = strchr(start, '.');
    size_t int_len = dot ? (size_t)(dot - start) : strlen(start);
    if (int_len <= 3) return;
    size_t commas = (int_len - 1) / 3;
    size_t len = strlen(s);
    if (len + commas + 1 > n) return;          /* no room: leave it plain */
    memmove(start + int_len + commas, start + int_len, len - (size_t)(start - s) - int_len + 1);
    char *src = start + int_len - 1, *dst = start + int_len + commas - 1;
    int k = 0;
    while (src >= start) {
        *dst-- = *src--;
        if (++k % 3 == 0 && src >= start) *dst-- = ',';
    }
}

bool fmt_num(double v, int sig, bool group, char *out, size_t n)
{
    if (!out || n == 0) return false;
    if (sig < 1) sig = 1;
    if (sig > 15) sig = 15;
    if (!isfinite(v)) {
        snprintf(out, n, "-");
        return false;
    }
    char tmp[64];
    /* Round to sig digits first, so float noise (0.1 + 0.2) goes. */
    snprintf(tmp, sizeof tmp, "%.*e", sig - 1, v);
    v = strtod(tmp, NULL);
    double av = fabs(v);
    if (av == 0) {
        snprintf(out, n, "0");
        return true;
    }
    if (av >= 1e12 || av < 1e-6) {
        char *e = strchr(tmp, 'e');
        int exp = e ? atoi(e + 1) : 0;
        if (e) *e = '\0';
        strip_zeros(tmp);
        snprintf(out, n, "%se%d", tmp, exp);
        return true;
    }
    int mag = (int)floor(log10(av));
    int decimals = sig - 1 - mag;
    if (decimals < 0) decimals = 0;
    if (decimals > 15) decimals = 15;
    snprintf(tmp, sizeof tmp, "%.*f", decimals, v);
    strip_zeros(tmp);
    if (strcmp(tmp, "-0") == 0) snprintf(tmp, sizeof tmp, "0");
    if (group) group_int(tmp, sizeof tmp);
    snprintf(out, n, "%s", tmp);
    return true;
}

bool fmt_si(double v, int sig, const char *unit, char *out, size_t n)
{
    static const struct { double f; const char *p; } pre[] = {
        { 1e12, "T" }, { 1e9, "G" }, { 1e6, "M" }, { 1e3, "k" }, { 1, "" },
        { 1e-3, "m" }, { 1e-6, "\xC2\xB5" }, { 1e-9, "n" }, { 1e-12, "p" },
    };
    if (!unit) unit = "";
    if (!isfinite(v)) {
        snprintf(out, n, "-");
        return false;
    }
    double av = fabs(v);
    size_t i = 4;                              /* no prefix */
    if (av != 0) {
        /* Round first so 999.99 k becomes 1 M rather than 1000 k. */
        char tmp[40];
        snprintf(tmp, sizeof tmp, "%.*e", (sig < 1 ? 1 : sig) - 1, av);
        double r = strtod(tmp, NULL);
        for (i = 0; i < sizeof pre / sizeof pre[0] - 1; i++)
            if (r >= pre[i].f) break;
    }
    char num[40];
    fmt_num(v / pre[i].f, sig, false, num, sizeof num);
    snprintf(out, n, "%s %s%s", num, pre[i].p, unit);
    return true;
}

void fmt_duration(double s, char *out, size_t n)
{
    if (!isfinite(s) || s < 0) {
        snprintf(out, n, "-");
        return;
    }
    if (s < 60) {
        char num[32];
        fmt_num(s, 3, false, num, sizeof num);
        snprintf(out, n, "%s s", num);
        return;
    }
    long long t = (long long)round(s);           /* llround isn't in the app ABI */
    long long d = t / 86400, h = t / 3600 % 24, m = t / 60 % 60, sec = t % 60;
    if (d > 0) snprintf(out, n, "%lld d %lld h", d, h);
    else if (h > 0) snprintf(out, n, "%lld h %02lld min", h, m);
    else snprintf(out, n, "%lld min %02lld s", m, sec);
}

void fmt_clock(long long cs, char *out, size_t n)
{
    if (cs < 0) cs = 0;
    long long h = cs / 360000, m = cs / 6000 % 60, s = cs / 100 % 60, c = cs % 100;
    if (h > 0) snprintf(out, n, "%lld:%02lld:%02lld.%02lld", h, m, s, c);
    else snprintf(out, n, "%02lld:%02lld.%02lld", m, s, c);
}

/* ---- entry ---------------------------------------------------------------- */
void entry_init(entry_t *e, bool allow_point, bool allow_sign, bool hex)
{
    memset(e, 0, sizeof *e);
    e->allow_point = allow_point;
    e->allow_sign = allow_sign;
    e->hex = hex;
}

void entry_set(entry_t *e, const char *text)
{
    snprintf(e->text, sizeof e->text, "%s", text ? text : "");
}

static int body_len(const entry_t *e)
{
    return (int)strlen(e->text) - (e->text[0] == '-');
}

bool entry_key(entry_t *e, entry_op_t op, char arg)
{
    size_t len = strlen(e->text);
    int max = e->max_len > 0 && e->max_len < ENTRY_MAX ? e->max_len : ENTRY_MAX - 1;
    switch (op) {
    case ENTRY_DIGIT: {
        bool ok = (arg >= '0' && arg <= '9') || (e->hex && arg >= 'A' && arg <= 'F');
        if (!ok || body_len(e) >= max || len >= ENTRY_MAX) return false;
        const char *body = e->text + (e->text[0] == '-');
        if (strcmp(body, "0") == 0) {          /* "0" then "5" is "5", not "05" */
            if (arg == '0') return false;
            e->text[len - 1] = arg;
            return true;
        }
        e->text[len] = arg;
        e->text[len + 1] = '\0';
        return true;
    }
    case ENTRY_POINT:
        if (!e->allow_point || strchr(e->text, '.') || body_len(e) >= max - 1 || len + 2 > ENTRY_MAX) return false;
        if (len == 0 || strcmp(e->text, "-") == 0) strcat(e->text, "0");
        strcat(e->text, ".");
        return true;
    case ENTRY_SIGN:
        if (!e->allow_sign) return false;
        if (e->text[0] == '-') memmove(e->text, e->text + 1, len);
        else if (len < ENTRY_MAX) {
            memmove(e->text + 1, e->text, len + 1);
            e->text[0] = '-';
        } else return false;
        return true;
    case ENTRY_BACK:
        if (len == 0) return false;
        e->text[len - 1] = '\0';
        return true;
    case ENTRY_CLEAR:
        if (len == 0) return false;
        e->text[0] = '\0';
        return true;
    }
    return false;
}

bool parse_num(const char *s, double *out)
{
    if (!s || !*s) return false;
    char *end;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || !isfinite(v)) return false;
    if (out) *out = v;
    return true;
}

bool entry_value(const entry_t *e, double *out)
{
    if (e->hex) return false;
    return parse_num(e->text, out);
}
