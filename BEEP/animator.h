/*
 * BEEP - animator.h
 * ------------------------------------------------------------------
 * The animation engine.  It owns ALL time and randomness, blends
 * between expression parameter sets, and overlays involuntary life:
 * randomised blinking, idle gaze wander, breathing and demo cycling.
 *
 * It publishes a concrete FaceState every frame.  The renderer never
 * sees timing or randomness, which keeps output reproducible.
 * ------------------------------------------------------------------
 */
#ifndef BEEP_ANIMATOR_H
#define BEEP_ANIMATOR_H

#include <stdint.h>
#include "face_types.h"

namespace beep {

struct AnimatorConfig {
  uint32_t transitionMs;      // cross-fade time between expressions
  uint32_t blinkMinMs;        // randomised blink cadence
  uint32_t blinkMaxMs;
  uint32_t blinkDurationMs;   // one close+open
  uint32_t asymBlinkChance;   // % chance a blink uses one eye
  uint32_t doubleBlinkChance; // % chance of a quick second blink
  uint32_t gazeMinMs;         // idle pupil-wander cadence
  uint32_t gazeMaxMs;
  uint32_t demoStepMs;        // dwell per expression in demo
  uint32_t quirkChance;       // % chance of an extra "dart"
  float gazeRadius;           // px
  float gazeSpeed;            // px per second
  float breatheAmp;           // px
  uint32_t breathePeriodMs;
};

class Animator {
 public:
  Animator();

  void begin(uint32_t now, const FaceGeometry& geo, const AnimatorConfig& cfg);

  // User command: show an expression and respect its timing policy.
  void setExpr(Expr e, uint32_t now);

  // Demo: cycle through every expression automatically.
  void startDemo(uint32_t now);
  bool demoRunning() const { return demo_; }

  Expr currentExpr() const { return target_; }

  void update(uint32_t now);

  const FaceState& face() const { return face_; }

 private:
  struct EyeBlink { bool active; uint32_t startAt; uint32_t dur; };

  void applyExpr(Expr e, uint32_t now, bool userCommand);
  float blendEase(uint32_t now) const;
  FaceParams currentParams(uint32_t now) const;
  void scheduleBlink(uint32_t now);
  float blinkAmount(int eye, uint32_t now);
  void buildFace(const FaceParams& p, uint32_t now, float blinkL, float blinkR);

  FaceGeometry geo_;
  AnimatorConfig cfg_;

  Expr target_;
  FaceParams from_;
  FaceParams to_;
  uint32_t blendStart_;
  bool hasExpire_;
  uint32_t expireAt_;

  EyeBlink blink_[2];
  uint32_t nextBlinkAt_;

  float gazeX_, gazeY_, gazeTX_, gazeTY_;
  uint32_t nextGazeAt_;

  bool demo_;
  uint32_t nextDemoAt_;
  uint8_t demoIndex_;

  uint32_t startMs_;
  uint32_t lastUpdate_;

  FaceState face_;
};

} // namespace beep

#endif // BEEP_ANIMATOR_H
