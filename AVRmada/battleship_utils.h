/* -------------------------------------------------------------------------
 * battleship_utils.h - Header file for battleship_utils.c
 *
 * v1.0 - Initial Commit
 * Copyright (c) 2025 Peter Kamp
 * ------------------------------------------------------------------------- */

#ifndef BATTLESHIP_UTILS_H
#define BATTLESHIP_UTILS_H

/* -------------------------------------------------------------------------
 *  Standard headers
 * ------------------------------------------------------------------------- */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------
 *  Board dimensions and color definitions
 * ------------------------------------------------------------------------- */
#define GRID_ROWS          10
#define GRID_COLS          10
#define CELL_SIZE_PX       16

#define PLAYER_GRID_X_PX   0
#define ENEMY_GRID_X_PX    160
#define GRID_Y_PX          40

#define HEADER_HEIGHT_PX   40
#define STATUS_Y_PX        210

/* Color definitions (RGB565 via gfx.h) */
#define CLR_BLACK          rgb(0,0,0)
#define CLR_WHITE          rgb(255,255,255)
#define CLR_CYAN           rgb(0,255,255)
#define CLR_NAVY           rgb(0,0,128)
#define CLR_SHIP           rgb(64,64,64)
#define CLR_HIT            rgb(255,0,0)
#define CLR_MISS           rgb(255,255,255)
#define CLR_GHOST_OK       rgb(0,255,0)
#define CLR_GHOST_BAD      rgb(255,0,0)
#define CLR_CURSOR         rgb(255,255,0)
#define CLR_PENDING		   rgb(255, 128, 0)

/* -------------------------------------------------------------------------
 *  Joystick configuration
 * ------------------------------------------------------------------------- */
#define JOY_CENTER_RAW     512
#define JOY_DEADZONE_RAW   40
#define JOY_MIN_RAW        (JOY_CENTER_RAW - JOY_DEADZONE_RAW)
#define JOY_MAX_RAW        (JOY_CENTER_RAW + JOY_DEADZONE_RAW)
#define JOY_REPEAT_DELAY_MS 150

/* -------------------------------------------------------------------------
 *  Ship constants
 * ------------------------------------------------------------------------- */
#define NUM_SHIPS 5
extern const uint8_t SHIP_LENGTHS[NUM_SHIPS];

/* -------------------------------------------------------------------------
 *  Game state structures
 * ------------------------------------------------------------------------- */
typedef struct {
    uint8_t row;
    uint8_t col;
    bool    occupied;
    bool    attacked;
} Cell;

typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t length;
    bool    horizontal;
} Ship;

/* -------------------------------------------------------------------------
 *  Extern global game state (defined in battleship_mp.c)
 * ------------------------------------------------------------------------- */
extern Cell   playerGrid[GRID_ROWS][GRID_COLS];
extern Cell   enemyGrid [GRID_ROWS][GRID_COLS];

extern Ship   playerFleet[NUM_SHIPS];
extern Ship   enemyFleet [NUM_SHIPS];

extern uint8_t playerRemaining;
extern uint8_t enemyRemaining;

extern uint8_t selRow, selCol;
extern uint8_t ghostShipIdx;
extern bool    ghostHorizontal;
extern uint8_t lastEnemyRow, lastEnemyCol;

/* -------------------------------------------------------------------------
 *  Function prototypes
 * ------------------------------------------------------------------------- */

/* ADC (Joystick) helpers */
void     adc_init(void);
uint16_t adc_read(uint8_t ch);

/* Button helpers */
void     button_init(void);
bool     button_is_pressed(void);

/* Random number generation */
void     srand16(uint16_t seed);
uint16_t rand16(void);

/* Drawing primitives */
void     draw_cell(uint8_t row, uint8_t col, uint16_t colour, uint16_t originX);
void     draw_cursor(uint8_t row, uint8_t col, uint16_t originX);

/* Text UI helpers */
void     header_place(void);
void     header_play(void);
void     status_msg(const char *msg);

/* Board utilities */
void     board_reset(void);
bool     ship_can_fit(Cell B[][GRID_COLS], uint8_t row, uint8_t col,
                      uint8_t len, bool horizontal);

/* Ship placement helpers */
void     ghost_update(uint8_t row, uint8_t col, bool horizontal, bool draw);
void     player_place_current_ship(uint8_t row, uint8_t col,
                                   bool horizontal, uint8_t len);
void     enemy_place_random(void);

/* GUI screen builders */
void     gui_draw_placement(void);
void     gui_draw_play_screen(void);

/* UART communication helpers */
void     uart_init(void);
int      uart_putchar(char c, FILE *stream);
uint8_t  uart_char_available(void);
char     uart_getchar(void);

#endif /* BATTLESHIP_UTILS_H */
