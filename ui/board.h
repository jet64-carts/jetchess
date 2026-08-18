// carts/jetchess/ui/board.h
//
// The board on the screen: where each square is, which square a pixel is in,
// and how the whole thing is painted.
//
// Two coordinate systems meet here and they are kept strictly apart. The
// ENGINE thinks in 0x88 squares, where a1 is 0x00 and the numbers climb up the
// board. The SCREEN thinks in pixels, where y climbs down. And the board can
// be turned round, so the player always sits behind their own pieces. Every
// conversion between the two happens in this file and nowhere else.
#ifndef JETCHESS_UI_BOARD_H
#define JETCHESS_UI_BOARD_H

#include "engine/chess.h"
#include "ui/view.h"

// The top-left pixel of a square, given which way up the board is.
void jc_board_square_origin(uint8_t square, bool flipped, int *x, int *y);

// Which square a screen pixel lies in, or JC_NO_SQUARE when it is off the
// board entirely.
uint8_t jc_board_square_at(int x, int y, bool flipped);

// Where a piece's picture goes when it stands on `square`: the square's
// origin, inset so the piece is centred.
void jc_board_piece_origin(uint8_t square, bool flipped, int *x, int *y);

// Draw one piece's picture with its top-left at (x, y), as it should look
// standing on `square` — which decides the rim a black piece wears, because
// bone vanishes against a cream square (art/pieces.h). Pass JC_NO_SQUARE for a
// piece in mid-air. Masked, so the board shows through everywhere it is not.
void jc_board_draw_piece(uint8_t piece, int x, int y, uint8_t square, bool hushed);

// Paint the board into the current draw target: its frame, the sixty-four
// squares with whatever marks they wear, every piece except the ones the
// overlay is carrying, and the rank and file marks in the band around it.
void jc_board_paint(const JcView *view);

// Paint what moves: a piece under the pointer, a piece sliding home, the ring
// under the pointer, and the four pieces offered when a pawn promotes. Each
// marks its own rectangle on the canvas.
void jc_board_paint_overlay(const JcView *view, JetStill *still);

// Where the four promotion choices are drawn, so the pointer can be asked
// which one it is over. `index` runs 0..3 in the order queen, rook, bishop,
// knight. Answers false when no promotion is being offered.
bool jc_board_promotion_slot(const JcView *view, int index, int *x, int *y, int *size);

// Where the plate those four cells stand on is: its top-left corner, the size
// being JC_PROMOTE_PLATE_W by JC_PROMOTE_PLATE_H. False when no pawn is
// waiting. The geometry gate uses it to prove the card fits on the screen from
// every square a pawn can promote on.
bool jc_board_promotion_card(const JcView *view, int *x, int *y);

// The kind offered by promotion slot `index`.
uint8_t jc_board_promotion_kind(int index);

#endif // JETCHESS_UI_BOARD_H
