/* ---------------------------------------------------------------------------
 * singleplayer.c - Singleplayer implementation for AVRmada
 *
 * v2.0
 * Copyright (c) 2025 Peter Kamp and Brendan Brooks
 * --------------------------------------------------------------------------- */
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
	if (q_full()) return;          /* Drop if ever overrun – harmless here */
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

/* AI board helpers ------------------------------------------------------- */

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

/* -----------------------------------------------------------------------------*/
/* Get the coordinates of a random (currently unattacked) player ship square	*/
/* ---------------------------------------------------------------------------- */
void find_random_ship_square(int* row_to_attack, int* col_to_attack) {
	
	// Edge case: no remaining player ship squares to attack
	if (playerRemaining == 0) {
		return;
	}
	
	// Randomly select a player ship square to hit (indexed 0 to n-1)
	int hit_index = rand_int(0, playerRemaining-1);
		
	// Iterate through all player squares until the unattacked player ship square corresponding to `hit_index` (0 to n-1) is found
	int i=0;
	for (int8_t y=0; y<GRID_ROWS; ++y) {
		for (int8_t x=0; x<GRID_COLS; ++x) {
				
			if (BITMAP_GET(playerOccupiedBitmap, y, x) && !BITMAP_GET(playerAttackedAtBitmap, y, x)) {		// *Unattacked* player ship squares
					
				if (i == hit_index) {
					*row_to_attack = y;
					*col_to_attack = x;
					player_ship_squares_attacked++;
					return;
				}
				i++;
			}
		}
	}
}

/* -----------------------------------------------------------------------------*/
/* Get the coordinates of a random (currently unattacked) player ocean square	*/
/* (If none left, gets the coords of a ship square)								*/
/* ---------------------------------------------------------------------------- */
void find_random_ocean_square(int* row_to_attack, int* col_to_attack) {
	
	// Compute the number of remaining ocean squares
	player_ocean_squares_left = GRID_CELLS - (player_ship_squares_attacked + playerRemaining + player_ocean_squares_attacked);
	
	// Edge case: no remaining player ocean squares to attack; must attack a ship
	if (player_ocean_squares_left == 0) {
		find_random_ship_square(row_to_attack, col_to_attack);
		return;
	}
	
	// Randomly select a player ocean square to hit (indexed 0 to n-1)
	int hit_index = rand_int(0, player_ocean_squares_left-1);
	
	// Iterate through all player squares until the unattacked player ocean square corresponding to `hit_index` (0 to n-1) is found
	int i=0;
	for (int8_t y=0; y<GRID_ROWS; ++y) {
		for (int8_t x=0; x<GRID_COLS; ++x) {
			
			if (!BITMAP_GET(playerOccupiedBitmap, y, x) && !BITMAP_GET(playerAttackedAtBitmap, y, x)) {		// *Unattacked* player ocean squares
				
				if (i == hit_index) {
					*row_to_attack = y;
					*col_to_attack = x;
					player_ocean_squares_attacked++;
					return;
				}
				i++;
			}
		}
	}	
	
}

/* ----------------------------------------------------------------- */
/* AI determines the square to attack, and returns it via			 */
/* row_to_attack and col_to_attack									 */
/* ----------------------------------------------------------------- */
void ai_attack_algorithm(int* row_to_attack, int* col_to_attack) {
		
	// Determine the probability the AI will hit a ship square, based on difficulty setting
	switch (aiDifficulty) {
		case AI_EASY:
			probability_of_hit = 0.10;	// 10%
			break;
		case AI_MEDIUM:
			probability_of_hit = 0.20;
			break;
		case AI_HARD:
			probability_of_hit = 0.50;
			break;
	}
	
	// Select a ship or ocean square based on that probability
	if (rand_bool(probability_of_hit)) {
		find_random_ship_square(row_to_attack, col_to_attack);		// (result returned in row_to_attack and col_to_attack)
	} else {
		find_random_ocean_square(row_to_attack, col_to_attack);
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
	
	// 2 - Initialize game variables
	player_ship_squares_attacked = 0;
	player_ocean_squares_attacked = 0;
	
	// 3 - Transmit ready back
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

	// 1 - Send back the result (player just hit/missed an AI ship)
	int hit = BITMAP_GET(aiOccupiedBitmap, row, col);
	snprintf(line, sizeof(line), "R %u %u %c", row, col, hit ? 'H' : 'M');
	q_push(line);

	// 2 - AI determines which player square to attack
	int row_to_attack = 0;
	int col_to_attack = 0;
	ai_attack_algorithm(&row_to_attack, &col_to_attack);  // (result stored in row_to_attack and col_to_attack)
	
	// 3 - AI attacks that square
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
