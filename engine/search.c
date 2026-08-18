// carts/jetchess/engine/search.c
//
// The search: how JetChess decides what to play.
//
// The idea underneath is minimax — assume both sides play their best, look as
// far ahead as time allows, and choose the move that leads to the best
// position you can be *forced* into. Written plainly that is unaffordable:
// twenty moves each, five moves deep, is three million positions. Everything
// in this file exists to cut that number down without changing the answer.
//
//   ALPHA-BETA. If one reply to a move is already bad enough, the rest of that
//     move's replies cannot save it, so they are never looked at. This alone
//     turns the cost from twenty-to-the-depth into roughly its square root —
//     but only if good moves are tried first, which is why move ordering gets
//     more attention here than the search itself.
//
//   ITERATIVE DEEPENING. Search to depth 1, then 2, then 3. That sounds like
//     waste and is the opposite: each pass leaves behind the best move it
//     found, the next pass tries it first, and better ordering saves far more
//     than the shallow passes cost. It also means there is always a move
//     ready, which is what lets the search be stopped at any moment.
//
//   THE TRANSPOSITION TABLE. The same position is reached by many move orders.
//     A big table remembers what each position was worth and which move was
//     best, keyed by its hash, so the second arrival is free.
//
//   QUIESCENCE. Stopping the count at a fixed depth is dangerous: the last
//     move might be "I take your queen", with the recapture one ply out of
//     sight. So at depth zero the search does not evaluate — it keeps going
//     through captures alone until the position is quiet.
//
//   NULL MOVE, LATE MOVE REDUCTIONS, and the small prunings alongside them,
//     each trade a sliver of accuracy for a great deal of depth. They are
//     marked where they appear.
#include "search.h"

#include "engine/internal.h"

// ---------------------------------------------------------------------------
// The transposition table
// ---------------------------------------------------------------------------
//
// Sixteen bytes an entry, and the full 64-bit key stored in each: a shorter
// key would collide about once in a few million probes, and a collision is a
// move from a different position played as if it belonged in this one.

#define JC_BOUND_NONE 0
#define JC_BOUND_EXACT 1 // the score is the true value
#define JC_BOUND_LOWER 2 // the true value is at least this (a beta cutoff)
#define JC_BOUND_UPPER 3 // the true value is at most this (nothing beat alpha)

typedef struct {
	uint64_t key;
	JcMove move;
	int16_t score;
	uint8_t depth;
	uint8_t bound;
} JcTableEntry;

static JcTableEntry *s_table;
static unsigned s_table_mask;  // entries - 1, so indexing is one AND
static unsigned s_table_used;

void jc_table_init(void *memory, unsigned bytes) {
	s_table = NULL;
	s_table_mask = 0;
	s_table_used = 0;
	unsigned entries = bytes / (unsigned)sizeof(JcTableEntry);
	// Round down to a power of two: a mask is far quicker than a remainder,
	// and the few entries given up cost nothing.
	unsigned power = 1;
	while (power * 2 <= entries) {
		power *= 2;
	}
	if (memory == NULL || power < 2) {
		return;
	}
	s_table = (JcTableEntry *)memory;
	s_table_mask = power - 1;
	jc_table_clear();
}

void jc_table_clear(void) {
	if (s_table) {
		memset(s_table, 0, ((size_t)s_table_mask + 1) * sizeof(JcTableEntry));
	}
	s_table_used = 0;
}

int jc_table_load(void) {
	if (!s_table) {
		return 0;
	}
	return (int)((uint64_t)s_table_used * 1000 / (s_table_mask + 1));
}

// A mate score means "mate in N plies from HERE", so it has to be stored
// relative to the position, not to the root, or a transposition would report a
// mate at the wrong distance.
static int16_t score_to_table(int score, int ply) {
	if (score > JC_MATE_THRESHOLD) {
		return (int16_t)(score + ply);
	}
	if (score < -JC_MATE_THRESHOLD) {
		return (int16_t)(score - ply);
	}
	return (int16_t)score;
}

static int score_from_table(int score, int ply) {
	if (score > JC_MATE_THRESHOLD) {
		return score - ply;
	}
	if (score < -JC_MATE_THRESHOLD) {
		return score + ply;
	}
	return score;
}

