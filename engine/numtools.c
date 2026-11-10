#include "numtools.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- bases ---------------------------------------------------------------- */
bool nt_parse_base(const char *s, int base, uint64_t *out)
{
    if (!s || (base != 2 && base != 8 && base != 10 && base != 16)) return false;
    uint64_t v = 0;
    int digits = 0;
    for (; *s; s++) {
        char c = *s;
        if (c == ' ' || c == '_' || c == ',') continue;
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        if (d >= base) return false;
        if (v > (UINT64_MAX - (uint64_t)d) / (uint64_t)base) return false;
        v = v * (uint64_t)base + (uint64_t)d;
        digits++;
    }
    if (!digits) return false;
    *out = v;
    return true;
}

void nt_format_base(uint64_t v, int base, bool group, char *out, size_t n)
{
    char rev[96];
    int len = 0, every = base == 10 ? 3 : base == 8 ? 3 : 4;
    char sep = base == 10 ? ',' : ' ';
    if (n == 0) return;
    if (base != 2 && base != 8 && base != 10 && base != 16) base = 10;
    int k = 0;
    do {
        if (group && k && k % every == 0) rev[len++] = sep;
        int d = (int)(v % (uint64_t)base);
        rev[len++] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
        v /= (uint64_t)base;
        k++;
    } while (v && len < (int)sizeof rev - 2);
    size_t o = 0;
    while (len > 0 && o + 1 < n) out[o++] = rev[--len];
    out[o] = '\0';
}

uint64_t nt_mask(int bits)
{
    return bits >= 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
}

int64_t nt_signed(uint64_t v, int bits)
{
    if (bits >= 64) return (int64_t)v;
    v &= nt_mask(bits);
    uint64_t sign = (uint64_t)1 << (bits - 1);
    return (v & sign) ? (int64_t)(v | ~nt_mask(bits)) : (int64_t)v;
}

/* ---- percentages ---------------------------------------------------------- */
static bool ok(double v, double *out)
{
    if (!isfinite(v)) return false;
    *out = v;
    return true;
}

bool nt_pct_of(double pct, double x, double *out) { return ok(pct * x / 100.0, out); }
bool nt_pct_ratio(double part, double whole, double *out) { return whole != 0 && ok(part / whole * 100.0, out); }
bool nt_pct_change(double from, double to, double *out) { return from != 0 && ok((to - from) / fabs(from) * 100.0, out); }
bool nt_pct_add(double x, double pct, double *out) { return ok(x * (1.0 + pct / 100.0), out); }

