// carts/jetchess/config.h
//
// Every number JetChess uses, named once. Where a value is a matter of taste
// the comment says what taste; where it is a matter of arithmetic the comment
// says the sum.
//
// THE LAYOUT IS A GRID, and it has exactly one spacing in it: forty pixels of
// margin round the screen and between the board and the panel. Inside the
// panel there is one gap between blocks and one between a label and the thing
// it heads, and the column of block positions is COMPUTED from the two — so
// moving a face's cap height moves everything below it and no number in this
// file is ever typed twice.
//
// THE TYPE IS A LADDER of five steps in one family, four of them baked into
// sprite cells at one height each (data/gfx/fonts.ron) and the fifth the
// logotype's. A baked face has exactly one size, which is what ended a whole
// class of fault: a bitmap face drawn at a fraction of its native height drops
// different rows out of different glyphs, and an `m` whose counters have
// closed is the loudest possible sign that nobody looked.
#ifndef JETCHESS_CONFIG_H
#define JETCHESS_CONFIG_H

// ---------------------------------------------------------------------------
// The screen
// ---------------------------------------------------------------------------

#define JC_SCREEN_W 1280
#define JC_SCREEN_H 720

// ---------------------------------------------------------------------------
// The board
// ---------------------------------------------------------------------------

#define JC_FILES 8
#define JC_RANKS 8

// The one spacing in the whole layout: the margin round the screen, and the
// gap between the board and the panel.
#define JC_GUTTER 40

// One square. Eighty pixels leaves a sixty-four pixel piece an eight pixel
// margin all round, which is the proportion a real set stands at.
#define JC_SQUARE_PX 80
#define JC_BOARD_PX (JC_SQUARE_PX * JC_FILES) // 640

#define JC_BOARD_X JC_GUTTER
#define JC_BOARD_Y ((JC_SCREEN_H - JC_BOARD_PX) / 2) // 40, which is the gutter again

// The hairline around the board: one neutral pixel, a pixel outside the
// squares. Enough to say where the board ends on a near-black ground, and not
// enough to become a frame the eye has to look past — and NOT the accent,
// which has exactly one job and this is not it.
#define JC_BOARD_FRAME_PX 1

// The contrast every piece of text in this cart clears against the ground it
// lands on, wherever that ground's own luminance allows it. WCAG's figure for
// body text, and the gate measures it rather than taking it on trust.
#define JC_CONTRAST_FLOOR 4.5f

// The coordinates are written OUTSIDE the board, in the band between its frame
// and the edge of the screen: the rank digits down the left-hand side, the
// file letters along the bottom, each one centred on the square it names. They
// are set on the LABEL rung (`JC_RUNG_COORD`, ui/type.h) and not on a rung of
// their own: a duplicate rung is a rung the gate cannot reach.
//
// THEY USED TO BE INSIDE, in the corners of the edge squares, and that put a
// word on the one ground in the cart the type was not baked for. A coordinate
// there landed on EIGHT grounds — two square colours, each plain or carrying
// the last move's tint, and both of those again on a quiet board — and no one
// ink cleared the contrast floor on all eight, so there were four of them and
// three grounds on which the floor could not be reached by any ink at all.
// Out here there is one ground, the room, and the room never changes.
//
// THE BAND IS THE MARGIN THE BOARD ALREADY HAD, and it could not be anything
// else: the screen is seven hundred and twenty pixels tall and the board with
// its two margins is exactly that, so there is no gutter left to carve out of
// anything. That is not a compromise. The printed border of a chess diagram is
// precisely where its letters go, and a mark out there is a mark the pieces
// never have to share a square with.
//
// It measures the same on both axes because JC_BOARD_Y is the gutter again,
// and the assert at the foot of this file is what keeps that true.
#define JC_COORD_BAND (JC_BOARD_X - JC_BOARD_FRAME_PX) // 39: the screen's edge to the frame