static void table_store(uint64_t key, JcMove move, int score, int depth, int bound, int ply) {
	if (!s_table) {
		return;
	}
	JcTableEntry *entry = &s_table[key & s_table_mask];
	// Depth-preferred: a deeper answer is worth more than a newer one, but a
	// different position always wins, because a stale entry for a position
	// nobody is searching is dead weight.
	if (entry->key == key && entry->depth > depth && bound != JC_BOUND_EXACT) {
		return;
	}
	if (entry->bound == JC_BOUND_NONE) {
		s_table_used++;
	}
	entry->key = key;
	entry->move = move;
	entry->score = score_to_table(score, ply);
	entry->depth = (uint8_t)depth;
	entry->bound = (uint8_t)bound;
}

// What the table remembers about this position, or NULL. The caller decides
// what to do with it: the move it holds is worth trying first whatever its
// depth, while its score may only be used when it was searched at least as
// deeply as we are about to.
static const JcTableEntry *table_find(uint64_t key) {
	if (!s_table) {
		return NULL;
	}
	const JcTableEntry *entry = &s_table[key & s_table_mask];
	return (entry->key == key && entry->bound != JC_BOUND_NONE) ? entry : NULL;
}

// Does this remembered score settle the question for the window we are asking
// about? An exact score always does; a lower bound does when it is already at
// or past beta; an upper bound when it is at or below alpha.
static bool entry_answers(const JcTableEntry *entry, int stored, int alpha, int beta) {
	switch (entry->bound) {
	case JC_BOUND_EXACT: return true;
	case JC_BOUND_LOWER: return stored >= beta;
	case JC_BOUND_UPPER: return stored <= alpha;
	default: return false;
	}
}

// ---------------------------------------------------------------------------
// Move ordering
// ---------------------------------------------------------------------------
//
// Alpha-beta's whole saving depends on trying the best move first, so these
// scores matter more to the engine's speed than anything else in the file.
// Highest first:
//
//   the table's move          the best move found here last time
//   winning captures          take the most valuable piece with the least
//   promotions                a new queen is worth looking at
//   killers                   quiet moves that caused a cutoff at this depth
//   history                   quiet moves that have caused cutoffs anywhere

#define JC_ORDER_TABLE_MOVE 16000
#define JC_ORDER_PROMOTION 15000
#define JC_ORDER_CAPTURE 10000
#define JC_ORDER_KILLER_FIRST 9500
#define JC_ORDER_KILLER_SECOND 9400
#define JC_ORDER_HISTORY_CAP 9000

// What a capture wins, in centipawns. An en-passant capture's victim is not on
// the target square, which is the one case worth spelling out.
static int victim_value(const JcPosition *position, JcMove move) {
	return (move.flags & JC_MOVE_EN_PASSANT)
	           ? jc_piece_value(JC_PAWN)
	           : jc_piece_value(JC_KIND(position->board[move.to]));
}

// Most Valuable Victim, Least Valuable Attacker: taking a queen with a pawn is
// the best-looking capture there is, and taking a pawn with a queen the worst.
// Ranked by KIND rather than by value, so the whole scale stays small enough
// that no capture can ever outrank the table's own move.
static int capture_order(const JcPosition *position, JcMove move) {
	const int victim = (move.flags & JC_MOVE_EN_PASSANT) ? JC_PAWN : JC_KIND(position->board[move.to]);
	const int attacker = JC_KIND(position->board[move.from]);
	return victim * 8 - attacker;
}

