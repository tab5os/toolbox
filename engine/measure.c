#include "measure.h"
#include <math.h>

#define RAD2DEG (180.0f / 3.14159265f)

double meas_default_px_per_mm(void)
{
    double diag_px = sqrt((double)MEAS_PANEL_W_PX * MEAS_PANEL_W_PX + (double)MEAS_PANEL_H_PX * MEAS_PANEL_H_PX);
    return diag_px / (MEAS_PANEL_DIAG_IN * 25.4);
}

meas_vec_t meas_to_screen(int rot, float px, float py, float pz)
{
    /* The screen's up direction in panel axes, for each rotation. Rotation
     * 90 is the panel's right edge up (t5_ui orient.c). */
    float ux, uy;
    switch (((rot % 360) + 360) % 360) {
    case 90: ux = 1, uy = 0; break;
    case 180: ux = 0, uy = 1; break;
    case 270: ux = -1, uy = 0; break;
    default: ux = 0, uy = -1; break;
    }
    float rx = -uy, ry = ux;                 /* screen right: up turned clockwise */
    return (meas_vec_t){ px * rx + py * ry, px * ux + py * uy, pz };
}

meas_tilt_t meas_tilt(meas_vec_t v)
{
    meas_tilt_t t;
    float plane = sqrtf(v.right * v.right + v.up * v.up);
    float out = fabsf(v.out);
    float mag = sqrtf(plane * plane + out * out);
    t.flat = out >= plane;                   /* within 45 degrees of flat */
    t.tilt_x = atan2f(v.right, out) * RAD2DEG;
    t.tilt_y = atan2f(v.up, out) * RAD2DEG;
    t.total = mag > 0 ? acosf(out / mag > 1 ? 1 : out / mag) * RAD2DEG : 0;
    t.edge = atan2f(v.right, v.up) * RAD2DEG;
    return t;
}

float meas_angle_diff(float a, float b)
{
    float d = fmodf(a - b, 360.0f);
    if (d > 180) d -= 360;
    if (d <= -180) d += 360;
    return d;
}

float meas_smooth(float y, float x, float k)
{
    return y + k * (x - y);
}

float meas_smooth_angle(float y, float x, float k)
{
    float r = y + k * meas_angle_diff(x, y);
    return meas_angle_diff(r, 0);
}

float meas_dbfs(const int16_t *pcm, size_t n)
{
    if (!pcm || n == 0) return MEAS_DB_FLOOR;
    double sum = 0, mean = 0;
    for (size_t i = 0; i < n; i++) mean += pcm[i];
    mean /= (double)n;                        /* drop any DC offset */
    for (size_t i = 0; i < n; i++) {
        double x = pcm[i] - mean;
        sum += x * x;
    }
    double rms = sqrt(sum / (double)n);
    if (rms < 1e-6) return MEAS_DB_FLOOR;
    float db = (float)(20.0 * log10(rms / 32768.0));
    return db < MEAS_DB_FLOOR ? MEAS_DB_FLOOR : db;
}

void meas_stats_reset(meas_stats_t *s)
{
    s->min = 1e9f;
    s->max = -1e9f;
    s->energy = 0;
    s->count = 0;
}

void meas_stats_add(meas_stats_t *s, float db)
{
    if (db < s->min) s->min = db;
    if (db > s->max) s->max = db;
    s->energy += pow(10.0, db / 10.0);
    s->count++;
}

float meas_stats_leq(const meas_stats_t *s)
{
    if (!s->count || s->energy <= 0) return MEAS_DB_FLOOR;
    return (float)(10.0 * log10(s->energy / s->count));
}
