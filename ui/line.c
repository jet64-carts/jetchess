// carts/jetchess/ui/line.c
//
// Building a line of text (line.h). Every call leaves the buffer terminated
// and the length honest, whether or not what was asked for fitted.
#include "line.h"

// The most characters a number can take, so a partial number is never
// written: ten digits covers every 32-bit value, and one more the terminator.
#define JC_NUMBER_MAX 11

void jc_line_clear(JcLine *line) {
	line->length = 0;
	line->text[0] = '\0';
}

void jc_line_add(JcLine *line, const char *text) {
	while (*text != '\0' && line->length < JC_LINE_MAX - 1) {
		line->text[line->length++] = *text++;
	}
	line->text[line->length] = '\0';
}

void jc_line_add_number(JcLine *line, unsigned value) {
	// Written whole or not at all. Half a number is worse than no number,
	// and the buffer is generous enough that this never happens in practice.
	if (line->length + JC_NUMBER_MAX >= JC_LINE_MAX) {
		return;
	}
	line->length += jc_uint_to_text(value, line->text + line->length);
	line->text[line->length] = '\0';
}
