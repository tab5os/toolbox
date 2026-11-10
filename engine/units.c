#include "units.h"
#include <math.h>
#include <string.h>

#define DEG "\xC2\xB0"
#define SQ "\xC2\xB2"
#define CU "\xC2\xB3"
#define MU "\xC2\xB5"
#define DOT "\xC2\xB7"
#define L(sym, name, f) { sym, name, f, 0, UK_LINEAR }
#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* Base: metre. Exact by definition except light-year (IAU). */
static const unit_t s_length[] = {
    L("mm", "millimetre", 1e-3), L("cm", "centimetre", 1e-2), L("m", "metre", 1), L("km", "kilometre", 1e3),
    L(MU "m", "micrometre", 1e-6), L("in", "inch", 0.0254), L("ft", "foot", 0.3048), L("yd", "yard", 0.9144),
    L("mi", "mile", 1609.344), L("nmi", "nautical mile", 1852), L("mil", "thou (1/1000 in)", 2.54e-5),
    L("ly", "light-year", 9460730472580800.0),
};
/* Base: kilogram. */
static const unit_t s_mass[] = {
    L("mg", "milligram", 1e-6), L("g", "gram", 1e-3), L("kg", "kilogram", 1), L("t", "tonne", 1e3),
    L("oz", "ounce", 0.028349523125), L("lb", "pound", 0.45359237), L("st", "stone", 6.35029318),
    L("ton (US)", "short ton", 907.18474), L("ton (UK)", "long ton", 1016.0469088), L("ct", "carat", 2e-4),
    L("gr", "grain", 6.479891e-5), L("ozt", "troy ounce", 0.0311034768),
};
/* Base: kelvin. */
static const unit_t s_temp[] = {
    { DEG "C", "Celsius", 1, 273.15, UK_LINEAR },
    { DEG "F", "Fahrenheit", 5.0 / 9.0, 273.15 - 32.0 * 5.0 / 9.0, UK_LINEAR },
    { "K", "kelvin", 1, 0, UK_LINEAR },
    { DEG "R", "Rankine", 5.0 / 9.0, 0, UK_LINEAR },
};
/* Base: litre. */
static const unit_t s_volume[] = {
    L("mL", "millilitre", 1e-3), L("L", "litre", 1), L("m" CU, "cubic metre", 1e3), L("cm" CU, "cubic centimetre", 1e-3),
    L("in" CU, "cubic inch", 0.016387064), L("ft" CU, "cubic foot", 28.316846592),
    L("fl oz (US)", "US fluid ounce", 0.0295735295625), L("cup (US)", "US cup", 0.2365882365),
    L("pt (US)", "US pint", 0.473176473), L("qt (US)", "US quart", 0.946352946), L("gal (US)", "US gallon", 3.785411784),
    L("fl oz (UK)", "imperial fluid ounce", 0.0284130625), L("pt (UK)", "imperial pint", 0.56826125),
    L("gal (UK)", "imperial gallon", 4.54609),
};
/* Base: square metre. */
static const unit_t s_area[] = {
    L("mm" SQ, "square millimetre", 1e-6), L("cm" SQ, "square centimetre", 1e-4), L("m" SQ, "square metre", 1),
    L("ha", "hectare", 1e4), L("km" SQ, "square kilometre", 1e6), L("in" SQ, "square inch", 6.4516e-4),
    L("ft" SQ, "square foot", 0.09290304), L("yd" SQ, "square yard", 0.83612736), L("ac", "acre", 4046.8564224),
    L("mi" SQ, "square mile", 2589988.110336),
};
/* Base: metre per second. */
static const unit_t s_speed[] = {
    L("m/s", "metres per second", 1), L("km/h", "kilometres per hour", 1 / 3.6), L("mph", "miles per hour", 0.44704),
    L("kn", "knot", 1852.0 / 3600.0), L("ft/s", "feet per second", 0.3048),
    L("Mach", "speed of sound, 15 " DEG "C sea level", 340.294), L("c", "speed of light", 299792458),
};
/* Base: pascal. */
static const unit_t s_pressure[] = {
    L("Pa", "pascal", 1), L("hPa", "hectopascal", 100), L("kPa", "kilopascal", 1e3), L("MPa", "megapascal", 1e6),
    L("bar", "bar", 1e5), L("mbar", "millibar", 100), L("atm", "standard atmosphere", 101325),
    L("psi", "pound per square inch", 6894.757293168), L("mmHg", "millimetre of mercury", 133.322387415),
    L("inHg", "inch of mercury", 3386.388640341), L("Torr", "torr", 101325.0 / 760.0),
};
/* Base: joule. */
static const unit_t s_energy[] = {
    L("J", "joule", 1), L("kJ", "kilojoule", 1e3), L("MJ", "megajoule", 1e6), L("Wh", "watt-hour", 3600),
    L("kWh", "kilowatt-hour", 3.6e6), L("cal", "calorie", 4.184), L("kcal", "kilocalorie (food Calorie)", 4184),
    L("BTU", "British thermal unit", 1055.05585262), L("eV", "electronvolt", 1.602176634e-19),
    L("ft" DOT "lbf", "foot-pound", 1.3558179483314004), L("mAh@3.7V", "milliamp-hour at 3.7 V", 13.32),
};
/* Base: watt. */
static const unit_t s_power[] = {
    L("mW", "milliwatt", 1e-3), L("W", "watt", 1), L("kW", "kilowatt", 1e3), L("MW", "megawatt", 1e6),
    L("hp", "mechanical horsepower", 745.69987158227022), L("PS", "metric horsepower", 735.49875),
    L("BTU/h", "BTU per hour", 0.29307107017222), L("kcal/h", "kilocalorie per hour", 4184.0 / 3600.0),
    L("dBm", "decibel-milliwatt", 0),   /* logarithmic: is_dbm() */
};
/* Base: byte. */
static const unit_t s_data[] = {
    L("bit", "bit", 0.125), L("B", "byte", 1), L("kB", "kilobyte", 1e3), L("MB", "megabyte", 1e6),
    L("GB", "gigabyte", 1e9), L("TB", "terabyte", 1e12), L("KiB", "kibibyte", 1024.0), L("MiB", "mebibyte", 1048576.0),
    L("GiB", "gibibyte", 1073741824.0), L("TiB", "tebibyte", 1099511627776.0), L("kbit", "kilobit", 125),
    L("Mbit", "megabit", 125e3), L("Gbit", "gigabit", 125e6),
};
/* Base: second. Month and year are Gregorian averages. */
static const unit_t s_time[] = {
    L(MU "s", "microsecond", 1e-6), L("ms", "millisecond", 1e-3), L("s", "second", 1), L("min", "minute", 60),
    L("h", "hour", 3600), L("d", "day", 86400), L("wk", "week", 604800), L("mo", "month (average)", 2629746),
    L("yr", "year (average)", 31556952),
};
/* Base: degree. */
static const unit_t s_angle[] = {
    L(DEG, "degree", 1), L("rad", "radian", 180.0 / 3.14159265358979323846), L("grad", "gradian", 0.9),
    L("'", "arcminute", 1.0 / 60.0), L("\"", "arcsecond", 1.0 / 3600.0), L("turn", "turn", 360),
    L("mil", "NATO mil (6400 per turn)", 360.0 / 6400.0),
};
/* Base: litres per 100 km. */
static const unit_t s_fuel[] = {
    L("L/100km", "litres per 100 km", 1),
    { "km/L", "kilometres per litre", 100, 0, UK_RECIP },
    { "mpg (US)", "miles per US gallon", 235.214583333333, 0, UK_RECIP },
    { "mpg (UK)", "miles per imperial gallon", 282.480936331822, 0, UK_RECIP },
};
/* Base: millilitre. */
static const unit_t s_cooking[] = {
    L("mL", "millilitre", 1), L("L", "litre", 1000), L("tsp", "US teaspoon", 4.92892159375),
    L("tbsp", "US tablespoon", 14.78676478125), L("fl oz", "US fluid ounce", 29.5735295625),
    L("cup", "US cup", 236.5882365), L("cup (metric)", "metric cup", 250), L("pt", "US pint", 473.176473),
    L("qt", "US quart", 946.352946), L("tsp (metric)", "metric teaspoon", 5), L("tbsp (metric)", "metric tablespoon", 15),
    L("tbsp (AU)", "Australian tablespoon", 20), L("cup (UK)", "imperial cup", 284.130625),
};

