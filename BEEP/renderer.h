/*
 * BEEP - renderer.h
 * ------------------------------------------------------------------
 * Deterministic drawing of a FaceState.  The renderer knows nothing
 * about time, emotion or random numbers - give it the same FaceState
 * twice and it draws the same face twice.
 * ------------------------------------------------------------------
 */
#ifndef BEEP_RENDERER_H
#define BEEP_RENDERER_H

#include <Adafruit_GFX.h>
#include "face_types.h"

namespace beep {

void drawFace(Adafruit_GFX& gfx, const FaceState& face, const FaceGeometry& geo);

} // namespace beep

#endif // BEEP_RENDERER_H