static void score_moves(JcSearch *search, const JcPosition *position, JcMoveList *list,
                        JcMove table_move, int ply) {
	for (int i = 0; i < list->count; i++) {
		const JcMove move = list->moves[i];
		int score;
		if (!jc_move_is_none(table_move) && jc_move_same(move, table_move)) {
			score = JC_ORDER_TABLE_MOVE;
		} else if (move.flags & JC_MOVE_PROMOTION) {
			score = JC_ORDER_PROMOTION + jc_piece_value(move.promotion);
		} else if (move.flags & JC_MOVE_CAPTURE) {
			score = JC_ORDER_CAPTURE + capture_order(position, move);
		} else if (jc_move_same(move, search->killers[ply][0])) {
			score = JC_ORDER_KILLER_FIRST;
		} else if (jc_move_same(move, search->killers[ply][1])) {
			score = JC_ORDER_KILLER_SECOND;
		} else {
			score = search->quiet_history[position->board[move.from]][move.to];
			if (score > JC_ORDER_HISTORY_CAP) {
				score = JC_ORDER_HISTORY_CAP;
			}
		}
		list->scores[i] = (int16_t)score;
	}
}

// Bring the best remaining move to position `index`. A selection sort done one
// step at a time: most of the list is never reached, because a cutoff happens
// after two or three moves, so sorting it all would be wasted work.
static JcMove pick_move(JcMoveList *list, int index) {
	int best = index;
	for (int i = index + 1; i < list->count; i++) {
		if (list->scores[i] > list->scores[best]) {
			best = i;
		}
	}
	const JcMove move = list->moves[best];
	list->moves[best] = list->moves[index];
	list->scores[best] = list->scores[index];
	list->moves[index] = move;
	return move;
}

// ---------------------------------------------------------------------------
// Draws the search must see
// ---------------------------------------------------------------------------

// Has this exact position stood before, in the game or in the line being
// searched? One repetition is treated as a draw inside the search: insisting
// on the third occurrence would let the engine walk into a repetition it
// cannot then avoid.
static bool is_repetition(const JcSearch *search, const JcPosition *position) {
	// Only positions since the last pawn move or capture can repeat: anything
	// before that is unreachable, so the walk is short.
	const int reach = position->halfmove < search->history_count
	                      ? position->halfmove
	                      : search->history_count;
	for (int back = 2; back <= reach; back += 2) {
		if (search->history[search->history_count - back] == position->key) {
			return true;
		}
	}
	return false;
}

static bool is_drawn(const JcSearch *search, const JcPosition *position) {
	return position->halfmove >= 100 || jc_is_insufficient_material(position) ||
	       is_repetition(search, position);
}

// ---------------------------------------------------------------------------
// The clock
// ---------------------------------------------------------------------------

// How often the clock is read. Reading it every node would cost more than the
// search saves; every few thousand is often enough to stop within a millisecond.
#define JC_CLOCK_INTERVAL 2048

static bool out_of_time(JcSearch *search) {
	if ((search->result.nodes & (JC_CLOCK_INTERVAL - 1)) != 0) {
		return search->aborted;
	}
	if (jc_search_should_stop()) {
		search->aborted = true;
	} else if (search->deadline != 0 && jc_clock_micros() >= search->deadline) {
		search->aborted = true;
	}
	return search->aborted;
}

// ---------------------------------------------------------------------------
// Quiescence: keep going until nothing is hanging
// ---------------------------------------------------------------------------

// A capture that cannot possibly bring the score up to alpha is not worth
// searching. The margin allows for the position being a little better than it
// looks; a queen's worth of slack is the traditional amount.
#define JC_DELTA_MARGIN 900

static int quiesce(JcSearch *search, JcPosition *position, int alpha, int beta, int ply) {
	search->result.nodes++;
	if (out_of_time(search)) {
		return 0;
	}
	if (ply >= JC_MAX_PLY - 1) {
		return jc_evaluate(position);
	}

	// "Stand pat": the side to move is never forced to capture, so the static
	// evaluation is a floor on what the position is worth.
	const int stand_pat = jc_evaluate(position);
	if (stand_pat >= beta) {
		return stand_pat;
	}
	if (stand_pat > alpha) {
		alpha = stand_pat;
	}

	JcMoveList list;
	jc_generate_captures(position, &list);
	score_moves(search, position, &list, jc_move_none(), ply);

	int best = stand_pat;
	for (int i = 0; i < list.count; i++) {
		const JcMove move = pick_move(&list, i);
		if (!(move.flags & JC_MOVE_PROMOTION) &&
		    stand_pat + victim_value(position, move) + JC_DELTA_MARGIN < alpha) {
			continue; // delta pruning: even winning this piece falls short
		}
		JcUndo undo;
		if (!jc_make_if_legal(position, move, &undo)) {
			continue;
		}
		search->history[search->history_count++] = position->key;
		const int score = -quiesce(search, position, -beta, -alpha, ply + 1);
		search->history_count--;
		jc_unmake(position, move, &undo);
		if (search->aborted) {
			return 0;
		}
		if (score > best) {
			best = score;
			if (score > alpha) {
				alpha = score;
				if (alpha >= beta) {
					break;
				}
			}
		}
	}
	return best;
}

