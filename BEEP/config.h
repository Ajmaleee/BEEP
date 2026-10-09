/*
 * BEEP - config.h
 * ------------------------------------------------------------------
 * All hardware-specific and tunable settings live here so they can be
 * changed in ONE place when the real module is available.
 *
 * Everything below is an ASSUMPTION until verified on hardware.  See
 * README.md ("Assumptions vs verified") for the checklist.
 * ------------------------------------------------------------------
 */
#ifndef BEEP_CONFIG_H
#define BEEP_CONFIG_H

// ------------------------------------------------------------------
// Display
// ------------------------------------------------------------------
// SSD1306 resolution.  128x64 is assumed; set to 128x32 if that is
// what you bought (the face auto-centres vertically using these).
#define BEEP_SCREEN_W 128
#define BEEP_SCREEN_H 64

// I2C pins.  GPIO8/GPIO9 are common on ESP32-C3 "Super Mini" boards,
// but they are NOT guaranteed.  Change these to match your wiring.
#define BEEP_I2C_SDA 8
#define BEEP_I2C_SCL 9

// SSD1306 I2C address.  0x3C is the most common; some modules use 0x3D.
#define BEEP_I2C_ADDR 0x3C

// I2C clock.  400 kHz is safe and gives roughly 30 fps on 128x64.
#define BEEP_I2C_HZ 400000UL

// Target frame period.  ~30 fps.  Raise to ~50 ms to save power.
#define BEEP_FRAME_MS 33

// ------------------------------------------------------------------
// Face geometry (screen pixels)
// ------------------------------------------------------------------
#define BEEP_EYE_HALF_W 16.0f   // half width of one eye
#define BEEP_EYE_CX_OFFSET 24.0f// each eye distance from screen centre
#define BEEP_EYE_CY 30.0f       // vertical centre of the eyes
#define BEEP_DEFAULT_SHAPE 2.6f // superellipse exponent (2 = ellipse)
#define BEEP_BROW_OFFSET_Y 20.0f
#define BEEP_BROW_HALF_W 9.0f
#define BEEP_MOUTH_BASE_Y 53.0f

// ------------------------------------------------------------------
// Animation tuning
// ------------------------------------------------------------------
#define BEEP_TRANSITION_MS 420UL      // cross-fade between expressions
#define BEEP_BLINK_MIN_MS 1800UL      // randomised blink bounds
#define BEEP_BLINK_MAX_MS 5200UL
#define BEEP_BLINK_DURATION_MS 150UL  // one close+open
#define BEEP_BLINK_ASYMM_CHANCE 18    // % chance a blink is one-eyed
#define BEEP_BLINK_DOUBLE_CHANCE 22   // % chance of a quick double blink
#define BEEP_GAZE_MIN_MS 1400UL       // idle pupil wander cadence
#define BEEP_GAZE_MAX_MS 4200UL
#define BEEP_GAZE_RADIUS 5.5f         // max idle pupil drift (px)
#define BEEP_GAZE_SPEED 34.0f         // pupil approach speed (px/s)
#define BEEP_BREATHE_AMP 0.7f         // subtle vertical bob (px)
#define BEEP_BREATHE_PERIOD_MS 4200UL // breathing period
#define BEEP_DEMO_STEP_MS 2600UL      // dwell time per expression in demo
#define BEEP_QUIRK_CHANCE 30          // % chance a gaze change becomes a dart

#endif // BEEP_CONFIG_H
