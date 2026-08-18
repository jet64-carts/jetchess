// carts/jetchess/main.c
//
// JetChess — a chess machine for the Jet64 fantasy console.
//
// This file is deliberately thin. It owns the four hooks, the handful of
// objects the whole cart is made of, and the one loop that joins them; every
// rule, every pixel and every sound lives in a module of its own.
//
//   engine/   pure chess, and no console API at all: the board, the moves,
//             the search, the notation, the opening book. It builds and runs
//             outside the machine, which is how tests/jetchess proves it.
//   game/     a game of chess: the scoresheet, the result, and when the
//             opponent thinks.
//   ui/       the screen: the board, the panel, the player's own hand, and
//             the one diagnostic the player can call up over all of it. Its
//             voice lives there too — ui/type.h holds the type ladder every
//             word is set through and ui/ink.h the table each colour prints
//             through, because the faces are baked sprite cells and a baked
//             face carries the colours its bake put into it. The compositor
//             that makes a still frame nearly free is the runtime's own
//             (jet_still.h).
//
// Two files sit beside this one rather than inside a layer, because each is
// about the game as a whole rather than about a part of it: audio.h, which
// turns what a move DID into a sound, and save.h, which writes down where the
// player left off and puts them back there next time.
//
// HOW A FRAME IS SPENT. Almost nothing on a chess screen moves, so almost
// nothing is redrawn. The whole screen is painted once into working memory and
// every frame after that restores the few rectangles the last frame disturbed
// (jet_still.h). A still frame costs tens of microseconds; the engine, on a
// machine with threads, is not on this thread at all.
#include "jet64.h"
#include "jet_display.h"
#include "jet_host.h"
#include "jet_io.h"
#include "jet_sprite.h"
#include "jet_still.h"
#include "jet_thread.h"
#include "jet_time.h"

#include "art/palette.h"
#include "audio.h"
#include "config.h"
#include "engine/book.h"
#include "game/brain.h"
#include "game/match.h"
#include "save.h"
#include "ui/board.h"
#include "ui/hand.h"
#include "ui/panel.h"
#include "ui/rate.h"
#include "ui/type.h"
#include "ui/view.h"

// The whole cart, as seven long-lived objects. They are statics rather than
// locals because three of them are tens of kilobytes — a search, a scoresheet
// and the record that is written from it — and a stack is not where those
// belong.
static JcMatch s_match;
static JcBrain s_brain;
static JcHand s_hand;
static JcSlide s_slide;
static JetStill s_still;
static JcView s_view;
static JcSave s_save;

// What a frame actually costs, summed so the cart can say so on its way out.
// Two clock reads a frame is eighty nanoseconds; the number they buy is the
// difference between claiming the compositor works and knowing it does.
static uint64_t s_draw_micros;

// ---------------------------------------------------------------------------
// Playing a move
// ---------------------------------------------------------------------------

// Play a move, sound it, and set the picture of it travelling. The match is
// updated at once and the slide is only the eye catching up, which is why the
// board knows to leave the arriving square to the overlay (ui/view.c).
static void play_move(JcMove move) {
	if (jc_move_is_none(move)) {
		return;
	}
	jc_match_play(&s_match, move);
	jc_audio_move_played(&s_match);

	s_slide.running = true;
	s_slide.move = move;
	s_slide.piece = s_match.position.board[move.to];
	s_slide.elapsed = 0.0f;
	s_slide.duration = JC_SLIDE_SECONDS;
}

static void new_game(void) {
	jc_brain_new_game(&s_brain);
	// A seed from the machine's own entropy, so the opening book takes a
	// different path through itself every game.
	jc_match_new(&s_match, JC_WHITE, jet_rand());
	jc_hand_reset(&s_hand, true);
	s_slide.running = false;
}

// ---------------------------------------------------------------------------
// Acting on what the player asked for
// ---------------------------------------------------------------------------

static void take_back(void) {
	// Two plies, so the player gets their own move back rather than being
	// handed the engine's turn. One, if the engine has not replied yet.
	jc_brain_abandon(&s_brain);
	s_slide.running = false;
	if (!jc_match_take_back(&s_match)) {
		return;
	}
	if (s_match.position.side != s_match.human_colour) {
		jc_match_take_back(&s_match);
	}
	jc_hand_reset(&s_hand, true);
}

