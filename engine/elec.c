#include "elec.h"
#include <math.h>

static int popcount4(unsigned k)
{
    int n = 0;
    for (int i = 0; i < 4; i++) n += (k >> i) & 1;
    return n;
}

static bool finite_all(const double *v, int n)
{
    for (int i = 0; i < n; i++)
        if (!isfinite(v[i])) return false;
    return true;
}

bool elec_ohm(double x[4], unsigned known)
{
    known &= 0xF;
    if (popcount4(known) != 2) return false;
    double V = x[OHM_V], I = x[OHM_I], R = x[OHM_R], P = x[OHM_P];
#define K(a, b) (known == ((1u << (a)) | (1u << (b))))
    if (K(OHM_V, OHM_I)) {
        if (I == 0) return false;
        R = V / I;
        P = V * I;
    } else if (K(OHM_V, OHM_R)) {
        if (R == 0) return false;
        I = V / R;
        P = V * V / R;
    } else if (K(OHM_V, OHM_P)) {
        if (V == 0 || P == 0) return false;
        I = P / V;
        R = V * V / P;
    } else if (K(OHM_I, OHM_R)) {
        V = I * R;
        P = I * I * R;
    } else if (K(OHM_I, OHM_P)) {
        if (I == 0) return false;
        V = P / I;
        R = P / (I * I);
    } else {                                   /* R and P */
        if (R <= 0 || P < 0) return false;
        V = sqrt(P * R);
        I = sqrt(P / R);
    }
#undef K
    double out[4] = { V, I, R, P };
    if (!finite_all(out, 4)) return false;
    for (int i = 0; i < 4; i++) x[i] = out[i];
    return true;
}

/* ---- colour bands ------------------------------------------------------------ */
static const char *const s_names[RC_COUNT] = {
    "Black", "Brown", "Red", "Orange", "Yellow", "Green", "Blue", "Violet", "Grey", "White", "Gold", "Silver", "None",
};
static const unsigned s_rgb[RC_COUNT] = {
    0x101010, 0x7B3F00, 0xD32F2F, 0xF57C00, 0xFDD835, 0x2E7D32, 0x1565C0, 0x7B1FA2, 0x9E9E9E, 0xFAFAFA,
    0xC9A227, 0xC0C0C0, 0xD7C49E,                     /* none: the body colour */
};
/* Tolerance in %, 0 where the colour isn't a tolerance (IEC 60062). */
static const double s_tol[RC_COUNT] = { 0, 1, 2, 0.05, 0.02, 0.5, 0.25, 0.1, 0.05, 0, 5, 10, 20 };
static const int s_tempco[RC_COUNT] = { 250, 100, 50, 15, 25, 20, 10, 5, 1, 0, 0, 0, 0 };

const char *elec_colour_name(int c) { return (unsigned)c < RC_COUNT ? s_names[c] : "?"; }
unsigned elec_colour_rgb(int c) { return (unsigned)c < RC_COUNT ? s_rgb[c] : 0; }

bool elec_band_allows(int nb, int band, int c)
{
    if ((unsigned)c >= RC_COUNT || nb < 4 || nb > 6 || band < 0 || band >= nb) return false;
    int digits = nb == 4 ? 2 : 3;
    if (band < digits) return c <= RC_WHITE && !(band == 0 && c == RC_BLACK);
    if (band == digits) return c <= RC_SILVER;                  /* multiplier */
    if (band == digits + 1) return s_tol[c] > 0;                /* tolerance */
    return c <= RC_GREY && s_tempco[c] > 0;                     /* temperature coefficient */
}

static double mult_of(int c)
{
    if (c == RC_GOLD) return 0.1;
    if (c == RC_SILVER) return 0.01;
    return pow(10, c);
}

bool elec_bands_decode(const int *b, int nb, elec_resistor_t *out)
{
    if (nb < 4 || nb > 6) return false;
    for (int i = 0; i < nb; i++)
        if (!elec_band_allows(nb, i, b[i])) return false;
    int digits = nb == 4 ? 2 : 3;
    double d = 0;
    for (int i = 0; i < digits; i++) d = d * 10 + b[i];
    out->ohms = d * mult_of(b[digits]);
    out->tol_pct = s_tol[b[digits + 1]];
    out->tempco_ppm = nb == 6 ? s_tempco[b[5]] : 0;
    return true;
}

