// carts/jetchess/ui/panel.h
//
// The column beside the board: what the game is called, what is happening,
// who is ahead, the scoresheet, what the engine is doing, and four buttons.
//
// It is laid out as a stack of blocks whose heights are named in config.h, so
// moving one block moves everything below it and nothing has to be measured
// twice.
#ifndef JETCHESS_UI_PANEL_H
#define JETCHESS_UI_PANEL_H

#include "ui/view.h"

// The three buttons along the bottom, in the order they are drawn. All three
// are VERBS. The opponent's strength is a value, so it lives in the ENGINE
// block as a setting and not here as a fourth thing to press.
typedef enum {
	JC_CHIP_NEW = 0,
	JC_CHIP_BACK,
	JC_CHIP_FLIP,
} JcChip;

// Paint the whole panel into the current draw target.
void jc_panel_paint(const JcView *view);

// Paint the parts that move every frame: the engine's readout while it is
// thinking, and whichever button the pointer is over. Each marks itself.
void jc_panel_paint_overlay(const JcView *view, JetStill *still);

// Where one button is.
void jc_panel_chip_rect(int index, int *x, int *y, int *w, int *h);

// Which button a pixel is over, or -1 for none.
int jc_panel_chip_at(int x, int y);

// Where the level control is — the setting at the end of the ENGINE block's
// label line, which cycles the opponent's strength when it is clicked.
void jc_panel_level_rect(int *x, int *y, int *w, int *h);

#endif // JETCHESS_UI_PANEL_H
