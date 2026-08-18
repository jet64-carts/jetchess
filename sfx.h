// carts/jetchess/sfx.h
//
// The sound bank map: which slot of data/sfx/sfx.sfx holds which sound. The
// game names sounds only through these, so re-authoring one in the Instrument
// Studio — which anybody with this cart can open and play — never needs a line
// of C to change.
//
// The bank is written by scripts/jetchess_sfx.py. Each patch is a short
// one-shot on the console's jsfxr engine, chosen to sound like wood on wood:
// the set is a quiet one, because a move you make forty times a game must
// never become a noise you want to turn off.
#ifndef JETCHESS_SFX_H
#define JETCHESS_SFX_H

#define JC_SFX_MOVE 0    // a piece set down on an empty square: a soft tock
#define JC_SFX_CAPTURE 1 // wood meeting wood, and one piece leaving the board
#define JC_SFX_CASTLE 2  // two pieces, one after the other
#define JC_SFX_CHECK 3   // a clear, short bell: your king is addressed
#define JC_SFX_PROMOTE 4 // a pawn becomes a queen
#define JC_SFX_GAME_OVER 5 // the game ends, however it ended
#define JC_SFX_CLICK 6   // a button in the panel
#define JC_SFX_REFUSE 7  // that move is not legal

#define JC_SFX_COUNT 8

#endif // JETCHESS_SFX_H
