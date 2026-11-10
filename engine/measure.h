/* Maths for the hardware tools: tilt from the accelerometer, sound level
 * from microphone samples, and the screen's physical scale. Plain C, no UI.
 *
 * Accelerometer input is the OS's "up" vector in panel axes, milli-g: x
 * towards the panel's right edge, y towards its bottom edge, z out of the
 * glass, with the panel in its native portrait orientation (the IMU's chip
 * axes on the Tab5, as the OS's auto-rotate uses them). Held upright in
 * portrait it reads about (0, -1000, 0). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- screen scale ------------------------------------------------------------
 * The Tab5's panel is a 5.0" 16:9 IPS, 720 x 1280. No mechanical drawing of
 * the module is published, so the default comes from the diagonal: the
 * 1468.6 px diagonal over 127.0 mm is 11.564 px/mm (0.0865 mm pitch, 293.7
 * ppi). The ruler can be calibrated against a card (85.60 mm, ISO/IEC 7810
 * ID-1) and the result is saved. */
#define MEAS_PANEL_W_PX 720
#define MEAS_PANEL_H_PX 1280
#define MEAS_PANEL_DIAG_IN 5.0
double meas_default_px_per_mm(void);
#define MEAS_CARD_W_MM 85.60
#define MEAS_CARD_H_MM 53.98
#define MEAS_PX_PER_MM_MIN 10.5
#define MEAS_PX_PER_MM_MAX 12.5

/* ---- tilt ------------------------------------------------------------------- */
typedef struct {
    float right, up, out;     /* the up vector in screen axes, milli-g */
} meas_vec_t;

/* Panel axes to the axes of the screen as shown at rotation (0, 90, 180 or
 * 270 degrees: the OS's t5_rotation_t times 90). right is towards the
 * screen's right edge as drawn, up towards its top edge, out of the glass. */
meas_vec_t meas_to_screen(int rotation_deg, float px, float py, float pz);

typedef struct {
    bool flat;               /* lying more or less flat (glass up or down) */
    float tilt_x, tilt_y;    /* flat: degrees the right edge / top edge is raised */
    float edge;              /* upright: the screen's bottom edge against the
                                horizon, degrees, -180..180, positive when the
                                right end is higher (anticlockwise turn) */
    float total;             /* flat: total tilt from level, degrees */
} meas_tilt_t;
meas_tilt_t meas_tilt(meas_vec_t v);

/* An angle a - b, wrapped to -180..180. */
float meas_angle_diff(float a, float b);
/* A low-pass step towards x: y + k (x - y). Angles wrap correctly in
 * meas_smooth_angle. */
float meas_smooth(float y, float x, float k);
float meas_smooth_angle(float y, float x, float k);

/* ---- sound ------------------------------------------------------------------ */
/* RMS of 16-bit samples in dB relative to full scale (a full-scale sine is
 * -3 dBFS). Silence is MEAS_DB_FLOOR. */
#define MEAS_DB_FLOOR (-120.0f)
float meas_dbfs(const int16_t *pcm, size_t n);

/* Running statistics over readings in dB: min, max, and the energy average
 * (Leq). */
typedef struct {
    float min, max;
    double energy;          /* sum of 10^(dB/10) */
    uint32_t count;
} meas_stats_t;
void meas_stats_reset(meas_stats_t *s);
void meas_stats_add(meas_stats_t *s, float db);
float meas_stats_leq(const meas_stats_t *s);
