#ifndef SINGLEPLAYER_H
#define SINGLEPLAYER_H

#include "battleship_utils.h"
#include <stdint.h>
#include <stdbool.h>

/* The AI's board */
uint8_t	aiOccupiedBitmap[BITMAP_SIZE];

/*  Reset any internal state before every new singleplayer game */
void sp_reset(void);

void sp_tick(void);

/* Fill the AI board with ships using RNG */
void ai_place_random(void);

/*  Called from main.c instead of sending real UART traffic */
void sp_on_tx_ready(uint16_t self_token);
void sp_on_tx_attack(uint8_t row, uint8_t col);
void sp_on_tx_result(uint8_t row, uint8_t col, bool hit);   /* (unused for now) */

#endif /* SINGLEPLAYER_H */
