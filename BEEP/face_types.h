/*
 * BEEP - face_types.h
 * ------------------------------------------------------------------
 * Pure data types shared by the expression table, the animation engine
 * and the renderer.  No hardware, no drawing, no timing.
 *
 *   FaceParams  -> an expression (owned by expressions.cpp)
 *   EyeState    -> one fully-resolved eye (produced by the animator)
 *   FaceState   -> a fully-resolved face   (produced by the animator)
 *   FaceGeometry-> where things sit on screen (owned by config.h/.ino)
 * ------------------------------------------------------------------
 */
#ifndef BEEP_FACE_TYPES_H
#define BEEP_FACE_TYPES_H

#include <stdint.h>

namespace beep {

// The emotional vocabulary.  Add new states at the end (before COUNT)
// and add a matching row in expressions.cpp.
enum Expr : uint8_t {
  EXPR_IDLE,
  EXPR_HAPPY,
  EXPR_EXCITED,
  EXPR_CURIOUS,
  EXPR_SURPRISED,
  EXPR_SLEEPY,
  EXPR_SLEEPING,
  EXPR_SAD,
  EXPR_ANNOYED,
  EXPR_AFFECTIONATE,
  EXPR_THINKING,
  EXPR_LISTENING,
  EXPR_CONFUSED,
  EXPR_COUNT
};

enum MouthShape : uint8_t {
  MOUTH_NONE = 0,
  MOUTH_FLAT,
  MOUTH_SMILE,
  MOUTH_BIGSMILE,
  MOUTH_FROWN,
  MOUTH_O,
  MOUTH_WAVY
};

enum PupilShape : uint8_t {
  PUPIL_ROUND = 0,
  PUPIL_HEART
};

// An expression: a complete, deterministic description of a resting face.
// Sign conventions:
//   top/bot   > 0 open outward, < 0 pull inward (used for happy arcs)
//   slant     > 0 lowers the INNER corner (anger), < 0 raises it (sad)
//   pdx/pdy   > 0 right / down
struct FaceParams {
  float top;         // upper eye boundary (px)
  float bot;         // lower eye boundary (px)
  float shape;       // superellipse exponent
  float pupil;       // pupil radius / eye half width (0 = invisible)
  float pdx, pdy;    // gaze bias (px)
  float slant;       // upper-lid corner slant (px)
  float squint;      // extra lower-lid raise (0..1)
  float browY;       // brow offset from base (px, +down)
  float browTilt;    // + inner down (angry), - inner up (sad)
  float browShow;    // 0..1
  float hl;          // highlight size (0..1)
  uint8_t pupilShape;
  uint8_t mouth;
  float mouthW;      // mouth half width (px)
  float mouthOpen;   // 0..1 openness for the open-mouth shape
  float mouthDepth;  // curve depth (px)
  float asym;        // -1..1 per-eye asymmetry
};

// One eye, resolved for this exact frame.
struct EyeState {
  float cx, cy;      // centre (screen px)
  float halfW;
  float top, bot;
  float shape;
  float slant;       // signed for this eye
  float squint;
  bool  pupilVisible;
  uint8_t pupilShape;
  float pupilR;
  float pdx, pdy;
  float hl;
};

// A whole face, resolved for this exact frame.  The renderer draws this
// and nothing else, so identical state always yields an identical face.
struct FaceState {
  EyeState left;
  EyeState right;
  uint8_t mouth;
  float mouthCx, mouthCy, mouthW, mouthOpen, mouthDepth;
  float browShow, browY, browTilt;
  float browCxL, browCxR, browCyL, browCyR, browHalfW;
};

// Screen placement of the facial features.
struct FaceGeometry {
  int16_t screenW;
  int16_t screenH;
  float eyeHalfW;
  float eyeCxOffset;
  float eyeCy;
  float browOffsetY;
  float browHalfW;
  float mouthBaseY;
};

} // namespace beep

#endif // BEEP_FACE_TYPES_H