static void act_on(JcIntent intent) {
	switch (intent.kind) {
	case JC_INTENT_MOVE:
		play_move(intent.move);
		break;
	case JC_INTENT_NEW_GAME:
		jc_audio_click();
		new_game();
		break;
	case JC_INTENT_TAKE_BACK:
		jc_audio_click();
		take_back();
		break;
	case JC_INTENT_FLIP:
		jc_audio_click();
		s_hand.flipped = !s_hand.flipped;
		break;
	case JC_INTENT_LEVEL:
		jc_audio_click();
		jc_brain_abandon(&s_brain);
		s_brain.level = (s_brain.level + 1) % JC_LEVEL_COUNT;
		break;
	case JC_INTENT_RATE:
		jc_audio_click();
		s_hand.showing_rate = !s_hand.showing_rate;
		break;
	case JC_INTENT_REFUSED:
		jc_audio_refuse();
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// The palette
// ---------------------------------------------------------------------------

// The cart's own palette file, read and set here, entry by entry, before
// anything reads a colour back.
//
// NOT jet_load_palette. That verb asks the host and returns at once; the host
// answers a tick or more later. Until then the machine shows whatever palette
// the host booted it with — on a Workstation rebuild that is the Workstation's
// live palette, not this file — and _init reads colours straight away: every
// voice's recolour table is built from them (ui/ink.h). Built against the
// wrong palette, a table points the soft edge of every word at the wrong
// entries, and when the real palette lands a moment later the words wear a
// dark, ragged ring. Reading the file here makes the palette true before the
// first table is built, on every runtime and whatever the host booted with.
//
// A write tells the host the palette changed, so the host takes these colours
// as the cart's own. If the file cannot be read the host is asked instead,
// which is no worse than before.
static void load_palette(void) {
	static uint8_t lut[JET_PALETTE_LUT_BYTES];
	JET_FILE file = jet_asset_open("palettes/" JC_PALETTE_NAME ".pal");
	const size_t read = file ? jet_fread(lut, 1, sizeof lut, file) : 0;
	if (file) {
		jet_fclose(file);
	}
	if (read != sizeof lut) {
		jet_load_palette(JC_PALETTE_NAME);
		return;
	}
	for (int entry = 0; entry < JET_PALETTE_SIZE; entry++) {
		const uint8_t *rgba = &lut[entry * 4];
		jet_palette_set((uint8_t)entry, rgba[0], rgba[1], rgba[2]);
	}
}

// ---------------------------------------------------------------------------
// The four hooks
// ---------------------------------------------------------------------------

// The whole screen, painted into the canvas. Called by the compositor, and
// only when something it draws from has actually changed.
static void paint_scene(void *user) {
	const JcView *view = (const JcView *)user;
	jet_cls(JC_VOID);
	jc_board_paint(view);
	jc_panel_paint(view);
}

void _init(void) {
	jet_display_set_resolution(JC_SCREEN_W, JC_SCREEN_H);
	jet_set_buffering_mode(JET_BUFFERING_TRIPLE);
	jet_host_set_vsync(true);
	// The cart's own palette: two board colours, two piece colours, one
	// accent, and the ramps that let an anti-aliased piece exist at all on a
	// machine with 256 colours (scripts/jetchess_art.py). Read here, and not
	// requested of the host, because jc_type_init reads it back (above).
	load_palette();
	jet_set_sprite_grid(JC_PIECE_PX, JC_PIECE_PX);
	// The cart's voice: four baked faces registered into the runtime's sprite
	// font slots, and the recolour table each colour prints through. Both read
	// memory that is true by now — the sprite banks the machine publishes before
	// a guest runs, and the palette set just above — and both are guest-side
	// statics, which a warm boot
	// does not preserve, so this is unconditional (ui/type.h).
	jc_type_init();
	jet_randomise();

	jc_engine_init();
	jc_book_init();
	jc_brain_init(&s_brain, JC_LEVEL_DEFAULT);
	// The canvas is one screen of working memory, so the resolution above has
	// to be settled before it is reserved.
	jet_still_init(&s_still);

	s_view.match = &s_match;
	s_view.hand = &s_hand;
	s_view.brain = &s_brain;
	s_view.slide = &s_slide;
	jc_hand_reset(&s_hand, false);
	new_game();
	// ...and then, if the player left one behind, the game they were playing,
	// the opponent they chose and the way round they had the board (save.h).
	// A fresh game is set up first so that a missing or unreadable record needs
	// no answer of its own: there is already a game for it to fall back on.
	const bool restored = jc_save_restore(&s_save, &s_match, &s_brain, &s_hand);

	printf("JetChess ready: %s opponent, %d book positions, %s, %s (%d plies)\n",
	       JC_LEVEL_TABLE[s_brain.level].name, jc_book_position_count(),
	       s_brain.threaded ? "thinking on a worker thread" : "thinking in slices",
	       restored ? "game restored" : "new game", s_match.ply_count);
}

void _update(float dt) {
	if (s_slide.running) {
		s_slide.elapsed += dt;
		s_slide.running = s_slide.elapsed < s_slide.duration;
	}

	act_on(jc_hand_update(&s_hand, &s_match, &s_view));

	if (s_match.result == JC_RESULT_PLAYING && !jc_match_humans_turn(&s_match)) {
		// Thinking starts at once, even while the player's own move is still
		// sliding home: the two overlap, and the reply feels immediate
		// without ever being hurried.
		jc_brain_begin(&s_brain, &s_match);
		if (!s_slide.running && jc_brain_poll(&s_brain)) {
			play_move(jc_brain_take(&s_brain));
		}
	}

	// Last, once everything that could have changed the game has had its turn:
	// the record is encoded and written if it differs from the one on disk.
	// Nothing above this line has to know that the game is being saved, which
	// is the whole reason it is here rather than in each of them (save.h).
	jc_save_sync(&s_save, &s_view);
}

void _draw(void) {
	const uint64_t began = jet_time_micros();
	jet_still_frame(&s_still, jc_view_fingerprint(&s_view), paint_scene, &s_view);
	jc_board_paint_overlay(&s_view, &s_still);
	jc_panel_paint_overlay(&s_view, &s_still);
	jc_rate_paint_overlay(&s_view, &s_still);
	s_draw_micros += jet_time_micros() - began;
}

void _end(void) {
	jc_brain_end(&s_brain);
	const uint64_t frames = s_still.counters.frames ? s_still.counters.frames : 1;
	printf("JetChess: %d plies, %llu frames from %llu whole repaints, %llu us a frame\n",
	       s_match.ply_count, (unsigned long long)s_still.counters.frames,
	       (unsigned long long)s_still.counters.bakes,
	       (unsigned long long)(s_draw_micros / frames));
}