// Where a mark stands in it: a rank digit centred across the left-hand band, a
// file letter centred down the bottom one — and where a placement is vertical
// it is made by the CAP, as every vertical placement in this cart is.
#define JC_COORD_RANK_X (JC_COORD_BAND / 2)
#define JC_COORD_FILE_INK_Y                                                                        \
	(JC_BOARD_Y + JC_BOARD_PX + JC_BOARD_FRAME_PX + (JC_COORD_BAND - JC_CAP_LABEL) / 2)

// ---------------------------------------------------------------------------
// The pieces
// ---------------------------------------------------------------------------

// One sprite cell, matching the bake in scripts/jetchess_art.py.
#define JC_PIECE_PX 64
#define JC_PIECE_INSET ((JC_SQUARE_PX - JC_PIECE_PX) / 2) // 8

// How far a lifted piece floats above the board.
#define JC_LIFT_PX 8

// The marks a square can wear.
//
// A dot says "you may move here" and a ring says "and there is something here
// to take". Both are the ACCENT, keylined in ink: the keyline is what lets one
// colour read on both squares, because the shape's edge then has ten to one
// against its own fill rather than one and a third against the board.
// Given as a WIDTH and not a radius: a filled circle from an integer centre
// covers an odd number of pixels, and an eighty-pixel square's centre falls
// between two of them, so every disc on the board was half a pixel out. Twenty
// pixels of accent inside two of ink is twenty-four, which centres exactly.
#define JC_HINT_DOT_PX 20
#define JC_HINT_KEYLINE_PX 2
#define JC_HINT_RING_R (JC_SQUARE_PX / 2 - 6)
#define JC_HINT_RING_PX 3

// THE RING. There is one ring shape on the board and every ring is it: three
// pixels thick, inset three, hairlined in ink on both edges. The pointer's
// ring and a checked king's ring are the same shape in different colours, so a
// player learns one mark once instead of three. Check asks for an answer;
// checkmate IS one, so a mated king's square is filled outright instead and
// the rest of the board goes quiet (art/palette.h, JC_LIGHT_QUIET).
#define JC_RING_PX 3      // the pointer's, and a checked king's
// ...and the square the piece came FROM. TWO and not one: a single pixel of
// accent between two of ink is a dark hairline with a gold thread in it, and
// what arrives at the eye is the ink. Two against two is gold.
#define JC_RING_HAND_PX 2
#define JC_RING_INSET 3

// What a lifted piece casts on the ground: an ellipse under its base, wider
// than the base and dense enough to read as contact rather than as texture. An
// offset copy of the sprite would read as a second piece standing behind the
// first, which is why it is an ellipse.
#define JC_LIFT_SHADOW_GROW 10
#define JC_LIFT_SHADOW_H 9

// ---------------------------------------------------------------------------
// The panel
// ---------------------------------------------------------------------------
//
// The panel's box is exactly the board's box: both run from the board frame's
// top to its bottom, and the margin outside each is the same. Two dominant
// blocks that disagree by a pixel is the one misalignment an eye always finds.

#define JC_PANEL_X (JC_BOARD_X + JC_BOARD_PX + JC_GUTTER)  // 720
#define JC_PANEL_Y (JC_BOARD_Y - JC_BOARD_FRAME_PX)        // 39
#define JC_PANEL_H (JC_BOARD_PX + 2 * JC_BOARD_FRAME_PX)   // 642
#define JC_PANEL_W (JC_SCREEN_W - JC_PANEL_X - JC_PANEL_Y) // 521: the board's own margin, mirrored
#define JC_PANEL_PAD 24

// The panel's left edge and its usable width. Every block inside it starts
// here, and a line of type that does not fit JC_BLOCK_W does not fit.
#define JC_BLOCK_X (JC_PANEL_X + JC_PANEL_PAD)
#define JC_BLOCK_W (JC_PANEL_W - 2 * JC_PANEL_PAD)
#define JC_BLOCK_RIGHT (JC_BLOCK_X + JC_BLOCK_W)