bool elec_bands_encode(double ohms, int nb, int *b)
{
    if (nb < 4 || nb > 6 || !(ohms >= 0.1) || ohms > 99.9e9) return false;
    int k = nb == 4 ? 2 : 3;
    int e = (int)floor(log10(ohms) + 1e-12) - (k - 1);
    double D = round(ohms / pow(10, e));
    if (D >= pow(10, k)) {
        e++;
        D = round(ohms / pow(10, e));
    }
    if (fabs(D * pow(10, e) - ohms) > 1e-6 * ohms) return false;    /* needs more digits */
    if (e < -2 || e > 9) return false;
    long di = (long)D;
    for (int i = k - 1; i >= 0; i--) {
        b[i] = (int)(di % 10);
        di /= 10;
    }
    if (b[0] == RC_BLACK) return false;
    b[k] = e == -1 ? RC_GOLD : e == -2 ? RC_SILVER : e;
    b[k + 1] = nb == 4 ? RC_GOLD : RC_BROWN;
    if (nb == 6) b[5] = RC_BROWN;           /* 100 ppm/K */
    return true;
}

/* ---- E-series ------------------------------------------------------------------ */
static const double s_e12[] = { 1.0, 1.2, 1.5, 1.8, 2.2, 2.7, 3.3, 3.9, 4.7, 5.6, 6.8, 8.2 };
static const double s_e24[] = { 1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0,
                                3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1 };

static double e_value(int series, int i)
{
    if (series == 12) return s_e12[i];
    if (series == 24) return s_e24[i];
    return round(pow(10, (double)i / series) * 100) / 100;   /* E48, E96: exact by this rule */
}

double elec_e_nearest(int series, double v, bool up)
{
    if (series != 12 && series != 24 && series != 48 && series != 96) series = 24;
    if (!(v > 0) || !isfinite(v)) return NAN;
    double dec = pow(10, floor(log10(v)));
    double m = v / dec;
    if (m >= 10) {           /* log10 rounding */
        dec *= 10;
        m /= 10;
    }
    double best = 10, best_d = INFINITY;
    for (int i = 0; i <= series; i++) {
        double c = i == series ? 10 : e_value(series, i);
        if (up) {
            if (c >= m * (1 - 1e-9)) return c * dec;
            continue;
        }
        double d = fabs(log(c / m));
        if (d < best_d - 1e-12) {
            best_d = d;
            best = c;
        } else if (fabs(d - best_d) <= 1e-12 && c > best) best = c;
    }
    return best * dec;
}

bool elec_led(double vs, double vf, double amps, int leds, elec_led_t *o)
{
    if (leds < 1 || !(amps > 0) || !(vf >= 0)) return false;
    double drop = vs - vf * leds;
    if (!(drop > 0)) return false;
    o->r_exact = drop / amps;
    o->r_e12 = elec_e_nearest(12, o->r_exact, true);
    o->i_e12 = drop / o->r_e12;
    o->p_resistor = o->i_e12 * o->i_e12 * o->r_e12;
    return isfinite(o->p_resistor);
}

bool elec_divider(double x[4], unsigned known)
{
    known &= 0xF;
    if (popcount4(known) < 3) return false;
    double vin = x[DIV_VIN], r1 = x[DIV_R1], r2 = x[DIV_R2], vout = x[DIV_VOUT];
    if (!(known & (1u << DIV_VOUT)) || known == 0xF) {
        if (r1 < 0 || r2 < 0 || r1 + r2 == 0) return false;
        vout = vin * r2 / (r1 + r2);
    } else if (!(known & (1u << DIV_VIN))) {
        if (!(r2 > 0) || r1 < 0) return false;
        vin = vout * (r1 + r2) / r2;
    } else if (!(known & (1u << DIV_R1))) {
        if (vout == 0 || !(r2 > 0) || fabs(vout) > fabs(vin) || vin * vout < 0) return false;
        r1 = r2 * (vin - vout) / vout;
    } else {
        if (!(r1 > 0) || fabs(vout) >= fabs(vin) || vin * vout < 0) return false;
        r2 = r1 * vout / (vin - vout);
    }
    double out[4] = { vin, r1, r2, vout };
    if (!finite_all(out, 4)) return false;
    for (int i = 0; i < 4; i++) x[i] = out[i];
    return true;
}

bool elec_battery_hours(double mah, double load, double usable, double *h)
{
    if (!(mah > 0) || !(load > 0) || !(usable > 0) || usable > 1) return false;
    *h = mah * usable / load;
    return isfinite(*h);
}