// ---------------------------------------------------------------------------
// The main search
// ---------------------------------------------------------------------------

// Null move: hand the opponent a free move. If the position is still winning
// after that, it was winning comfortably, and the whole branch can be cut. It
// is switched off in the endgame, where being obliged to move is often the
// worst thing that can happen to you and "a free move" is not free at all.
#define JC_NULL_MIN_DEPTH 3
#define JC_NULL_REDUCTION 2

// Late move reductions: once several moves have been tried and none beat
// alpha, the rest are probably no better, so they are searched shallower. If
// one surprises us it is searched again properly.
#define JC_LMR_MIN_DEPTH 3
#define JC_LMR_FIRST_REDUCED 4

// Does the side to move have anything but pawns? The null move needs this, for
// the reason in its comment.
static bool has_pieces(const JcPosition *position) {
	const uint8_t colour = position->side;
	const uint8_t count = position->roster_count[colour];
	for (uint8_t slot = 0; slot < count; slot++) {
		const uint8_t kind = JC_KIND(position->board[position->roster[colour][slot]]);
		if (kind != JC_PAWN && kind != JC_KING) {
			return true;
		}
	}
	return false;
}

static void remember_line(JcSearch *search, int ply, JcMove move) {
	search->line[ply][0] = move;
	const int below = search->line_length[ply + 1];
	for (int i = 0; i < below; i++) {
		search->line[ply][i + 1] = search->line[ply + 1][i];
	}
	search->line_length[ply] = below + 1;
}

