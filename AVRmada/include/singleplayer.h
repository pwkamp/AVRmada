#ifndef SINGLEPLAYER_H
#define SINGLEPLAYER_H

#include "battleship_utils.h"
#include <stdint.h>
#include <stdbool.h>

/*  Reset any internal state before every new single?player game */
void sp_reset(void);

void sp_tick(void);

/*  Called from main.c instead of sending real UART traffic */
void sp_on_tx_ready(uint16_t self_token);
void sp_on_tx_attack(uint8_t row, uint8_t col);
void sp_on_tx_result(uint8_t row, uint8_t col, bool hit);   /* (unused for now) */

#endif /* SINGLEPLAYER_H */
