/* Number, money and random tools. Plain C, no UI. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- bases ---------------------------------------------------------------- */
/* Parse digits in base 2, 8, 10 or 16 (spaces and '_' ignored). false on a
 * bad digit, an empty string or overflow past 64 bits. */
bool nt_parse_base(const char *s, int base, uint64_t *out);
/* Digits of v in base 2, 8, 10 or 16. group: binary in fours, hex in fours,
 * decimal in threes (with commas), separated by spaces. */
void nt_format_base(uint64_t v, int base, bool group, char *out, size_t n);
/* v as a signed number of `bits` bits (8, 16, 32 or 64), two's complement. */
int64_t nt_signed(uint64_t v, int bits);
uint64_t nt_mask(int bits);

/* ---- percentages ---------------------------------------------------------- */
/* All return false when there's no answer (dividing by zero). */
bool nt_pct_of(double pct, double x, double *out);            /* pct% of x */
bool nt_pct_ratio(double part, double whole, double *out);    /* part is out% of whole */
bool nt_pct_change(double from, double to, double *out);      /* from -> to is out% */
bool nt_pct_add(double x, double pct, double *out);           /* x plus pct% */

/* ---- ratio ---------------------------------------------------------------- */
uint64_t nt_gcd(uint64_t a, uint64_t b);
/* a:b in lowest terms. Decimals are scaled up first (1.5:2 -> 3:4). false
 * if either is zero, negative or too big. */
bool nt_ratio_simplify(double a, double b, double *ra, double *rb);
/* Given a:b, the missing side for a new a (or new b). */
bool nt_ratio_scale(double a, double b, double new_a, double *new_b);

/* ---- money ---------------------------------------------------------------- */
typedef struct {
    double tip, total, per_person, tip_per_person;
} nt_tip_t;
bool nt_tip(double bill, double pct, int people, bool round_up, nt_tip_t *out);

typedef struct {
    double saved, final, with_tax;
} nt_discount_t;
/* price less pct%, then less extra% (a second discount, 0 for none),
 * then plus tax%. */
bool nt_discount(double price, double pct, double extra_pct, double tax_pct, nt_discount_t *out);

/* Price per unit of two offers; best = 0 or 1, saving_pct: how much
 * cheaper per unit the best is. */
typedef struct {
    double unit[2];
    int best;              /* -1 when equal */
    double saving_pct;
} nt_unitprice_t;
bool nt_unit_price(double price_a, double qty_a, double price_b, double qty_b, nt_unitprice_t *out);

/* ---- random --------------------------------------------------------------- */
/* xoshiro256** seeded through splitmix64. Mix in fresh entropy (touch timing,
 * the clock, sensor noise) whenever there is some. */
typedef struct {
    uint64_t s[4];
} nt_rng_t;
void nt_rng_seed(nt_rng_t *r, uint64_t seed);
void nt_rng_mix(nt_rng_t *r, uint64_t entropy);
uint64_t nt_rng_next(nt_rng_t *r);
/* Uniform in [lo, hi], without modulo bias. lo > hi swaps them. */
int64_t nt_rng_range(nt_rng_t *r, int64_t lo, int64_t hi);

#define NT_PW_LOWER 1u
#define NT_PW_UPPER 2u
#define NT_PW_DIGITS 4u
#define NT_PW_SYMBOLS 8u
#define NT_PW_NO_AMBIGUOUS 16u    /* no 0 O o 1 l I | */
#define NT_PW_MAX 64
/* A password of len (4..64) characters from the chosen sets, with at least
 * one from each set. Returns the entropy in bits, or 0 if no set is chosen
 * or out is too small. */
double nt_password(nt_rng_t *r, int len, unsigned sets, char *out, size_t n);