// The spacings inside the panel, and they are measured between INK rather than
// between boxes — which is the only way a column of five faces at five sizes
// reads as one rhythm.
#define JC_SECTION_GAP 24 // one block's ink bottom to the next block's label ink
#define JC_LABEL_DROP 10  // a label's ink bottom to its content's ink top
#define JC_ACTION_GAP 24  // ...and the same again before the row of verbs
#define JC_RULE_GAP 18    // the clear air above and below the wordmark's rule
#define JC_STRIP_GAP 9    // the material figure's ink to the spare pieces under it

// ---------------------------------------------------------------------------
// The type
// ---------------------------------------------------------------------------
//
// FIVE STEPS, ONE FAMILY. Four of them are cuts of Fontin baked into sprite
// bank 0 by the Sprite Font Lab (data/gfx/fonts.ron): proportional, spaced by
// the letterform, and anti-aliased against the exact colour they are read on,
// which is the nearest thing an indexed machine has to a real typeface. The
// fifth is the logotype, and it alone is still the Font ROM's: a wordmark is
// allowed a voice of its own, and Press Start 2P is the console's.
//
// The cart used to set nine rungs in four unrelated bitmap faces at whatever
// heights the Font ROM happened to bake. Every one of them was monospaced, the
// small ones were chunky and the light ones were spindly — the exact two
// faults a reader notices first and cannot name.
//
// A FACE IS FOUR NUMBERS, not one. Which slot the runtime knows it by, where
// its glyph cells sit in the sheet, how big a cell is, and the ink height it
// was cut at — that last one because the word space is a quarter of the em and
// the em of a bitmap face is the height its letters were drawn at, not the box
// they were dropped into. ui/type.h registers each face from exactly these.
//
// WHERE THE CELLS LAND, which is the one thing about the bake that this file
// has to agree with. An index is a whole-sheet cell index in that face's own
// grid — 1024 pixels of sheet divided by the cell — and it is the number the
// runtime, the Sprite Editor and `jet_sprite_font_set` all address by. The
// pieces fill 64-pixel cells 0..95, which is rows 0 to 383 of bank 0; the four
// faces are packed in order below that and none of them reaches the bank's own
// floor at row 1024:
//
//   label       16 px cells, 64 a row, index 1536 -> rows 384..415
//   instrument  24 px cells, 42 a row, index  756 -> rows 432..503
//   text        32 px cells, 32 a row, index  512 -> rows 512..607
//   statement   32 px cells, 32 a row, index  608 -> rows 608..703
//
// They are PACKED, so no one of them moves on its own: an overlap is glyphs
// drawn over glyphs, and the gate that would catch it is the one that counts
// blank cells (tests/jetchess/visual.c), which sees a collision only when it
// happens to erase a whole letter. This table lives here rather than beside
// the recipe it describes because data/gfx/fonts.ron is written by the Sprite
// Font Lab, and a tool that serialises a file cannot keep a comment in it.
#define JC_SFONT_LABEL 0
#define JC_SFONT_LABEL_INDEX 1536
#define JC_SFONT_LABEL_CELL 16
#define JC_SFONT_LABEL_EM 13
#define JC_SFONT_INSTRUMENT 1
#define JC_SFONT_INSTRUMENT_INDEX 756
#define JC_SFONT_INSTRUMENT_CELL 24
#define JC_SFONT_INSTRUMENT_EM 16
#define JC_SFONT_TEXT 2
#define JC_SFONT_TEXT_INDEX 512
#define JC_SFONT_TEXT_CELL 32
#define JC_SFONT_TEXT_EM 21
#define JC_SFONT_STATEMENT 3
#define JC_SFONT_STATEMENT_INDEX 608
#define JC_SFONT_STATEMENT_CELL 32
#define JC_SFONT_STATEMENT_EM 27

// The Font ROM bank the logotype is set from, and the height it is set at.
// It is the only rung left on the ROM, which is why data/project.ron now names
// one face of its own where it used to name four. (Bank 3 still holds one, and
// not by this cart's choice: the platform reserves it for the workstation's
// terminal console and fills it back in — data/FONT-NOTICE.md says so.)
#define JC_FONT_MARK 0
#define JC_TYPE_MARK 32

