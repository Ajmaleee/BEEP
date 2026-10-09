/*
 * BEEP - expressions.h
 * ------------------------------------------------------------------
 * The expression library: maps an Expr to its FaceParams, its display
 * name, and its timing policy (temporary vs persistent).
 * ------------------------------------------------------------------
 */
#ifndef BEEP_EXPRESSIONS_H
#define BEEP_EXPRESSIONS_H

#include "face_types.h"

namespace beep {

// Resting parameters for an expression.
const FaceParams& expressionParams(Expr e);

// Human-readable name ("idle", "happy", ...).
const char* expressionName(Expr e);

// Case-insensitive name -> Expr. Returns false if unknown.
bool expressionFromName(const char* name, Expr& out);

// Timing policy.  Returns true if the expression is TEMPORARY and writes
// its hold time; returns false if it is PERSISTENT (stays until changed).
bool expressionIsTemporary(Expr e, uint32_t& holdMs);

} // namespace beep

#endif // BEEP_EXPRESSIONS_H
