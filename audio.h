// carts/jetchess/audio.h
//
// What JetChess sounds like. One call per thing that happens, so no other file
// ever names a bank slot (sfx.h holds the map).
//
// The set is deliberately quiet. A move is made forty times a game, and a
// sound you hear forty times has to be one you would not think to turn off.
#ifndef JETCHESS_AUDIO_H
#define JETCHESS_AUDIO_H

#include "game/match.h"

// The sound of the move just played. Reads the scoresheet's last entry and the
// position it led to, so the caller never has to work out which sound it was:
// a capture, a castle, a promotion, and whether it gave check, are all facts
// the match already knows.
void jc_audio_move_played(const JcMatch *match);

// A button in the panel, and a move the rules do not allow.
void jc_audio_click(void);
void jc_audio_refuse(void);

#endif // JETCHESS_AUDIO_H