static const unit_cat_t s_cats[UC_COUNT] = {
    [UC_LENGTH] = { "length", "Length", s_length, N(s_length), 1, 5, false },
    [UC_MASS] = { "mass", "Mass", s_mass, N(s_mass), 2, 5, false },
    [UC_TEMP] = { "temp", "Temperature", s_temp, N(s_temp), 0, 1, true },
    [UC_VOLUME] = { "volume", "Volume", s_volume, N(s_volume), 1, 10, false },
    [UC_AREA] = { "area", "Area", s_area, N(s_area), 2, 6, false },
    [UC_SPEED] = { "speed", "Speed", s_speed, N(s_speed), 1, 2, false },
    [UC_PRESSURE] = { "pressure", "Pressure", s_pressure, N(s_pressure), 4, 7, false },
    [UC_ENERGY] = { "energy", "Energy", s_energy, N(s_energy), 4, 6, false },
    [UC_POWER] = { "power", "Power", s_power, N(s_power), 2, 4, true },
    [UC_DATA] = { "data", "Data size", s_data, N(s_data), 3, 7, false },
    [UC_TIME] = { "time", "Time", s_time, N(s_time), 4, 3, false },
    [UC_ANGLE] = { "angle", "Angle", s_angle, N(s_angle), 0, 1, true },
    [UC_FUEL] = { "fuel", "Fuel economy", s_fuel, N(s_fuel), 0, 2, false },
    [UC_COOKING] = { "cooking", "Cooking", s_cooking, N(s_cooking), 5, 0, false },
};

