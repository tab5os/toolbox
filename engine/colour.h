/* Colour: HEX, RGB, HSL and HSV, and WCAG contrast. Plain C, no UI. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t r, g, b;
} col_rgb_t;

typedef struct {
    double h, s, l;      /* 0..360, 0..100, 0..100 */
} col_hsl_t;

typedef struct {
    double h, s, v;      /* 0..360, 0..100, 0..100 */
} col_hsv_t;

/* "#1E88E5", "1e88e5", "#18E" (and "18E"). */
bool col_parse_hex(const char *s, col_rgb_t *out);
void col_format_hex(col_rgb_t c, char *out, size_t n);   /* "#1E88E5" */
col_hsl_t col_rgb_to_hsl(col_rgb_t c);
col_rgb_t col_hsl_to_rgb(col_hsl_t h);
col_hsv_t col_rgb_to_hsv(col_rgb_t c);
/* WCAG 2 relative luminance (0..1) and contrast ratio (1..21). */
double col_luminance(col_rgb_t c);
double col_contrast(col_rgb_t a, col_rgb_t b);
uint32_t col_to_u32(col_rgb_t c);
col_rgb_t col_from_u32(uint32_t v);
