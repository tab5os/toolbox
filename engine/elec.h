/* Electronics: Ohm's law, resistor colour codes, E-series, LED resistors,
 * voltage dividers, battery runtime. Plain C, no UI. */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Ohm's law. vals[] is V, I, R, P (volts, amps, ohms, watts). Given two
 * known ones (bits of `known`, 1 << OHM_V ...), fills in the other two.
 * false if fewer than two are known or there's no answer. */
enum { OHM_V, OHM_I, OHM_R, OHM_P };
bool elec_ohm(double vals[4], unsigned known);

/* Resistor colour bands. Colours 0-9 are black..white, then gold (10),
 * silver (11), none (12). */
enum { RC_BLACK, RC_BROWN, RC_RED, RC_ORANGE, RC_YELLOW, RC_GREEN, RC_BLUE, RC_VIOLET, RC_GREY, RC_WHITE,
       RC_GOLD, RC_SILVER, RC_NONE, RC_COUNT };
const char *elec_colour_name(int c);
unsigned elec_colour_rgb(int c);           /* 0xRRGGBB for drawing */
/* Valid colours for band `band` of an n-band (4, 5 or 6) resistor. */
bool elec_band_allows(int nbands, int band, int colour);

typedef struct {
    double ohms;
    double tol_pct;     /* 0 if the band is missing or invalid */
    int tempco_ppm;     /* 6-band only, else 0 */
} elec_resistor_t;
/* Decode bands[0..nbands-1]. false for an impossible combination. */
bool elec_bands_decode(const int *bands, int nbands, elec_resistor_t *out);
/* Encode ohms into the digit and multiplier bands of an n-band resistor
 * (n = 4: 2 digits, 5 or 6: 3 digits). The tolerance band is set to brown
 * (1%) for 5 and 6 bands and gold (5%) for 4. false if ohms needs more
 * significant digits than the bands hold or is out of range (0.1 to 99.9 G). */
bool elec_bands_encode(double ohms, int nbands, int *bands);

/* E-series: 12, 24, 48 or 96 values per decade. The nearest standard value
 * (ties go up), or the next one at or above v when up is set. */
double elec_e_nearest(int series, double v, bool up);

/* LED series resistor: supply volts, LED forward volts, current in amps,
 * leds in series. */
typedef struct {
    double r_exact, r_e12, i_e12, p_resistor;
} elec_led_t;
bool elec_led(double vs, double vf, double amps, int leds, elec_led_t *out);

/* Voltage divider. vals[] is Vin, R1, R2, Vout; any three give the fourth
 * (bits of known as for elec_ohm). R1 is the top resistor. */
enum { DIV_VIN, DIV_R1, DIV_R2, DIV_VOUT };
bool elec_divider(double vals[4], unsigned known);

/* Hours a battery lasts: capacity mAh, load mA, usable fraction (0..1]. */
bool elec_battery_hours(double mah, double load_ma, double usable, double *hours);
