#include "singleplayer.h"
#include <string.h>
#include <stdio.h>

extern void net_inject_line(const char *line);

/* ------------------------------------------------------------------ */
/* Simple circular queue of pending lines to inject on the next tick  */
/* ------------------------------------------------------------------ */
#define QCAP 4
static char  qbuf[QCAP][32];
static uint8_t qhead = 0, qtail = 0;

static inline bool q_full (void) { return (uint8_t)(qhead + 1) % QCAP == qtail; }
static inline bool q_empty(void) { return qhead == qtail; }

static void q_push(const char *s)
{
	if (q_full()) return;          /* drop if ever over?run – harmless here */
	strncpy(qbuf[qhead], s, 31);
	qbuf[qhead][31] = '\0';
	qhead = (uint8_t)(qhead + 1) % QCAP;
}

/* Public methods --------------------------------------------------------- */

void sp_reset(void)
{
	qhead = qtail = 0;
}

void sp_tick(void)
{
	if (q_empty()) return;
	net_inject_line(qbuf[qtail]);
	qtail = (uint8_t)(qtail + 1) % QCAP;
}

/* AI board setup helper -------------------------------------------------- */

/* ------------------------------------------------------------------ */
/* Fill the AI board with ships using RNG							  */
/* ------------------------------------------------------------------ */
void ai_place_random(void) {
	
	// Reseed the RNG for ship placement
	srand16(adc_read(3) * adc_read(4));	
	
	// Start with an empty board
	memset(aiOccupiedBitmap, 0, BITMAP_SIZE);

	for (uint8_t i = 0; i < NUM_SHIPS; ++i) {
		uint8_t len = SHIP_LENGTHS[i];

		// Generate all possible valid positions for this ship
		typedef struct {
			uint8_t row, col;
			bool horizontal;
		} ShipPos;

		ShipPos positions[GRID_ROWS * GRID_COLS * 2];
		uint16_t pos_count = 0;

		for (uint8_t row = 0; row < GRID_ROWS; ++row) {
			for (uint8_t col = 0; col < GRID_COLS; ++col) {
				if (ship_can_fit(aiOccupiedBitmap, row, col, len, true)) {
					positions[pos_count++] = (ShipPos){row, col, true};
				}
				if (ship_can_fit(aiOccupiedBitmap, row, col, len, false)) {
					positions[pos_count++] = (ShipPos){row, col, false};
				}
			}
		}

		// Shuffle the valid positions
		for (uint16_t j = 0; j < pos_count - 1; ++j) {
			uint16_t k = j + (rand16() % (pos_count - j));
			ShipPos tmp = positions[j];
			positions[j] = positions[k];
			positions[k] = tmp;
		}

		// Place the ship at the first valid position
		if (pos_count > 0) {
			ShipPos chosen = positions[0];
			for (uint8_t k = 0; k < len; ++k) {
				uint8_t r = chosen.row + (chosen.horizontal ? 0 : k);
				uint8_t c = chosen.col + (chosen.horizontal ? k : 0);
				BITMAP_SET(aiOccupiedBitmap, r, c);
			}
		}
	}
}

void find_random_ship_square(int* row_to_attack, int* col_to_attack) {
	
	if (playerRemaining == 0) {
		return;
	}
		
	int hit_index = rand_int(0, playerRemaining-1);
		
	int i=0;
		
	for (int8_t y=0; y<GRID_ROWS; ++y) {
		for (int8_t x=0; x<GRID_COLS; ++x) {
				
			if (BITMAP_GET(playerOccupiedBitmap, y, x) && !BITMAP_GET(enemyAttackBitmap, y, x)) {		// If an unattacked player square
					
				if (i == hit_index) {
					*row_to_attack = y;
					*col_to_attack = x;
					return;
				}
				i++;
			}
		}
	}
}

/* Spoofed TX helpers ----------------------------------------------------- */

/* ------------------------------------------------------------------ */
/* Player transmits that they're ready to the AI					  */
/* ------------------------------------------------------------------ */
void sp_on_tx_ready(uint16_t self_token)
{
	// 1 - Set up the AI board
	ai_place_random();
	
	// 2 - Transmit ready back
	char line[32];
	snprintf(line, sizeof(line), "READY %u", 1);   /* static peer token = 1 */
	q_push(line);
}

/* ------------------------------------------------------------------ */
/* Player attacks the AI											  */
/* ------------------------------------------------------------------ */
void sp_on_tx_attack(uint8_t row, uint8_t col)
{
	char line[32];

	// 1 - Send result (player just hit/missed an AI ship)
	int hit = BITMAP_GET(aiOccupiedBitmap, row, col);
	snprintf(line, sizeof(line), "R %u %u %c", row, col, hit ? 'H' : 'M');
	q_push(line);

	// 2 - AI determines which player square to attack, and attacks
	
	int row_to_attack = 0;
	int col_to_attack = 0;
	find_random_ship_square(&row_to_attack, &col_to_attack);
	
	char coord[30]; sprintf(coord, "%d,%d", row_to_attack, col_to_attack);
	status_msg(coord);
	
	snprintf(line, sizeof(line), "A %u %u", row_to_attack, col_to_attack);
	q_push(line);
}

/* ------------------------------------------------------------------ */
/* Player transmits a hit/miss result back to the AI (not used)		  */
/* ------------------------------------------------------------------ */
void sp_on_tx_result(uint8_t row, uint8_t col, bool hit)
{
	// Not used by the AI – ignore
	(void)row; (void)col; (void)hit;
}