// EVERY CAP AND INK BELOW WAS MEASURED, not chosen. tests/jetchess/visual.c
// registers each face, walks the ink of one capital, and reports two things:
// how tall that capital stands, and how far below the printed y its ink
// begins. The gate then asserts these constants against that measurement, so
// the layout and the bake can never drift apart — change a px_height in
// fonts.ron and the build fails with the new number in hand.
//
// A BLOCK IS SPACED BY THE CAP, because a glyph box is as tall as its cell and
// its letter is not; a column spaced by boxes reads as a column spaced by
// accident. The five caps come out 9, 11, 14, 18, 28 — a modular scale a shade
// wider than a perfect fourth, which is why no two of them are ever mistaken
// for each other.
#define JC_CAP_LABEL 9
#define JC_INK_LABEL 2
#define JC_CAP_INSTRUMENT 11
#define JC_INK_INSTRUMENT 5
#define JC_CAP_TEXT 14
#define JC_INK_TEXT 7
#define JC_CAP_STATEMENT 18
#define JC_INK_STATEMENT 5
#define JC_CAP_MARK 28
#define JC_INK_MARK 0

// THE TWO COLOURS THE BAKE GRADED BETWEEN. Every face is rasterised over
// JC_PANEL, the entry the panel's own field wears, and each edge pixel is
// blended towards it (data/gfx/fonts.ron, `blend_background`): that ramp,
// spent in palette entries, is the whole of the anti-aliasing on a machine
// with no alpha channel. The other end is `fill_top`, the solid ink a glyph's
// middle is, and ui/ink.c re-hues the run between them into every voice the
// cart owns — which is why one bake serves nine colours.
#define JC_INK_RAMP_FOOT JC_PANEL
#define JC_INK_RAMP_HEAD JC_TEXT

// ...AND THE RING THEY WEAR. The Lab grows an outline of that same backdrop
// colour around every glyph — the halo that keeps a face legible over a ground
// it was not baked for. One pixel, on all four faces (data/gfx/fonts.ron,
// `outline_px`), and it earns its place inside the panel alone: the panel sets
// words on THREE fields — its own, a well cut into it, and a control lit under
// the pointer — and a ring in the field's own colour is what lets one bake
// read on all three.
//
// IT IS DECLARED HERE BECAUSE THE RUNTIME HAS TO BE TOLD. Given a ring's
// width, every advance is measured from the letterform INSIDE it and the ring
// hangs over the pen the way a drop shadow does (jet_text.h); left at zero
// against a baked ring, every advance carries two pixels of halo and every
// line in the cart is silently mis-spaced. That is what a 0 here did, and the
// type gate now measures the letterform and the ring apart so that a bake and
// this number can never disagree quietly again.
//
// AND IT IS NOT PART OF A CAP. Every height in the ladder above is the
// LETTERFORM's, because a block is spaced by what the eye can see and the ring
// is the panel's own colour on the panel's own field.
#define JC_TYPE_OUTLINE_PX 1

// THE TRACKING EVERY LINE CARRIES. A sprite face advances by the letterform
// alone, so the side bearings a typographer drew are not in the bake; one
// pixel between glyphs is what puts them back. It is added by jc_rung_format
// to whatever extra a caller asks for, so there is one answer to "how tight is
// this face set" and every line in the cart shares it.
#define JC_TRACK 1

// A RIGHT-ALIGNED LINE LANDS FLUSH, and that is new. A monospaced face is
// aligned by its ADVANCE, so the last glyph's ink stopped short of the edge by
// whatever side bearing its cell held — three pixels for the score line, one
// for a file letter — and two constants existed to add them back. The baked
// faces advance by the letterform itself and jet_text_width measures the same
// thing, so both numbers measured zero and both are gone. The type gate still
// prints what it measures and still fails if it is ever not zero.

