/*
 * BEEP - renderer.cpp
 * ------------------------------------------------------------------
 * Procedural drawing of a face.  Every eye is built as a scanline
 * "aperture" bounded above and below by a superellipse profile, so a
 * single parameter set produces ovals, half-lidded eyes, anger slants
 * and closed/happy crescents.  The pupil is a shape mask clipped to
 * that aperture (a black disc/heart), plus a white glint.
 * ------------------------------------------------------------------
 */
#include "renderer.h"
#include <math.h>

namespace beep {

static const float PI_F = 3.14159265f;

// Largest number of horizontal scanlines a single eye may span.
static const int kMaxCols = 128;

// ------------------------------------------------------------------
// helpers
// ------------------------------------------------------------------
static float profile(float t, float shape) {
  float at = fabsf(t);
  if (at >= 1.0f) return 0.0f;
  float v = 1.0f - powf(at, shape);
  if (v <= 0.0f) return 0.0f;
  return powf(v, 1.0f / shape);
}

static void thickPoint(Adafruit_GFX& g, int x, int y) {
  g.drawPixel(x, y, 1);
  g.drawPixel(x + 1, y, 1);
  g.drawPixel(x, y + 1, 1);
  g.drawPixel(x + 1, y + 1, 1);
}

static bool insideHeart(float dx, float dy) {
  float u = dx * 1.05f;
  float v = -dy * 1.05f;         // y is down; flip for the heart curve
  float q = u * u + v * v - 1.0f;
  return (q * q * q - u * u * v * v * v) <= 0.0f;
}

// ------------------------------------------------------------------
// one eye
// ------------------------------------------------------------------
static void drawEye(Adafruit_GFX& g, const EyeState& e) {
  int x0 = (int)floorf(e.cx - e.halfW);
  int x1 = (int)ceilf(e.cx + e.halfW);
  int n = x1 - x0 + 1;
  if (n > kMaxCols) n = kMaxCols;

  int16_t top[kMaxCols];
  int16_t bot[kMaxCols];
  uint8_t ok[kMaxCols];

  for (int i = 0; i < n; ++i) ok[i] = 0;

  for (int ix = x0; ix <= x1; ++ix) {
    int i = ix - x0;
    if (i < 0 || i >= n) continue;

    float t = (ix - e.cx) / e.halfW;
    float prof = profile(t, e.shape);
    if (prof <= 0.0f) continue;

    float topY = e.cy - e.top * prof + e.slant * t;
    float botY = e.cy + e.bot * prof;
    if (e.squint > 0.0f) botY -= e.squint * e.bot * 0.6f * prof;
    if (botY < topY) { float mid = 0.5f * (topY + botY); topY = botY = mid; }

    top[i] = (int16_t)ceilf(topY);
    bot[i] = (int16_t)floorf(botY);
    ok[i] = 1;
    if (bot[i] >= top[i]) g.drawFastVLine(ix, top[i], bot[i] - top[i] + 1, 1);
  }

  if (!e.pupilVisible || e.pupilR <= 0.5f) return;

  float pcx = e.cx + e.pdx;
  float pcy = e.cy + e.pdy;
  float r = e.pupilR;

  int px0 = (int)floorf(pcx - r), px1 = (int)ceilf(pcx + r);
  int py0 = (int)floorf(pcy - r), py1 = (int)ceilf(pcy + r);

  for (int ix = px0; ix <= px1; ++ix) {
    int i = ix - x0;
    if (i < 0 || i >= n || !ok[i]) continue;
    for (int iy = py0; iy <= py1; ++iy) {
      if (iy < top[i] || iy > bot[i]) continue;
      float dx = (ix - pcx) / r;
      float dy = (iy - pcy) / r;
      bool in = (e.pupilShape == PUPIL_HEART) ? insideHeart(dx, dy)
                                              : (dx * dx + dy * dy) <= 1.0f;
      if (in) g.drawPixel(ix, iy, 0);
    }
  }

  if (e.hl > 0.0f) {
    float hr = fmaxf(0.8f, r * 0.30f * e.hl);
    float hx = pcx - r * 0.38f;
    float hy = pcy - r * 0.42f;
    for (int ix = (int)floorf(hx - hr); ix <= (int)ceilf(hx + hr); ++ix) {
      int i = ix - x0;
      if (i < 0 || i >= n || !ok[i]) continue;
      for (int iy = (int)floorf(hy - hr); iy <= (int)ceilf(hy + hr); ++iy) {
        if (iy < top[i] || iy > bot[i]) continue;
        if ((ix - hx) * (ix - hx) + (iy - hy) * (iy - hy) > hr * hr) continue;
        float dx = (ix - pcx) / r;
        float dy = (iy - pcy) / r;
        bool in = (e.pupilShape == PUPIL_HEART) ? insideHeart(dx, dy)
                                                : (dx * dx + dy * dy) <= 1.0f;
        if (in) g.drawPixel(ix, iy, 1);
      }
    }
  }
}

// ------------------------------------------------------------------
// brows
// ------------------------------------------------------------------
static void drawBrow(Adafruit_GFX& g, float cx, float cy, float innerSign,
                     float yOff, float tilt, float halfW) {
  float innerX = cx + innerSign * halfW;
  float outerX = cx - innerSign * halfW;
  float innerY = cy + yOff + tilt * 3.0f;
  float outerY = cy + yOff - tilt * 3.0f;
  int steps = (int)fabsf(innerX - outerX);
  if (steps < 1) return;
  for (int i = 0; i <= steps; ++i) {
    float tt = (float)i / (float)steps;
    int x = (int)lroundf(outerX + (innerX - outerX) * tt);
    int y = (int)lroundf(outerY + (innerY - outerY) * tt);
    thickPoint(g, x, y);
  }
}

// ------------------------------------------------------------------
// mouth
// ------------------------------------------------------------------
static void drawMouth(Adafruit_GFX& g, const FaceState& f) {
  float cx = f.mouthCx, cy = f.mouthCy, w = f.mouthW, d = f.mouthDepth;
  int x0 = (int)floorf(cx - w), x1 = (int)ceilf(cx + w);
  switch (f.mouth) {
    case MOUTH_NONE:
      break;
    case MOUTH_FLAT:
      g.fillRect(x0, (int)lroundf(cy), (x1 - x0) + 1, 2, 1);
      break;
    case MOUTH_SMILE:
      for (int x = x0; x <= x1; ++x) {
        float t = (x - cx) / w;
        thickPoint(g, x, (int)lroundf(cy - d * (t * t)));
      }
      break;
    case MOUTH_FROWN:
      for (int x = x0; x <= x1; ++x) {
        float t = (x - cx) / w;
        thickPoint(g, x, (int)lroundf(cy + d * (t * t)));
      }
      break;
    case MOUTH_BIGSMILE:
      for (int x = x0; x <= x1; ++x) {
        float t = (x - cx) / w;
        float topY = cy - d * (t * t);
        float span = fmaxf(1.0f, d * 0.7f * (1.0f - t * t)) * f.mouthOpen;
        float botY = cy + span;
        int y0 = (int)ceilf(topY);
        int y1 = (int)floorf(botY);
        if (y1 >= y0) g.drawFastVLine(x, y0, y1 - y0 + 1, 1);
      }
      break;
    case MOUTH_O: {
      float r = w;
      for (int ix = (int)floorf(cx - r); ix <= (int)ceilf(cx + r); ++ix) {
        for (int iy = (int)floorf(cy - r); iy <= (int)ceilf(cy + r); ++iy) {
          float dd = sqrtf((ix - cx) * (ix - cx) + (iy - cy) * (iy - cy));
          if (dd <= r && dd >= r - 2.0f) g.drawPixel(ix, iy, 1);
        }
      }
      break;
    }
    case MOUTH_WAVY:
      for (int x = x0; x <= x1; ++x) {
        float t = (x - cx) / w;
        thickPoint(g, x, (int)lroundf(cy + sinf(t * PI_F * 2.0f) * d));
      }
      break;
  }
}

// ------------------------------------------------------------------
// whole face
// ------------------------------------------------------------------
void drawFace(Adafruit_GFX& g, const FaceState& f, const FaceGeometry& geo) {
  drawEye(g, f.left);
  drawEye(g, f.right);

  if (f.browShow > 0.05f) {
    drawBrow(g, f.browCxL, f.browCyL, 1.0f, f.browY, f.browTilt, geo.browHalfW);
    drawBrow(g, f.browCxR, f.browCyR, -1.0f, f.browY, f.browTilt, geo.browHalfW);
  }

  drawMouth(g, f);
}

} // namespace beep