static int negamax(JcSearch *search, JcPosition *position, int depth, int alpha, int beta,
                   int ply, bool allow_null) {
	search->line_length[ply] = 0;
	if (out_of_time(search)) {
		return 0;
	}
	if (ply > 0 && is_drawn(search, position)) {
		return 0;
	}
	if (ply >= JC_MAX_PLY - 1) {
		return jc_evaluate(position);
	}

	// Being in check is never a place to stop counting: the position is by
	// definition unsettled, so the search is extended by a ply instead.
	const bool in_check = jc_in_check(position);
	if (in_check) {
		depth++;
	}
	if (depth <= 0) {
		return quiesce(search, position, alpha, beta, ply);
	}

	search->result.nodes++;
	const bool is_principal = beta - alpha > 1;

	// The table is asked about this position once. Its move is always worth
	// trying first; its score is only taken on a narrow window, because on the
	// principal line an approximate score would cut the variation short.
	const JcTableEntry *remembered = table_find(position->key);
	const JcMove table_move = remembered ? remembered->move : jc_move_none();
	if (ply > 0 && !is_principal && remembered && remembered->depth >= depth) {
		const int stored = score_from_table(remembered->score, ply);
		if (entry_answers(remembered, stored, alpha, beta)) {
			return stored;
		}
	}

	if (allow_null && !is_principal && !in_check && depth >= JC_NULL_MIN_DEPTH &&
	    has_pieces(position)) {
		JcUndo undo;
		jc_make_null(position, &undo);
		search->history[search->history_count++] = position->key;
		const int score = -negamax(search, position, depth - 1 - JC_NULL_REDUCTION,
		                           -beta, -beta + 1, ply + 1, false);
		search->history_count--;
		jc_unmake_null(position, &undo);
		if (search->aborted) {
			return 0;
		}
		if (score >= beta) {
			// A mate score from a null-move search is not to be trusted: the
			// side that "passed" would never really have passed.
			return jc_score_is_mate(score) ? beta : score;
		}
	}

	JcMoveList list;
	jc_generate(position, &list);
	score_moves(search, position, &list, table_move, ply);

	int best = -JC_INFINITY;
	JcMove best_move = jc_move_none();
	int bound = JC_BOUND_UPPER;
	int played = 0;

	for (int i = 0; i < list.count; i++) {
		const JcMove move = pick_move(&list, i);
		JcUndo undo;
		if (!jc_make_if_legal(position, move, &undo)) {
			continue;
		}
		played++;
		search->history[search->history_count++] = position->key;

		const bool quiet = (move.flags & (JC_MOVE_CAPTURE | JC_MOVE_PROMOTION)) == 0;
		int score;
		if (played == 1) {
			// The first move gets the full window: it is the one most likely
			// to be best, and its score sets the bar for everything after.
			score = -negamax(search, position, depth - 1, -beta, -alpha, ply + 1, true);
		} else {
			int reduction = 0;
			if (quiet && !in_check && depth >= JC_LMR_MIN_DEPTH && played >= JC_LMR_FIRST_REDUCED) {
				reduction = 1 + (played > 8 && depth > 5 ? 1 : 0);
			}
			// A null window: we only need to know whether this move beats the
			// best so far, not by how much. That is far cheaper to prove.
			score = -negamax(search, position, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, true);
			// It did beat it, so the shortcuts have to be paid back: first the
			// reduction, then — if we are on the principal line and the score
			// landed inside the window — the narrow window itself.
			if (score > alpha && reduction > 0) {
				score = -negamax(search, position, depth - 1, -alpha - 1, -alpha, ply + 1, true);
			}
			if (score > alpha && score < beta) {
				score = -negamax(search, position, depth - 1, -beta, -alpha, ply + 1, true);
			}
		}

		search->history_count--;
		jc_unmake(position, move, &undo);
		if (search->aborted) {
			return 0;
		}

		if (score > best) {
			best = score;
			best_move = move;
			if (score > alpha) {
				alpha = score;
				bound = JC_BOUND_EXACT;
				remember_line(search, ply, move);
				if (alpha >= beta) {
					bound = JC_BOUND_LOWER;
					if (quiet) {
						// A quiet move good enough to cut here will probably
						// cut in its sibling positions too. Remember it twice
						// over: once for this depth, once for the whole game.
						if (!jc_move_same(move, search->killers[ply][0])) {
							search->killers[ply][1] = search->killers[ply][0];
							search->killers[ply][0] = move;
						}
						search->quiet_history[position->board[move.from]][move.to] +=
						    (int16_t)(depth * depth);
						if (search->quiet_history[position->board[move.from]][move.to] > JC_ORDER_HISTORY_CAP) {
							// Halve the whole table rather than let one entry
							// saturate: the ordering is about which move is
							// better, and halving keeps every ratio intact.
							for (int piece = 0; piece < 16; piece++) {
								for (int square = 0; square < JC_BOARD_SLOTS; square++) {
									search->quiet_history[piece][square] /= 2;
								}
							}
						}
					}
					break;
				}
			}
		}
	}

	if (played == 0) {
		// No legal move: mate if the king is attacked, stalemate if not. The
		// mate score counts down by ply so a quicker mate scores higher.
		search->line_length[ply] = 0;
		return in_check ? -JC_MATE_SCORE + ply : 0;
	}

	table_store(position->key, best_move, best, depth, bound, ply);
	return best;
}

// ---------------------------------------------------------------------------
// The root
// ---------------------------------------------------------------------------