// The blocks down the panel: where each one's first INK sits, as an offset
// from the panel's own top.
//
// THE COLUMN IS COMPUTED, not typed. Every line below is the line above it
// plus that block's own cap height plus the gap the design gives the two, so
// changing a face's cap moves everything under it and nothing is measured
// twice. The old column was nine hand-fitted numbers whose stated arithmetic
// had already drifted from the numbers themselves; there is nothing left here
// to drift.
//
// IT IS ANCHORED AT BOTH ENDS. The head grows down from the panel's top pad
// and the foot grows up from its floor, so the type ladder can never push the
// buttons off the bottom of the panel — the only block that can be squeezed is
// the move well, and the _Static_assert under it says when the squeeze has
// gone too far.
#define JC_PANEL_WORDMARK_Y JC_PANEL_PAD
#define JC_PANEL_RULE_Y (JC_PANEL_WORDMARK_Y + JC_CAP_MARK + JC_RULE_GAP)
#define JC_PANEL_STATUS_Y (JC_PANEL_RULE_Y + 1 + JC_RULE_GAP)
#define JC_PANEL_OPENING_Y (JC_PANEL_STATUS_Y + JC_CAP_STATEMENT + JC_SECTION_GAP)
#define JC_PANEL_OPENING_VALUE_Y (JC_PANEL_OPENING_Y + JC_CAP_LABEL + JC_LABEL_DROP)
#define JC_PANEL_MATERIAL_Y (JC_PANEL_OPENING_VALUE_Y + JC_CAP_TEXT + JC_SECTION_GAP)
#define JC_PANEL_MATERIAL_ROW_Y (JC_PANEL_MATERIAL_Y + JC_CAP_LABEL + JC_LABEL_DROP)
#define JC_PANEL_CAPTURES_Y (JC_PANEL_MATERIAL_ROW_Y + JC_CAP_TEXT + JC_STRIP_GAP)
#define JC_PANEL_MOVES_Y (JC_PANEL_CAPTURES_Y + JC_CAPTURE_PX + JC_SECTION_GAP)
#define JC_PANEL_MOVES_WELL_Y (JC_PANEL_MOVES_Y + JC_CAP_LABEL + JC_LABEL_DROP)

// ...and the foot, upward from the floor.
#define JC_PANEL_CHIPS_Y (JC_PANEL_H - JC_PANEL_PAD - JC_CHIP_H)
#define JC_PANEL_READOUT_Y                                                                         \
	(JC_PANEL_CHIPS_Y - JC_ACTION_GAP - JC_CAP_LABEL - JC_LABEL_DROP - JC_CAP_INSTRUMENT)
#define JC_PANEL_READOUT_LINE_Y (JC_PANEL_READOUT_Y + JC_CAP_LABEL + JC_LABEL_DROP)

// The well is exactly as tall as the rows it holds, so its floor lands under
// the last of them rather than wherever the arithmetic happened to stop. What
// the column leaves over goes into the air before the ENGINE block, where it
// cannot be seen; the proof that there is any to leave is at the foot of this
// file.
#define JC_PANEL_MOVES_H JC_MOVE_WELL_NEEDS

// The engine's footnote ticks over many times a second, so it is redrawn by
// the overlay rather than baked into the canvas — and an overlay has to say
// exactly what it disturbed. The band is the footnote's own cell, which covers
// every descender the face can draw, plus the room the depth track takes under
// it; a band a pixel too small leaves a smear that outlives the frame that
// made it (jet_still.h, law four).
#define JC_READOUT_TRACK_DROP 4
#define JC_READOUT_TRACK_H 2
#define JC_READOUT_BAND_H (JC_SFONT_INSTRUMENT_CELL + JC_READOUT_TRACK_DROP)

// The mark at the end of the wordmark's line, where the band would otherwise
// be three-quarters empty. While a game is on it is a disc of the moving
// side's own piece colour; when one ends, the score line takes its place — one
// slot that always says what state the game is in.
#define JC_TURN_DISC_R 10

// The wordmark is set in a monospaced face, and its `T` is narrower than its
// cell. Left alone, the T-to-C junction opens to twice every other gap and
// lands exactly where the colour changes, so the logotype reads "JET CHESS".
// The second word is pulled back by the difference — every letter gap in the
// finished mark is then four pixels, that one included, which is what makes
// the amber-to-cream change a colour change and not a word break.
#define JC_WORDMARK_KERN_T 4 // the T's left bearing
#define JC_WORDMARK_KERN 8   // ...and its right, plus the shift above

