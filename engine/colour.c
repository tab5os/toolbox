#include "colour.h"
#include <math.h>
#include <stdio.h>

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool col_parse_hex(const char *s, col_rgb_t *out)
{
    if (!s) return false;
    if (*s == '#') s++;
    int v[6], n = 0;
    for (; *s; s++) {
        if (n >= 6) return false;
        int h = hexval(*s);
        if (h < 0) return false;
        v[n++] = h;
    }
    if (n == 3) *out = (col_rgb_t){ (uint8_t)(v[0] * 17), (uint8_t)(v[1] * 17), (uint8_t)(v[2] * 17) };
    else if (n == 6) *out = (col_rgb_t){ (uint8_t)(v[0] * 16 + v[1]), (uint8_t)(v[2] * 16 + v[3]), (uint8_t)(v[4] * 16 + v[5]) };
    else return false;
    return true;
}

void col_format_hex(col_rgb_t c, char *out, size_t n)
{
    snprintf(out, n, "#%02X%02X%02X", c.r, c.g, c.b);
}

static double hue_of(double r, double g, double b, double max, double d)
{
    if (d == 0) return 0;
    double h;
    if (max == r) h = fmod((g - b) / d, 6.0);
    else if (max == g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    h *= 60;
    return h < 0 ? h + 360 : h;
}

col_hsl_t col_rgb_to_hsl(col_rgb_t c)
{
    double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    double max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    double min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    double d = max - min, l = (max + min) / 2;
    double s = d == 0 ? 0 : d / (1 - fabs(2 * l - 1));
    return (col_hsl_t){ hue_of(r, g, b, max, d), s * 100, l * 100 };
}

static uint8_t byte_of(double v)
{
    double x = round(v * 255);
    return (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x);
}

col_rgb_t col_hsl_to_rgb(col_hsl_t in)
{
    double h = fmod(in.h, 360);
    if (h < 0) h += 360;
    double s = in.s / 100, l = in.l / 100;
    s = s < 0 ? 0 : s > 1 ? 1 : s;
    l = l < 0 ? 0 : l > 1 ? 1 : l;
    double c = (1 - fabs(2 * l - 1)) * s, x = c * (1 - fabs(fmod(h / 60, 2) - 1)), m = l - c / 2;
    double r, g, b;
    switch ((int)(h / 60)) {
    case 0: r = c, g = x, b = 0; break;
    case 1: r = x, g = c, b = 0; break;
    case 2: r = 0, g = c, b = x; break;
    case 3: r = 0, g = x, b = c; break;
    case 4: r = x, g = 0, b = c; break;
    default: r = c, g = 0, b = x; break;
    }
    return (col_rgb_t){ byte_of(r + m), byte_of(g + m), byte_of(b + m) };
}

col_hsv_t col_rgb_to_hsv(col_rgb_t c)
{
    double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    double max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    double min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    double d = max - min;
    return (col_hsv_t){ hue_of(r, g, b, max, d), max == 0 ? 0 : d / max * 100, max * 100 };
}

static double lin(uint8_t v)
{
    double c = v / 255.0;
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

double col_luminance(col_rgb_t c)
{
    return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}

double col_contrast(col_rgb_t a, col_rgb_t b)
{
    double la = col_luminance(a), lb = col_luminance(b);
    if (la < lb) {
        double t = la;
        la = lb;
        lb = t;
    }
    return (la + 0.05) / (lb + 0.05);
}

uint32_t col_to_u32(col_rgb_t c) { return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; }
col_rgb_t col_from_u32(uint32_t v) { return (col_rgb_t){ (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v }; }