// A tiny generator, used only to choose among moves the search rates equally.
// It is the engine's one deliberate piece of chance, and it is what makes two
// games from the same opening different games.
static uint32_t next_random(uint32_t *state) {
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

void jc_search_begin(JcSearch *search, const JcPosition *position,
                     const uint64_t *history, int history_count, JcSearchLimits limits) {
	memset(search, 0, sizeof *search);
	search->position = *position;
	search->limits = limits;
	search->next_depth = 1;
	search->result.best = jc_move_none();
	search->result.depth = 0;

	// Keep only the history that can still matter: positions before the last
	// irreversible move can never come round again.
	int keep = history_count < JC_HISTORY_KEYS ? history_count : JC_HISTORY_KEYS;
	for (int i = 0; i < keep; i++) {
		search->history[i] = history[history_count - keep + i];
	}
	search->history_count = keep;
	search->root_ply = keep;

	jc_generate_legal(position, &search->root);
	for (int i = 0; i < search->root.count; i++) {
		search->root_score[i] = -JC_INFINITY;
	}
	search->choice_seed = (uint32_t)(position->key ^ (position->key >> 32)) | 1u;
	search->deadline = limits.micros ? jc_clock_micros() + limits.micros : 0;
	search->done = search->root.count == 0;
	if (search->root.count > 0) {
		// Always have something playable, even if the first iteration is cut
		// short before it finishes a single move.
		search->result.best = search->root.moves[0];
	}
}

// Order the root moves for the next iteration: the best move from the last one
// first, then the rest by the scores they earned.
static void order_root(JcSearch *search) {
	for (int i = 0; i < search->root.count; i++) {
		for (int j = i + 1; j < search->root.count; j++) {
			if (search->root_score[j] > search->root_score[i]) {
				const JcMove move = search->root.moves[i];
				const int score = search->root_score[i];
				search->root.moves[i] = search->root.moves[j];
				search->root_score[i] = search->root_score[j];
				search->root.moves[j] = move;
				search->root_score[j] = score;
			}
		}
	}
}

// Choose among the moves the search likes, weighted by how much it likes them.
//
// The best move keeps the largest share and a move at the very edge of the
// band the smallest, so a weaker opponent is one that USUALLY finds the best
// move and sometimes settles for the second — which is what a weaker player
// is. Choosing uniformly across the band instead makes an opponent that plays
// its fifth choice as readily as its first, and that is not weaker, it is
// erratic: measured against fixed-depth opponents, a uniform pick inside forty
// centipawns cost about two whole plies of strength.
//
// A move `deficit` centipawns below the best is weighted `spread - deficit`,
// so the weight falls to nothing exactly at the edge of the band.
static void choose_from_the_best(JcSearch *search) {
	int best = search->root_score[0];
	for (int i = 1; i < search->root.count; i++) {
		if (search->root_score[i] > best) {
			best = search->root_score[i];
		}
	}
	// A forced mate is not a matter of taste: when one is found, it is played.
	if (jc_score_is_mate(best)) {
		return;
	}
	const int spread = search->limits.spread;
	long total = 0;
	for (int i = 0; i < search->root.count; i++) {
		const int deficit = best - search->root_score[i];
		if (deficit < spread) {
			total += spread - deficit;
		}
	}
	long wanted = (long)(next_random(&search->choice_seed) % (uint32_t)total);
	for (int i = 0; i < search->root.count; i++) {
		const int deficit = best - search->root_score[i];
		if (deficit >= spread) {
			continue;
		}
		wanted -= spread - deficit;
		if (wanted >= 0) {
			continue;
		}
		search->result.best = search->root.moves[i];
		search->result.score = search->root_score[i];
		if (search->result.line_length == 0 ||
		    !jc_move_same(search->result.line[0], search->result.best)) {
			// The variation belongs to the move the search LIKED; once a
			// different one is chosen, only the move itself is still true.
			search->result.line[0] = search->result.best;
			search->result.line_length = 1;
		}
		return;
	}
}

bool jc_search_step(JcSearch *search) {
	if (search->done) {
		return true;
	}
	const uint64_t began = jc_clock_micros();
	const uint64_t step_deadline = search->limits.step_micros
	                                   ? began + search->limits.step_micros
	                                   : 0;
	const int depth = search->next_depth;

	if (!search->iterating) {
		// A fresh iteration: put the moves the last one liked at the front,
		// because alpha-beta saves most when the best move is tried first.
		order_root(search);
		search->root_index = 0;
		search->root_alpha = -JC_INFINITY;
		search->root_best = 0;
		search->iterating = true;
	}

	while (search->root_index < search->root.count) {
		const int index = search->root_index;
		const JcMove move = search->root.moves[index];
		// Each root move starts from a fresh copy of the position, not from
		// the one the move before it unmade. Unmaking restores the board and
		// the hash exactly, but it leaves the ROSTER holding the same pieces
		// in a different order (board.c), and the generator walks the roster —
		// so a move searched after another sees its own moves in a different
		// order, and does a slightly different amount of work. Copying three
		// hundred bytes per root move removes that entirely, which is what
		// makes a search run in slices do precisely the work of one run
		// straight through. The tests hold the two to the same node count.
		JcPosition position = search->position;
		JcUndo undo;
		jc_make(&position, move, &undo);
		search->history[search->history_count++] = position.key;

		// THE WINDOW EVERY ROOT MOVE IS SEARCHED WITH, and why it depends on
		// the spread.
		//
		// With no spread, alpha-beta's usual discipline applies: once a move
		// has scored X, no later move needs a score at all — only a proof that
		// it is not better. A null window proves that far more cheaply, and
		// what such a search returns is a BOUND, not a value.
		//
		// With a spread, those scores are the very thing that chooses the move
		// (search.h), and bounds cannot be compared. A move whose true worth is
		// three pawns below the best can return a bound a hair under alpha,
		// because the null window cut the moment it had proved "no better" —
		// and the spread would then pick it as a near-equal. That is not a
		// weaker opponent; it is an opponent that hangs pieces.
		//
		// So when a spread is asked for, alpha is held that far BELOW the best
		// score so far and every move gets the full window. Everything inside
		// the band comes back with a real number; everything outside it fails
		// low, which is exactly the answer the band wanted anyway.
		const int floor = search->root_alpha == -JC_INFINITY
		                      ? -JC_INFINITY
		                      : search->root_alpha - search->limits.spread;
		int score;
		if (index == 0 || search->limits.spread > 0) {
			score = -negamax(search, &position, depth - 1, -JC_INFINITY, -floor, 1, true);
		} else {
			score = -negamax(search, &position, depth - 1, -floor - 1, -floor, 1, true);
			if (score > floor) {
				score = -negamax(search, &position, depth - 1, -JC_INFINITY, -floor, 1, true);
			}
		}
		search->history_count--;
		jc_unmake(&position, move, &undo);
		if (search->aborted) {
			break;
		}
		search->root_partial[index] = score;
		if (score > search->root_alpha) {
			search->root_alpha = score;
			search->root_best = index;
			remember_line(search, 0, move);
		}
		search->root_index++;
		// The budget is spent: stop between root moves and resume here next
		// time. The first move of an iteration always finishes, because until
		// it does there is no window for the rest to be searched against.
		if (step_deadline != 0 && search->root_index < search->root.count &&
		    jc_clock_micros() >= step_deadline) {
			break;
		}
	}

	const bool iteration_finished = !search->aborted && search->root_index >= search->root.count;
	if (iteration_finished) {
		// Only a whole iteration is believed. A part-finished one has seen
		// some moves at this depth and the rest at the last, which is not a
		// comparison anybody should act on.
		for (int i = 0; i < search->root.count; i++) {
			search->root_score[i] = search->root_partial[i];
		}
		search->result.depth = depth;
		search->result.best = search->root.moves[search->root_best];
		search->result.score = search->root_score[search->root_best];
		search->result.line_length = search->line_length[0];
		for (int i = 0; i < search->line_length[0]; i++) {
			search->result.line[i] = search->line[0][i];
		}
		if (search->limits.spread > 0) {
			choose_from_the_best(search);
		}
		search->iterating = false;
		search->next_depth++;
	}
	search->result.micros += jc_clock_micros() - began;

	// Stop when the depth asked for is reached, when a forced mate is found
	// (nothing deeper can improve on it), when there is only one legal move to
	// choose between, or when the clock has run out.
	search->done = search->aborted ||
	               (!search->iterating &&
	                (search->next_depth > search->limits.depth || search->root.count <= 1 ||
	                 jc_score_is_mate(search->result.score) ||
	                 (search->deadline != 0 && jc_clock_micros() >= search->deadline)));
	return search->done;
}

void jc_search_run(JcSearch *search) {
	while (!jc_search_step(search)) {
		// Every turn of this loop is one deepening iteration.
	}
}
