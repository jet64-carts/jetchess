// carts/jetchess/ui/ink.h
//
// One baked face, every colour it speaks in.
//
// A sprite font carries the colours its bake put into it, so `TextFormat.color`
// means nothing to one (`docs/sprite_fonts/design.md` §2.6). What answers back
// is `TextFormat.palette_map`: a 256-entry table every glyph pixel is drawn
// through. `jet_palette_ink_map` builds one from the two colours the bake
// declared — which are `data/gfx/fonts.ron`'s `blend_background` and
// `fill_top`, named here as JC_INK_RAMP_FOOT and JC_INK_RAMP_HEAD — by
// projecting each entry onto that ramp and re-hueing it towards the new ink at
// the position it already held. So the soft anti-aliased edge stays a graded
// run and the outline ring stays the colour it was baked in, which is the
// whole reason the faces are worth baking that way.
//
// A TABLE, NOT A STAMP. Painting one colour over every visible pixel is what
// the drop shadow does; it would flatten both the edge and the ring, and the
// cart would be back to the hard bitmap type it just left.
//
// EVERY VOICE IS BUILT ONCE, AT BOOT. Building one searches the whole palette
// for every entry, so the five of them are five searches at `_init` and no
// work at all in a frame. They are plain data and the tables outlive every
// call that points at them, which is what `jet_print_ext` requires.
#ifndef JETCHESS_UI_INK_H
#define JETCHESS_UI_INK_H

#include "jet64.h"

// Every colour JetChess sets a word in, and the only colours it can. The list
// is public because the type gate walks it: a specimen of each voice, each
// measured against the ground it is read on (tests/jetchess/visual.c).
extern const uint8_t JC_INK_VOICE[];
extern const int JC_INK_VOICES;

// Build a table for each voice. Call once in `_init`, after the palette is
// live — which is only true once the cart has set it itself. The palette a
// machine boots with is the host's to choose (on a Workstation rebuild, its own
// live palette), and `jet_load_palette` lands a tick or more after it returns,
// so main.c reads the cart's palette file and sets every entry first.
void jc_ink_init(void);

// The table that prints in `colour`, or NULL for a colour no voice was built
// for — which draws the face in the colours it was baked in, so an undeclared
// ink is a thing you can see rather than a thing you cannot.
const uint8_t *jc_ink_map(uint8_t colour);

#endif // JETCHESS_UI_INK_H