/* ---- ratio ---------------------------------------------------------------- */
uint64_t nt_gcd(uint64_t a, uint64_t b)
{
    while (b) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

bool nt_ratio_simplify(double a, double b, double *ra, double *rb)
{
    if (!(a > 0) || !(b > 0) || a > 1e12 || b > 1e12) return false;
    /* Scale both by 10 until they're whole (up to 6 decimal places). */
    double scale = 1;
    for (int i = 0; i < 6; i++) {
        double x = a * scale, y = b * scale;
        if (fabs(x - round(x)) < 1e-9 * scale && fabs(y - round(y)) < 1e-9 * scale) break;
        scale *= 10;
    }
    double x = round(a * scale), y = round(b * scale);
    if (x < 1 || y < 1 || x > 9e15 || y > 9e15) return false;
    uint64_t g = nt_gcd((uint64_t)x, (uint64_t)y);
    *ra = x / (double)g;
    *rb = y / (double)g;
    return true;
}

bool nt_ratio_scale(double a, double b, double new_a, double *new_b)
{
    return a != 0 && ok(new_a * b / a, new_b);
}

/* ---- money ---------------------------------------------------------------- */
bool nt_tip(double bill, double pct, int people, bool round_up, nt_tip_t *o)
{
    if (!(bill >= 0) || !(pct >= 0) || people < 1 || !isfinite(bill) || !isfinite(pct)) return false;
    o->tip = bill * pct / 100.0;
    o->total = bill + o->tip;
    o->per_person = o->total / people;
    if (round_up) {
        /* Each person pays a whole amount; the tip takes up the difference. */
        double each = ceil(o->per_person - 1e-9);
        o->per_person = each;
        o->total = each * people;
        o->tip = o->total - bill;
    }
    o->tip_per_person = o->tip / people;
    return true;
}

bool nt_discount(double price, double pct, double extra, double tax, nt_discount_t *o)
{
    if (!isfinite(price) || !isfinite(pct) || !isfinite(extra) || !isfinite(tax)) return false;
    double f = price * (1.0 - pct / 100.0) * (1.0 - extra / 100.0);
    o->final = f;
    o->saved = price - f;
    o->with_tax = f * (1.0 + tax / 100.0);
    return true;
}

bool nt_unit_price(double pa, double qa, double pb, double qb, nt_unitprice_t *o)
{
    if (!(qa > 0) || !(qb > 0) || !isfinite(pa) || !isfinite(pb)) return false;
    o->unit[0] = pa / qa;
    o->unit[1] = pb / qb;
    /* fmin and fmax aren't in the app ABI. */
    double lo = o->unit[0] < o->unit[1] ? o->unit[0] : o->unit[1];
    double hi = o->unit[0] < o->unit[1] ? o->unit[1] : o->unit[0];
    if (fabs(o->unit[0] - o->unit[1]) <= 1e-12 * hi) {
        o->best = -1;
        o->saving_pct = 0;
    } else {
        o->best = o->unit[0] < o->unit[1] ? 0 : 1;
        o->saving_pct = hi > 0 ? (hi - lo) / hi * 100.0 : 0;
    }
    return true;
}

/* ---- random --------------------------------------------------------------- */
static uint64_t splitmix(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void nt_rng_seed(nt_rng_t *r, uint64_t seed)
{
    for (int i = 0; i < 4; i++) r->s[i] = splitmix(&seed);
}

void nt_rng_mix(nt_rng_t *r, uint64_t e)
{
    uint64_t x = e ^ r->s[0];
    for (int i = 0; i < 4; i++) r->s[i] ^= splitmix(&x);
    if (!(r->s[0] | r->s[1] | r->s[2] | r->s[3])) r->s[0] = 1;   /* never all zero */
}

static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

uint64_t nt_rng_next(nt_rng_t *r)
{
    uint64_t *s = r->s;
    uint64_t result = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return result;
}

int64_t nt_rng_range(nt_rng_t *r, int64_t lo, int64_t hi)
{
    if (lo > hi) {
        int64_t t = lo;
        lo = hi;
        hi = t;
    }
    uint64_t span = (uint64_t)hi - (uint64_t)lo;
    if (span == UINT64_MAX) return (int64_t)nt_rng_next(r);
    uint64_t range = span + 1, limit = UINT64_MAX - UINT64_MAX % range, x;
    do x = nt_rng_next(r); while (x >= limit);
    return (int64_t)((uint64_t)lo + x % range);
}

double nt_password(nt_rng_t *r, int len, unsigned sets, char *out, size_t n)
{
    static const char *const all[4] = {
        "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "0123456789", "!#$%&*+-=?@^_~.:;/()[]{}<>",
    };
    static const char *ambiguous = "0Oo1lI|";
    char pool[4][32];
    int sizes[4] = { 0 }, nsets = 0, total = 0;
    if (len < 4) len = 4;
    if (len > NT_PW_MAX) len = NT_PW_MAX;
    if (!out || n < (size_t)len + 1) return 0;
    for (int i = 0; i < 4; i++) {
        if (!(sets & (1u << i))) continue;
        int k = 0;
        for (const char *c = all[i]; *c; c++)
            if (!(sets & NT_PW_NO_AMBIGUOUS) || !strchr(ambiguous, *c)) pool[nsets][k++] = *c;
        sizes[nsets++] = k;
        total += k;
    }
    if (!nsets || len < nsets) return 0;
    /* One from each set, the rest from all of them, then shuffle. */
    for (int i = 0; i < len; i++) {
        if (i < nsets) {
            out[i] = pool[i][nt_rng_range(r, 0, sizes[i] - 1)];
            continue;
        }
        int k = (int)nt_rng_range(r, 0, total - 1);
        for (int j = 0; j < nsets; j++) {
            if (k < sizes[j]) {
                out[i] = pool[j][k];
                break;
            }
            k -= sizes[j];
        }
    }
    for (int i = len - 1; i > 0; i--) {
        int j = (int)nt_rng_range(r, 0, i);
        char t = out[i];
        out[i] = out[j];
        out[j] = t;
    }
    out[len] = '\0';
    return len * log2((double)total);
}