// The tick that stands before every label. It is what turns a stack of
// paragraphs into a stack of SECTIONS, and it is furniture, so it is not the
// accent's colour.
#define JC_LABEL_TICK_W 3
#define JC_LABEL_TICK_GAP 9

// How far small capitals are tracked apart. A face designed for eight-pixel
// body text sets its capitals too tight to read as a label, so both places
// that use them open them out: a section head by two pixels, and the word
// inside a control by one, because a control is already boxed.
#define JC_LABEL_TRACK 2
#define JC_CONTROL_TRACK 1

// The material bar: two hundred and forty pixels, calibrated to five pawns
// either way — which is the range a game actually spends its time in. A bar
// scaled for nine is a bar that never moves. One track colour, because two
// read as a progress bar stuck at half; the ends are named instead, on the
// section label's own line, directly over the ends they name.
// ODD, on purpose: an even track cannot have a centre. At 240 the one-pixel
// tick left Black 120 pixels of travel and White 119, so the same "+1" came
// out 24 pixels wide one way and 23 the other and a mirror-image position did
// not mirror. 241 gives each side exactly 120, and 24 pixels a pawn.
// The leader's spare pieces, drawn at a third of the sprite's own size and
// aligned to the end of the bar that names their side. The bar says HOW MUCH
// and the figure says how much in pawns; neither says WHAT, and "a bishop and
// two pawns" is a different game from "four pawns".
#define JC_CAPTURE_PX 24
#define JC_CAPTURE_GAP 2

#define JC_BAR_W 241
#define JC_BAR_H 8
#define JC_BAR_MAX_PAWNS 5
#define JC_BAR_TICK_OVERHANG 2

// The move list is a SCORESHEET: two column-pairs side by side, ten numbered
// moves in each, so twenty moves are on show and the well's whole width is
// used. A single three-column list left a third of the box permanently empty.
// Twenty rows on show, in two column-pairs of ten — and the pitch is
// SEVENTEEN. The captured strip takes a row's worth of height out of the well
// and the type ladder took two more; seventeen over an eleven-pixel cap is six
// pixels of leading, which is what a scoresheet's own ruling looks like, and
// it is what keeps all twenty moves on show.
#define JC_MOVE_PAD 6 // the same above row one as below row ten
#define JC_MOVE_HEADER_H (JC_MOVE_PAD + JC_CAP_LABEL + JC_MOVE_PAD) // to the rule under the heads
#define JC_MOVE_ROW_H 17
#define JC_MOVE_ROWS 10
#define JC_MOVE_COLUMNS 2

// What ten rows actually need, from the head's rule to the floor under the
// last of them. The proof that the well still holds them is at the foot of
// this file, where every constant it reads has been declared.
#define JC_MOVE_WELL_NEEDS                                                                         \
	(JC_MOVE_HEADER_H + JC_MOVE_PAD + (JC_MOVE_ROWS - 1) * JC_MOVE_ROW_H + JC_CAP_INSTRUMENT +     \
	    JC_MOVE_PAD)
// The two column-pairs get EQUAL halves of the well, with the same gutter
// outside each. Set from the block's left edge alone, the left gutter came out
// 30 pixels and the right margin 50, so the second pair's slot was 27 pixels
// narrower than the first's — invisible until a move needs the room.
#define JC_MOVE_GUTTER JC_PANEL_PAD
#define JC_MOVE_HALF 212           // one column-pair's width
#define JC_MOVE_NUMBER_RIGHT 46 // the move's number is right-aligned to here
// A column head is set in the label face and its moves in the instrument
// face. Both are baked faces and both draw their ink AT the pen, so a head and
// the column under it share one x and need no correction between them — while
// the two were Font ROM banks their left bearings differed by a pixel and this
// line carried it.
#define JC_MOVE_WHITE_X 58
// Black's column sits closer to White's than the two pairs sit to each other,
// which is the only thing that makes a scoresheet read as pairs. At 148 the
// gap inside a pair and the gap across the gutter measured the same to within
// five pixels, so the table read as four evenly spaced columns.
#define JC_MOVE_BLACK_X 118

