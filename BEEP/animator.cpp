/*
 * BEEP - animator.cpp
 * ------------------------------------------------------------------
 */
#include "animator.h"
#include <Arduino.h>
#include <math.h>

namespace beep {

static const float PI_F = 3.14159265f;
static const float CLOSED_TOP = 1.2f;   // closed-lid profile a blink settles to
static const float CLOSED_BOT = 2.2f;

static inline bool reached(uint32_t now, uint32_t at) {
  return (int32_t)(now - at) >= 0;
}

static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

static float randRange(float lo, float hi) {
  return lo + (hi - lo) * ((float)random(0, 10000) / 10000.0f);
}

static void fillEye(EyeState& e, float cx, float cy, float halfW,
                    float top, float bot, float shape, float slant, float squint,
                    bool vis, uint8_t pshape, float pupilR, float pdx, float pdy,
                    float hl, float blink, float bob) {
  e.cx = cx;
  e.cy = cy + bob;
  e.halfW = halfW;
  e.top = lerpf(top, CLOSED_TOP, blink);
  e.bot = lerpf(bot, CLOSED_BOT, blink);
  e.shape = shape;
  e.slant = slant;
  e.squint = squint * (1.0f - blink);
  e.pupilVisible = vis;
  e.pupilShape = pshape;
  e.pupilR = pupilR;
  e.pdx = pdx;
  e.pdy = pdy;
  e.hl = hl;
}

Animator::Animator() {}

void Animator::begin(uint32_t now, const FaceGeometry& geo, const AnimatorConfig& cfg) {
  geo_ = geo;
  cfg_ = cfg;

  target_ = EXPR_IDLE;
  from_ = expressionParams(EXPR_IDLE);
  to_ = from_;
  blendStart_ = now;
  hasExpire_ = false;
  expireAt_ = 0;

  blink_[0].active = false; blink_[0].startAt = 0; blink_[0].dur = 0;
  blink_[1].active = false; blink_[1].startAt = 0; blink_[1].dur = 0;
  nextBlinkAt_ = now + (uint32_t)random((long)cfg_.blinkMinMs, (long)cfg_.blinkMaxMs);

  gazeX_ = gazeY_ = gazeTX_ = gazeTY_ = 0.0f;
  nextGazeAt_ = now + (uint32_t)random((long)cfg_.gazeMinMs, (long)cfg_.gazeMaxMs);

  demo_ = false;
  nextDemoAt_ = 0;
  demoIndex_ = 0;

  startMs_ = now;
  lastUpdate_ = now;

  buildFace(to_, now, 0.0f, 0.0f);
}

void Animator::applyExpr(Expr e, uint32_t now, bool userCommand) {
  from_ = currentParams(now);
  to_ = expressionParams(e);
  target_ = e;
  blendStart_ = now;
  if (userCommand) {
    uint32_t hold = 0;
    if (expressionIsTemporary(e, hold)) {
      hasExpire_ = true;
      expireAt_ = now + hold;
    } else {
      hasExpire_ = false;
    }
  }
}

void Animator::setExpr(Expr e, uint32_t now) {
  demo_ = false;
  applyExpr(e, now, true);
}

void Animator::startDemo(uint32_t now) {
  demo_ = true;
  demoIndex_ = 0;
  from_ = currentParams(now);
  to_ = expressionParams(EXPR_IDLE);
  target_ = EXPR_IDLE;
  blendStart_ = now;
  hasExpire_ = false;
  nextDemoAt_ = now + cfg_.demoStepMs;
}

float Animator::blendEase(uint32_t now) const {
  uint32_t el = now - blendStart_;
  if (cfg_.transitionMs == 0 || (int32_t)el >= (int32_t)cfg_.transitionMs) return 1.0f;
  float t = (float)el / (float)cfg_.transitionMs;
  return t * t * (3.0f - 2.0f * t);
}

FaceParams Animator::currentParams(uint32_t now) const {
  float t = blendEase(now);
  const FaceParams& a = from_;
  const FaceParams& b = to_;
  FaceParams r;
  r.top = lerpf(a.top, b.top, t);
  r.bot = lerpf(a.bot, b.bot, t);
  r.shape = lerpf(a.shape, b.shape, t);
  r.pupil = lerpf(a.pupil, b.pupil, t);
  r.pdx = lerpf(a.pdx, b.pdx, t);
  r.pdy = lerpf(a.pdy, b.pdy, t);
  r.slant = lerpf(a.slant, b.slant, t);
  r.squint = lerpf(a.squint, b.squint, t);
  r.browY = lerpf(a.browY, b.browY, t);
  r.browTilt = lerpf(a.browTilt, b.browTilt, t);
  r.browShow = lerpf(a.browShow, b.browShow, t);
  r.hl = lerpf(a.hl, b.hl, t);
  r.mouthW = lerpf(a.mouthW, b.mouthW, t);
  r.mouthOpen = lerpf(a.mouthOpen, b.mouthOpen, t);
  r.mouthDepth = lerpf(a.mouthDepth, b.mouthDepth, t);
  r.asym = lerpf(a.asym, b.asym, t);
  r.pupilShape = (t >= 0.5f) ? b.pupilShape : a.pupilShape;
  r.mouth = (t >= 0.5f) ? b.mouth : a.mouth;
  return r;
}

void Animator::scheduleBlink(uint32_t now) {
  bool asym = (uint32_t)random(100) < cfg_.asymBlinkChance;
  bool dbl = (uint32_t)random(100) < cfg_.doubleBlinkChance;
  uint32_t dur = (uint32_t)((long)cfg_.blinkDurationMs + random(-20, 40));

  if (asym) {
    int w = (int)random(2);
    blink_[w].active = true;
    blink_[w].startAt = now;
    blink_[w].dur = dur;
  } else {
    blink_[0].active = true;
    blink_[0].startAt = now;
    blink_[0].dur = dur;
    blink_[1].active = true;
    blink_[1].startAt = now + (uint32_t)random(0, 45);
    blink_[1].dur = dur;
  }

  uint32_t wait = dbl ? (dur + 130U + (uint32_t)random(0, 90))
                      : (uint32_t)random((long)cfg_.blinkMinMs, (long)cfg_.blinkMaxMs);
  nextBlinkAt_ = now + wait;
}

float Animator::blinkAmount(int eye, uint32_t now) {
  EyeBlink& b = blink_[eye];
  if (!b.active) return 0.0f;
  uint32_t el = now - b.startAt;
  if (el >= b.dur) { b.active = false; return 0.0f; }
  float p = (float)el / (float)b.dur;
  return (p < 0.5f) ? (p * 2.0f) : ((1.0f - p) * 2.0f);
}

void Animator::update(uint32_t now) {
  uint32_t dt = now - lastUpdate_;
  lastUpdate_ = now;
  if (dt > 250U) dt = 250U;   // ignore jumps after a pause

  if (demo_ && reached(now, nextDemoAt_)) {
    demoIndex_ = (uint8_t)((demoIndex_ + 1) % (uint8_t)EXPR_COUNT);
    applyExpr((Expr)demoIndex_, now, false);
    nextDemoAt_ = now + cfg_.demoStepMs;
  } else if (hasExpire_ && reached(now, expireAt_)) {
    hasExpire_ = false;
    applyExpr(EXPR_IDLE, now, false);
  }

  if (reached(now, nextBlinkAt_)) scheduleBlink(now);
  float blinkL = blinkAmount(0, now);
  float blinkR = blinkAmount(1, now);

  if (reached(now, nextGazeAt_)) {
    gazeTX_ = randRange(-cfg_.gazeRadius, cfg_.gazeRadius);
    gazeTY_ = randRange(-cfg_.gazeRadius, cfg_.gazeRadius);
    uint32_t cad = (uint32_t)random((long)cfg_.gazeMinMs, (long)cfg_.gazeMaxMs);
    if ((uint32_t)random(100) < cfg_.quirkChance) cad = 260U;  // quick follow-up dart
    nextGazeAt_ = now + cad;
  }
  {
    float dx = gazeTX_ - gazeX_;
    float dy = gazeTY_ - gazeY_;
    float d = sqrtf(dx * dx + dy * dy);
    float step = cfg_.gazeSpeed * (float)dt / 1000.0f;
    if (d <= step || d < 0.001f) {
      gazeX_ = gazeTX_;
      gazeY_ = gazeTY_;
    } else {
      gazeX_ += dx / d * step;
      gazeY_ += dy / d * step;
    }
  }

  FaceParams p = currentParams(now);
  buildFace(p, now, blinkL, blinkR);
}

void Animator::buildFace(const FaceParams& p, uint32_t now, float blinkL, float blinkR) {
  float bob = 0.0f;
  if (cfg_.breatheAmp > 0.0f && cfg_.breathePeriodMs > 0) {
    float phase = (float)((now - startMs_) % cfg_.breathePeriodMs) / (float)cfg_.breathePeriodMs;
    bob = sinf(phase * 2.0f * PI_F) * cfg_.breatheAmp;
  }

  float cxL = geo_.screenW * 0.5f - geo_.eyeCxOffset;
  float cxR = geo_.screenW * 0.5f + geo_.eyeCxOffset;

  float asym = p.asym;
  float topL = p.top * (1.0f - 0.18f * asym);
  float topR = p.top * (1.0f + 0.18f * asym);
  float cyL = geo_.eyeCy + 2.0f * asym;
  float cyR = geo_.eyeCy - 2.0f * asym;

  float gw = 1.0f - fminf(1.0f, (fabsf(p.pdx) + fabsf(p.pdy)) / 8.0f);
  float pdx = p.pdx + gazeX_ * gw;
  float pdy = p.pdy + gazeY_ * gw;

  float pupilR = p.pupil * geo_.eyeHalfW;
  float maxX = geo_.eyeHalfW - pupilR - 1.0f;
  if (maxX < 1.0f) maxX = 1.0f;
  float verticalRoom = fminf(topL, p.bot) - pupilR - 0.5f;
  if (verticalRoom < 1.0f) verticalRoom = 1.0f;
  pdx = fmaxf(-maxX, fminf(maxX, pdx));
  pdy = fmaxf(-verticalRoom, fminf(verticalRoom, pdy));

  bool vis = p.pupil > 0.0f;

  fillEye(face_.left, cxL, cyL, geo_.eyeHalfW, topL, p.bot, p.shape, p.slant,
          p.squint, vis, p.pupilShape, pupilR, pdx, pdy, p.hl, blinkL, bob);
  fillEye(face_.right, cxR, cyR, geo_.eyeHalfW, topR, p.bot, p.shape, -p.slant,
          p.squint, vis, p.pupilShape, pupilR, pdx, pdy, p.hl, blinkR, bob);

  face_.mouth = p.mouth;
  face_.mouthCx = geo_.screenW * 0.5f;
  face_.mouthCy = geo_.mouthBaseY + bob;
  face_.mouthW = p.mouthW;
  face_.mouthOpen = p.mouthOpen;
  face_.mouthDepth = p.mouthDepth;

  face_.browShow = p.browShow;
  face_.browY = p.browY;
  face_.browTilt = p.browTilt;
  face_.browHalfW = geo_.browHalfW;
  face_.browCxL = cxL;
  face_.browCxR = cxR;
  face_.browCyL = cyL - geo_.browOffsetY;
  face_.browCyR = cyR - geo_.browOffsetY;
}

} // namespace beep
