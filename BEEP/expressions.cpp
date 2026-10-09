/*
 * BEEP - expressions.cpp
 * ------------------------------------------------------------------
 * Adding a new expression is a three-line job:
 *   1. add the enum value in face_types.h (before EXPR_COUNT)
 *   2. add a row to kParams below
 *   3. add its name + hold time to kInfo below
 * The renderer never needs to change.
 *
 * Row order matches FaceParams exactly:
 *   top, bot, shape, pupil, pdx, pdy, slant, squint,
 *   browY, browTilt, browShow, hl, pupilShape, mouth,
 *   mouthW, mouthOpen, mouthDepth, asym
 * ------------------------------------------------------------------
 */
#include "expressions.h"
#include <string.h>
#include <ctype.h>

namespace beep {

static const FaceParams kParams[EXPR_COUNT] = {
  /* IDLE        */ {17.0f, 15.0f, 2.6f, 0.42f,  0.0f,  0.0f,  0.0f, 0.00f,  0.0f,  0.0f, 0.0f, 0.70f, PUPIL_ROUND, MOUTH_NONE,     13.0f, 0.0f, 4.0f,  0.0f},
  /* HAPPY       */ {15.0f, 11.0f, 2.6f, 0.44f,  0.0f,  0.0f,  0.0f, 0.15f,  0.0f,  0.0f, 0.0f, 0.80f, PUPIL_ROUND, MOUTH_SMILE,    13.0f, 0.0f, 5.0f,  0.0f},
  /* EXCITED     */ {19.0f, 17.0f, 2.6f, 0.50f,  0.0f, -1.0f,  0.0f, 0.00f,  0.0f,  0.0f, 0.0f, 1.00f, PUPIL_ROUND, MOUTH_BIGSMILE, 16.0f, 1.0f, 7.0f,  0.0f},
  /* CURIOUS     */ {17.0f, 15.0f, 2.6f, 0.42f,  4.0f, -2.0f,  0.0f, 0.00f,  0.0f, -0.4f, 0.5f, 0.80f, PUPIL_ROUND, MOUTH_O,         3.5f, 0.0f, 3.0f,  0.6f},
  /* SURPRISED   */ {21.0f, 19.0f, 2.6f, 0.28f,  0.0f,  0.0f,  0.0f, 0.00f, -3.0f, -0.2f, 1.0f, 0.90f, PUPIL_ROUND, MOUTH_O,         5.0f, 0.0f, 5.0f,  0.0f},
  /* SLEEPY      */ { 9.0f,  8.0f, 2.6f, 0.40f,  0.0f,  2.0f,  1.5f, 0.10f,  0.0f,  0.0f, 0.0f, 0.50f, PUPIL_ROUND, MOUTH_FLAT,      8.0f, 0.0f, 4.0f,  0.0f},
  /* SLEEPING    */ { 1.4f,  2.6f, 2.6f, 0.00f,  0.0f,  0.0f,  0.0f, 0.00f,  0.0f,  0.0f, 0.0f, 0.00f, PUPIL_ROUND, MOUTH_NONE,     13.0f, 0.0f, 4.0f,  0.0f},
  /* SAD         */ {12.0f, 12.0f, 2.6f, 0.40f,  0.0f,  2.0f, -3.0f, 0.00f,  1.0f, -1.0f, 1.0f, 0.60f, PUPIL_ROUND, MOUTH_FROWN,     9.0f, 0.0f, 3.0f,  0.0f},
  /* ANNOYED     */ { 9.0f, 13.0f, 2.6f, 0.34f, -1.0f,  1.0f,  5.0f, 0.00f,  2.0f,  1.0f, 1.0f, 0.50f, PUPIL_ROUND, MOUTH_FROWN,     8.0f, 0.0f, 2.0f,  0.0f},
  /* AFFECTIONATE*/ {14.0f, 13.0f, 2.6f, 0.58f,  0.0f,  0.0f,  0.0f, 0.20f,  0.0f,  0.0f, 0.0f, 0.90f, PUPIL_HEART, MOUTH_SMILE,    11.0f, 0.0f, 4.0f,  0.0f},
  /* THINKING    */ {13.0f, 13.0f, 2.6f, 0.42f,  5.0f, -4.0f,  0.0f, 0.00f, -1.0f, -0.6f, 0.7f, 0.70f, PUPIL_ROUND, MOUTH_FLAT,      5.0f, 0.0f, 4.0f,  0.4f},
  /* LISTENING   */ {18.0f, 16.0f, 2.6f, 0.46f,  0.0f, -0.5f,  0.0f, 0.00f, -1.5f, -0.2f, 0.6f, 0.80f, PUPIL_ROUND, MOUTH_O,         3.0f, 0.0f, 2.5f,  0.0f},
  /* CONFUSED    */ {15.0f, 15.0f, 2.6f, 0.40f,  3.0f, -2.0f,  0.0f, 0.00f,  0.0f, -0.5f, 1.0f, 0.70f, PUPIL_ROUND, MOUTH_WAVY,     12.0f, 0.0f, 2.0f,  0.8f},
};

// Per-expression timing: 0 = persistent, otherwise temporary hold (ms).
static const uint32_t kHold[EXPR_COUNT] = {
  0,      // IDLE
  2600,   // HAPPY
  3000,   // EXCITED
  2600,   // CURIOUS
  1500,   // SURPRISED
  4000,   // SLEEPY
  0,      // SLEEPING   (persistent)
  4000,   // SAD
  3200,   // ANNOYED
  3600,   // AFFECTIONATE
  4500,   // THINKING
  0,      // LISTENING  (persistent)
  2600,   // CONFUSED
};

static const char* kNames[EXPR_COUNT] = {
  "idle", "happy", "excited", "curious", "surprised", "sleepy",
  "sleeping", "sad", "annoyed", "affectionate", "thinking",
  "listening", "confused"
};

const FaceParams& expressionParams(Expr e) {
  if (e >= EXPR_COUNT) e = EXPR_IDLE;
  return kParams[e];
}

const char* expressionName(Expr e) {
  if (e >= EXPR_COUNT) e = EXPR_IDLE;
  return kNames[e];
}

bool expressionIsTemporary(Expr e, uint32_t& holdMs) {
  if (e >= EXPR_COUNT) e = EXPR_IDLE;
  holdMs = kHold[e];
  return holdMs > 0;
}

static bool iequals(const char* a, const char* b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    ++a; ++b;
  }
  return *a == *b;
}

bool expressionFromName(const char* name, Expr& out) {
  for (uint8_t i = 0; i < EXPR_COUNT; ++i) {
    if (iequals(name, kNames[i])) { out = (Expr)i; return true; }
  }
  return false;
}

} // namespace beep