// The buttons along the bottom: three verbs. The opponent's strength is a
// VALUE, not a verb, so it lives in the ENGINE block and not in this row.
//
// Their labels are set at ELEVEN pixels of cap in a forty-pixel box. At seven
// they were the same size as "OPENING" and "WHITE" — the three primary actions
// of the whole cart carrying the same type as its micro-labels, which made
// them read as chrome no matter how bright the ink was.
//
// 473 pixels less two 8-pixel gaps is 457, which is not divisible by three.
// The spare pixel goes to the MIDDLE button, so the row stays symmetric about
// its own centre; on the last button it made the row lean right by a pixel
// against a column that is otherwise exact.
#define JC_CHIP_COUNT 3
#define JC_CHIP_H 40
#define JC_CHIP_GAP 8

// The level control, at the right-hand end of the ENGINE block's label line.
// Eighteen tall rather than twenty-four: the box used to end four pixels above
// the footnote under it where the panel's own rhythm says thirteen.
//
// The chevrons are ANCHORED to the box, twelve pixels in from each border, and
// only the name is centred between them. Centred as one string with the name,
// they moved with the name's length — so the control's own arrows shifted
// whenever the setting changed, which is the one thing a stepper must not do.
#define JC_LEVEL_CHIP_W 136
#define JC_LEVEL_CHIP_H 18
#define JC_LEVEL_CHEVRON_INSET 12

// ---------------------------------------------------------------------------
// The promotion card
// ---------------------------------------------------------------------------
//
// A pawn on the far rank stops the game and asks a question, so the board goes
// quiet under the same colours a finished game wears and four answers appear
// on it. The card is FOUR BOARD SQUARES and nothing else: the board's own
// grid, on the pawn's own file, stacking away from the edge the pawn arrived
// at so the column always fits, with each piece standing on the board's own
// baseline. It is the only modal thing in the cart, and it borrows every rule
// it uses rather than inventing one.
#define JC_PROMOTE_CHOICES 4
#define JC_PROMOTE_CELLS_H (JC_SQUARE_PX * JC_PROMOTE_CHOICES) // 320
#define JC_PROMOTE_KEYLINE_PX 2

// The ring that marks the choice under the pointer. The accent, INSET rather
// than filling the cell: a filled cell would hide the piece it is offering,
// which is the one thing the card exists to show.
// ...and it is THE ring: a card built out of board squares marks a choice the
// way the board marks a square. See JC_RING_PX above.

// ---------------------------------------------------------------------------
// The frame-rate readout
// ---------------------------------------------------------------------------
//
// A diagnostic, off until R is pressed, and the only thing on the screen that
// is not part of the game. The reading itself is the FIRMWARE's (jet_draw_fps)
// and it takes no position: it sets the number hard into the screen's own
// top-right corner, four pixels in. JetChess has exactly one margin and four
// is not it, so ui/rate.c carries the reading onto the cart's grid with the
// camera rather than re-implementing a line of it.
//
// WHERE IT GOES. The band above the panel — the top gutter, the one part of
// the room nothing else uses — right-aligned to the panel's own text column,
// so the reading hangs off the same edge as every value in the panel below it,
// and centred in that band by its CAP, as everything in this cart is. It is
// set on the READOUT rung, because that is what it is: the same quiet
// machine's-own-figure voice the engine's footnote speaks in.
#define JC_RATE_RIGHT JC_BLOCK_RIGHT
#define JC_RATE_INK_Y ((JC_PANEL_Y - JC_CAP_INSTRUMENT) / 2) // centred in the band by its cap

// The firmware's own inset, mirrored — the one number in this file that
// belongs to another (runtime/jet_utils.c), and therefore the one number in it
// that could go quietly out of date. It does not: tests/jetchess/visual.c
// draws the firmware's reading, sets the SAME words itself where the layout
// says they belong, and holds the two frames to being the same frame.
#define JC_RATE_FIRMWARE_PAD 4

