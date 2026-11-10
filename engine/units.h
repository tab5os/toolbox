/* Unit conversion tables. Every unit converts to a base unit for its
 * category: base = v * factor + offset (linear), or base = factor / v
 * (reciprocal, for fuel economy in distance per volume). Plain C, no UI. */
#pragma once
#include <stdbool.h>

typedef enum {
    UC_LENGTH, UC_MASS, UC_TEMP, UC_VOLUME, UC_AREA, UC_SPEED, UC_PRESSURE,
    UC_ENERGY, UC_POWER, UC_DATA, UC_TIME, UC_ANGLE, UC_FUEL, UC_COOKING,
    UC_COUNT
} units_cat_t;

typedef enum { UK_LINEAR, UK_RECIP } unit_kind_t;

typedef struct {
    const char *sym;        /* short, shown next to numbers: "km", "\xC2\xB0" "C" */
    const char *name;       /* longer: "kilometre" */
    double factor, offset;
    unit_kind_t kind;
} unit_t;

typedef struct {
    const char *id;         /* stable, for saved settings: "length" */
    const char *name;       /* "Length" */
    const unit_t *units;
    int count;
    int def_from, def_to;   /* the units a new page starts with */
    bool allow_negative;    /* temperature */
} unit_cat_t;

const unit_cat_t *units_cat(units_cat_t c);
/* Convert v from unit `from` to unit `to` of category c. false (and NaN)
 * for an index out of range or a value with no answer (0 km/L). */
bool units_convert(units_cat_t c, int from, int to, double v, double *out);
/* The index of a unit by its symbol, or -1. */
int units_find(units_cat_t c, const char *sym);
