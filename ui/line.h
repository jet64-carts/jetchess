// carts/jetchess/ui/line.h
//
// A short line of text, built up a piece at a time.
//
// The cart has no `snprintf` and wants none: every line it shows is a few
// fixed words and a number or two. This is the whole of what that needs —
// three calls, a fixed buffer, and no way to write past the end of it, so a
// line that would be too long is simply cut short instead of becoming a bug.
#ifndef JETCHESS_UI_LINE_H
#define JETCHESS_UI_LINE_H

#include "engine/chess.h"

// Long enough for the longest sentence the panel says, which is the engine's
// readout at its wordiest.
#define JC_LINE_MAX 96

typedef struct {
	char text[JC_LINE_MAX];
	int length;
} JcLine;

// An empty line, ready to be written into.
void jc_line_clear(JcLine *line);

// Add some words. Anything past the end of the buffer is dropped.
void jc_line_add(JcLine *line, const char *text);

// Add a number, in the engine's own spelling, so that every number the cart
// shows is written by one piece of code.
void jc_line_add_number(JcLine *line, unsigned value);

#endif // JETCHESS_UI_LINE_H
