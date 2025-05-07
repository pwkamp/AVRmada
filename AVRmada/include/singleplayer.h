#ifndef SINGLEPLAYER_H
#define SINGLEPLAYER_H

#include "battleship_utils.h"

#ifndef F_CPU
#define F_CPU		16000000UL
#endif

#include <stdint.h>
#include <stdbool.h>

/* The AI's board */
uint8_t	aiOccupiedBitmap[BITMAP_SIZE];

/* AI game variables */
float	 probability_of_hit;				// The computed probability the AI will hit a player ship square (instead of ocean); 0.0 to 1.0
uint16_t player_ship_squares_attacked;		// Number of player ship squares attacked so far
uint16_t player_ocean_squares_attacked;		// Number of player ocean squares attacked so far
uint16_t player_ocean_squares_left;			// Number of remaining player ocean squares

/*  Reset any internal state before every new singleplayer game */
void sp_reset(void);

void sp_tick(void);

/* Fill the AI board with ships using RNG */
void ai_place_random(void);

/* The algorithm used by the AI to determine which player square to attack */
void ai_attack_algorithm(int* row_to_attack, int* col_to_attack);

/*  Called from main.c instead of sending real UART traffic */
void sp_on_tx_ready(uint16_t self_token);
void sp_on_tx_attack(uint8_t row, uint8_t col);
void sp_on_tx_result(uint8_t row, uint8_t col, bool hit);   /* (unused for now) */

#endif /* SINGLEPLAYER_H */
