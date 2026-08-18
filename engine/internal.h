// carts/jetchess/engine/internal.h
//
// The engine's own shared workings: the geometry of the board and the one
// table-building call that board.c makes on attack.c's behalf. Nothing outside
// engine/ includes this — the rest of the cart sees chess.h and no more.
#ifndef JETCHESS_ENGINE_INTERNAL_H
#define JETCHESS_ENGINE_INTERNAL_H

#include "engine/portable.h"

// The eight directions a queen moves in, as 0x88 steps: the four diagonals
// first, then the four straight lines. Sliced by the four constants below, so
// a bishop, a rook and a queen all read one table.
extern const int8_t JC_DIRECTIONS[8];
#define JC_DIAGONALS_FIRST 0
#define JC_DIAGONALS 4
#define JC_STRAIGHTS_FIRST 4
#define JC_STRAIGHTS 4

// A knight's eight leaps, as 0x88 steps.
extern const int8_t JC_KNIGHT_LEAPS[8];

// The longest run of squares a slider can cross: seven, edge to edge.
#define JC_SLIDE_RANGE 7

// Build the attack tables. Called once by jc_engine_init and nowhere else.
void jc_attack_tables_init(void);

#endif // JETCHESS_ENGINE_INTERNAL_H
