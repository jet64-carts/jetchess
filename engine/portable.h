// carts/jetchess/engine/portable.h
//
// The number types, and the handful of library calls the engine uses.
//
// The engine is ordinary C. It calls no console function at all, which is what
// lets tests/jetchess build it outside the machine and put tens of millions of
// positions through it in half a second. But it does need `uint8_t`, `bool`
// and `memset`, and inside a cart those cannot be asked for the usual way: a
// `#include <stdint.h>` is stripped before the compiler sees it, because the
// jet_ API is the sandbox boundary and the console hands every unit its own
// copy of that surface instead (claude.md note #1).
//
// So both spellings are asked for at once. Inside a cart the four system
// includes vanish and jet_core.h answers; outside, on an include path with
// runtime/ on it, all five are real and say the same thing. This is the only
// file under engine/ that knows the console exists.
#ifndef JETCHESS_ENGINE_PORTABLE_H
#define JETCHESS_ENGINE_PORTABLE_H

#include "jet_core.h"

#endif // JETCHESS_ENGINE_PORTABLE_H