const unit_cat_t *units_cat(units_cat_t c)
{
    return (unsigned)c < UC_COUNT ? &s_cats[c] : NULL;
}

/* dBm is logarithmic: P(W) = 10^(dBm/10) / 1000. */
static bool is_dbm(const unit_t *u)
{
    return strcmp(u->sym, "dBm") == 0;
}

static bool to_base(const unit_t *u, double v, double *out)
{
    if (is_dbm(u)) *out = pow(10.0, v / 10.0) / 1000.0;
    else if (u->kind == UK_RECIP) {
        if (v == 0) return false;
        *out = u->factor / v;
    } else *out = v * u->factor + u->offset;
    return isfinite(*out);
}

static bool from_base(const unit_t *u, double b, double *out)
{
    if (is_dbm(u)) {
        if (b <= 0) return false;
        *out = 10.0 * log10(b * 1000.0);
    } else if (u->kind == UK_RECIP) {
        if (b == 0) return false;
        *out = u->factor / b;
    } else *out = (b - u->offset) / u->factor;
    return isfinite(*out);
}

bool units_convert(units_cat_t c, int from, int to, double v, double *out)
{
    const unit_cat_t *cat = units_cat(c);
    *out = NAN;
    if (!cat || from < 0 || from >= cat->count || to < 0 || to >= cat->count || !isfinite(v)) return false;
    double b;
    if (!to_base(&cat->units[from], v, &b)) return false;
    /* Below absolute zero has no answer. */
    if (c == UC_TEMP && b < -1e-9) return false;
    double r;
    if (!from_base(&cat->units[to], b, &r)) return false;
    *out = r;
    return true;
}

int units_find(units_cat_t c, const char *sym)
{
    const unit_cat_t *cat = units_cat(c);
    if (!cat || !sym) return -1;
    for (int i = 0; i < cat->count; i++)
        if (strcmp(cat->units[i].sym, sym) == 0) return i;
    return -1;
}
