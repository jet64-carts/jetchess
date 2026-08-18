// carts/jetchess/ui/type.h
//
// The type ladder, and the one way JetChess sets a word.
//
// Three rules live here, and they are the reason this is a module rather than
// a couple of calls at each site:
//
//   A RUNG IS FOUR NUMBERS, not one. Which face, how tall that face is asked
//   to be, how tall its capitals actually come out, and how far below the
//   printed y its ink begins. Those last two were MEASURED by
//   tests/jetchess/visual.c and are written down in config.h; a caller that
//   carries only "which face" cannot space anything by its ink and will
//   quietly space it by its cell instead.
//
//   TEXT IS POSITIONED BY ITS INK. A glyph cell is as tall as the face's line
//   height and its letter is not, so a column of five faces spaced by cells
//   reads as five separate decisions. `jc_print_ink` prints so the first row
//   of INK lands exactly on the y you asked for.
//
//   A FACE CHOOSES ITS OWN VOICE. Four of the five faces are sprite fonts, and
//   a sprite font carries the colours its bake put into it — `TextFormat.color`
//   means nothing to one. `jc_rung_format` is therefore the one place a colour
//   is turned into the recolour table that prints it (ui/ink.h), which is what
//   makes "set this in the accent" mean the same thing everywhere.
//
// FIVE FACES, NINE NAMES. The ladder below names a rung for each job rather
// than for each height, and three of those jobs share the statement face while
// three more share the instrument's. That is not a duplicate rung: a rung used
// to be a (face, size) pair that the gate identified by its numbers, and a
// sprite face has exactly one cap height and one ink offset, so the numbers
// now belong to the FACE and the names belong to the jobs. The gate measures
// the four faces; the names are how the panel says which one it means.
#ifndef JETCHESS_UI_TYPE_H
#define JETCHESS_UI_TYPE_H

#include "config.h"
#include "jet64.h"
#include "jet_text.h"

// One rung of the ladder. Passing all four together is what lets every caller
// position by ink and mean it.
typedef struct {
	uint8_t font;
	uint8_t size;
	uint8_t cap;
	uint8_t ink;
} JcRung;

// A rung set in one of the four baked faces. `size` is the face's own cell:
// a sprite font ignores it when it draws (it has one native size), and stating
// it keeps `jet_text_char_width` and the layout arithmetic around a call site
// reading the same number the face actually is.
#define JC_FACE(NAME)                                                                              \
	((JcRung){JET_FONT_SPRITE(JC_SFONT_##NAME), JC_SFONT_##NAME##_CELL, JC_CAP_##NAME,             \
	    JC_INK_##NAME})

// The ladder itself, one name per job.
//
// The logotype, and the only rung still set from the Font ROM: a wordmark is
// allowed a voice of its own, and this one is the console's.
//
// It was tried in the statement face, tracked, when the rest of the cart moved
// to Fontin — one family throughout, and no pixel face left on the screen. It
// is not worth having, and the reason is structural rather than a matter of
// taste: a sprite face has ONE size, and the only one big enough to carry a
// logotype is the size "White to move" is set at, so the mark and the line
// under it came out the same height and the mark stopped reading as a mark. A
// logotype set in a different family from the text under it is ordinary
// typography; a logotype the same size as the heading below it is a mistake.
#define JC_RUNG_WORDMARK ((JcRung){JC_FONT_MARK, JC_TYPE_MARK, JC_CAP_MARK, JC_INK_MARK})
// The statement face says the three things this cart says out loud: what the
// game is waiting for, how it ended, and what you may press.
#define JC_RUNG_STATUS JC_FACE(STATEMENT)
#define JC_RUNG_RESULT JC_FACE(STATEMENT)
#define JC_RUNG_ACTION JC_FACE(STATEMENT)
// The text face reads: the opening's name, the material figure.
#define JC_RUNG_VALUE JC_FACE(TEXT)
// The instrument face is the machine's own hand — the scoresheet it writes,
// the setting it is playing at, and the footnote it thinks aloud in.
#define JC_RUNG_MOVE JC_FACE(INSTRUMENT)
#define JC_RUNG_LEVEL JC_FACE(INSTRUMENT)
#define JC_RUNG_READOUT JC_FACE(INSTRUMENT)
// The label face heads a block, heads a column, and marks a board's edge.
#define JC_RUNG_LABEL JC_FACE(LABEL)
#define JC_RUNG_COORD JC_RUNG_LABEL

// Register the four sprite faces and build every voice they speak in. Called
// once from `_init`, unconditionally: a font slot is guest-side state and a
// guest's statics are new on every spawn, warm boot included.
void jc_type_init(void);

// A rung as the text engine wants it, with a letterspacing a caller may want
// to open a small capital out by, and the voice the colour asks for.
TextFormat jc_rung_format(JcRung rung, uint8_t colour, int letterspacing);

// Print so the text's INK begins at `ink_y`: from `x`, ending at `right`, or
// centred on `centre_x`.
void jc_print_ink(const char *text, int x, int ink_y, JcRung rung, uint8_t colour);
void jc_print_ink_right(const char *text, int right, int ink_y, JcRung rung, uint8_t colour);
// ...and centred on `centre_x`, with a tracking, because what this cart centres
// wants two different ones. The word inside a control is a small capital and is
// opened out by a pixel, because a control is already boxed; a line of running
// text and a single rank or file mark are both set solid, the one because it is
// a sentence and the other because one letter has nothing to be tracked against.
void jc_print_ink_centre(const char *text, int centre_x, int ink_y, JcRung rung, uint8_t colour,
                         int letterspacing);

#endif // JETCHESS_UI_TYPE_H