// The widest reading the firmware could ever print — "FPS 4294967295" — which
// is the band the overlay marks, because the reading is right-aligned and only
// its left edge moves. The tracker works in thirty-two pixel cells, so a band
// that cannot be outgrown costs exactly what a tight one would (jet_still.h,
// law four).
#define JC_RATE_MAX_CHARS 14

// ---------------------------------------------------------------------------
// Movement and time
// ---------------------------------------------------------------------------

// How long a piece takes to slide from one square to another when the engine
// moves it. Long enough to follow with the eye, short enough not to wait.
#define JC_SLIDE_SECONDS 0.16f

// The shortest a reply may take, in ticks. An engine that answers in four
// milliseconds feels like a table of answers rather than an opponent, and the
// move's sound needs room to be heard.
//
// Counted in TICKS rather than in microseconds on purpose. A tick is one
// hundred and twentieth of a second on every host the machine has (the
// contract _update is called under), so the wait is the same everywhere AND
// the same on a headless run, where ticks are not paced by a clock at all and
// a wall-clock wait would either never end or end at once.
#define JC_THINK_FLOOR_TICKS 40

// How much of a tick the search may take when there is no worker thread to run
// it on — a browser. At 120 ticks a second this leaves the frame most of its
// budget, and a deepening iteration simply spans a few frames.
#define JC_SEARCH_SLICE_MICROS 3000u

// The transposition table, in bytes. Eight mebibytes is half a million
// entries: more than a five-ply search can fill, and nothing on a machine with
// sixty-four mebibytes of working memory.
#define JC_TABLE_BYTES (8u * 1024u * 1024u)

// ---------------------------------------------------------------------------
// The opponent
// ---------------------------------------------------------------------------

// Three levels. `depth` is how far ahead it looks and `spread` how far below
// its favourite a move may score and still be played (engine/search.h explains
// why that is the honest way to make an opponent weaker). The cap is a safety
// net for a position that explodes, not the way a level is normally reached.
#define JC_LEVEL_COUNT 3
#define JC_LEVEL_CASUAL 0
#define JC_LEVEL_CLUB 1
#define JC_LEVEL_MASTER 2
#define JC_LEVEL_DEFAULT JC_LEVEL_CLUB

// ---------------------------------------------------------------------------
// What the layout promises
// ---------------------------------------------------------------------------
//
// The panel's column is computed from the type ladder and anchored at both
// ends, which leaves exactly two ways for a change to go wrong. Both fail
// here, at compile time, with the numbers in hand — and neither can reach a
// screenshot.

// The move well still fits between the block above it and the block below,
// with the panel's own gap to spare. A taller face or a wider gap fails here
// rather than by printing the tenth row over the well's own floor.
_Static_assert(JC_PANEL_MOVES_WELL_Y + JC_PANEL_MOVES_H + JC_SECTION_GAP <= JC_PANEL_READOUT_Y,
    "the move well no longer fits above the ENGINE block: the type ladder grew, or a panel gap did");

// ...and the chevrons of the level control are anchored inside a box that has
// to hold the name between them.
_Static_assert(JC_LEVEL_CHIP_H >= JC_CAP_INSTRUMENT,
    "the level control is shorter than the name it has to hold");

// THE COORDINATE BAND IS THE SAME ON BOTH AXES, which is the whole of what
// puts a rank digit as far off the board as a file letter. It holds while the
// board is centred in the screen and its margin is the gutter; move either and
// the marks go lopsided — by a few pixels, evenly, on every screen, which is
// the kind of wrong nobody sees and everybody feels.
_Static_assert(JC_SCREEN_H - (JC_BOARD_Y + JC_BOARD_PX + JC_BOARD_FRAME_PX) == JC_COORD_BAND,
    "the band under the board is not the band beside it: the coordinates would sit lopsided");

// ...and the band is still tall enough for the face that stands in it. Where
// each mark actually lands is measured in pixels by tests/jetchess/visual.c;
// this is the one way it can go wrong before there is a frame to measure.
_Static_assert(JC_CAP_LABEL <= JC_COORD_BAND,
    "the coordinate band is shorter than the face that has to stand in it");

#endif // JETCHESS_CONFIG_H
