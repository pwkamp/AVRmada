/* -------------------------------------------------------------------------
 * battleship_utils.h
 * Helper routines for Battleship, optimized for minimal SRAM usage
 *
 * v2.0 - Bitmap Board representation for SRAM Optimization
 * Copyright (c) 2025 Peter Kamp
 * ------------------------------------------------------------------------- */

#ifndef BATTLESHIP_UTILS_H
#define BATTLESHIP_UTILS_H

/* -------------------------------------------------------------------------
 *  Standard headers
 * ------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 *  Board dimensions and definitions
 * ------------------------------------------------------------------------- */
#define GRID_ROWS			10
#define GRID_COLS			10
#define GRID_CELLS			(GRID_ROWS * GRID_COLS)
#define BITMAP_SIZE			((GRID_CELLS + 7) / 8)   /* bytes needed to store one bit per cell */

/* -------------------------------------------------------------------------
 *  Pixel geometry
 * ------------------------------------------------------------------------- */
#define CELL_SIZE_PX		16
#define PLAYER_GRID_X_PX	0
#define ENEMY_GRID_X_PX		160
#define GRID_Y_PX			40

#define HEADER_HEIGHT_PX	40
#define STATUS_Y_PX			210

/* -------------------------------------------------------------------------
 *  Color definitions (RGB565 via gfx.h)
 * ------------------------------------------------------------------------- */
#define CLR_NONE			rgb(1,2,3)
#define CLR_MM_BG			rgb(0,0,0)
#define CLR_BLACK			rgb(0,0,0)
#define CLR_WHITE			rgb(255,255,255)
#define CLR_DARK_GRAY		rgb(64,64,64)
#define CLR_LIGHT_GRAY		rgb(128,128,128)
#define CLR_GREEN			rgb(0,255,0)
#define CLR_RED				rgb(255,0,0)
#define CLR_YELLOW			rgb(255,255,0)
#define CLR_ORANGE			rgb(255,128,0)
#define CLR_CYAN			rgb(0,255,255)
#define CLR_NAVY			rgb(0,0,128)
#define CLR_SHIP			rgb(64,64,64)
#define CLR_HIT				rgb(255,0,0)
#define CLR_MISS			rgb(255,255,255)
#define CLR_GHOST_OK		rgb(0,255,0)
#define CLR_GHOST_BAD		rgb(255,0,0)
#define CLR_CURSOR			rgb(255,255,0)
#define CLR_PENDING			rgb(255,128,0)

/* -------------------------------------------------------------------------
 *  Joystick configuration
 * ------------------------------------------------------------------------- */
#define JOY_CENTER_RAW		512
#define JOY_DEADZONE_RAW	40
#define JOY_MIN_RAW			(JOY_CENTER_RAW - JOY_DEADZONE_RAW)
#define JOY_MAX_RAW			(JOY_CENTER_RAW + JOY_DEADZONE_RAW)
#define JOY_REPEAT_DELAY_MS 150

/* -------------------------------------------------------------------------
 *  AI definitions
 * ------------------------------------------------------------------------- */
typedef enum {
	AI_EASY,
	AI_MEDIUM,
	AI_HARD
} AIDifficulty;

extern AIDifficulty aiDifficulty;

/* -------------------------------------------------------------------------
 *  Ship constants
 * ------------------------------------------------------------------------- */
#define NUM_SHIPS 5
extern const uint8_t SHIP_LENGTHS[NUM_SHIPS];

/* -------------------------------------------------------------------------
 *  Bitmap board states
 *
 *  Each of these arrays is BITMAP_SIZE bytes; each bit represents one cell
 *  in row-major order (bit 0 = row 0,col 0; bit 1 = row 0,col 1; … bit 99 = row 9,col 9).
 * ------------------------------------------------------------------------- */
extern uint8_t playerOccupiedBitmap		[BITMAP_SIZE];
extern uint8_t playerAttackedAtBitmap	[BITMAP_SIZE];
extern uint8_t enemyConfirmedHitBitmap	[BITMAP_SIZE];
extern uint8_t enemyAttackedAtBitmap	[BITMAP_SIZE];

/* -------------------------------------------------------------------------
 *  Internal bit-manipulation helpers (static inline)
 * ------------------------------------------------------------------------- */

/**
 * Compute a linear index (0 to GRID_ROWS*GRID_COLS-1) from a 2D row/col.
 * This maps (row, col) into the flat bit-array.
 */
static inline uint16_t _grid_index(uint8_t row, uint8_t col) {
    return (uint16_t)row * GRID_COLS + col;
}

/**
 * Given a bit-array index i, compute which byte contains bit i.
 * Equivalent to i / 8.
 */
static inline uint8_t _byte_index(uint16_t idx) {
    return (uint8_t)(idx >> 3);
}

/**
 * Given a bit-array index i, compute a mask with a 1 in the bit position i%8.
 * Equivalent to (1 << (i & 7)).
 */
static inline uint8_t _bit_mask(uint16_t idx) {
    return (uint8_t)(1u << (idx & 7));
}

/* -------------------------------------------------------------------------
 *  Macros to get/set/clear a bit in a bitmap
 * ------------------------------------------------------------------------- */

/**
 * Test whether bit (r, c) in 'bitmap' is set.
 * 1) compute flat index = _grid_index(r,c)
 * 2) find containing byte = bitmap[_byte_index(index)]
 * 3) mask off the desired bit = _bit_mask(index)
 * 4) return true if nonzero
 */
#define BITMAP_GET(bitmap, r, c)					\
	( (bitmap[_byte_index(_grid_index((r),(c)))]	\
	& _bit_mask(_grid_index((r),(c)))) != 0 )

/**
 * Set bit (r, c) in 'bitmap' to 1.
 * 1) compute flat index
 * 2) locate containing byte
 * 3) OR in the bit mask
 */
#define BITMAP_SET(bitmap, r, c)					\
	( bitmap[_byte_index(_grid_index((r),(c)))]		\
	|= _bit_mask(_grid_index((r),(c))) )

/**
 * Clear bit (r, c) in 'bitmap' to 0.
 * 1) compute flat index
 * 2) locate containing byte
 * 3) AND with inverse of bit mask
 */
#define BITMAP_CLEAR(bitmap, r, c)					\
	( bitmap[_byte_index(_grid_index((r),(c)))]		\
	&= (uint8_t)~_bit_mask(_grid_index((r),(c))) )

/* -------------------------------------------------------------------------
 *  Ship description
 * ------------------------------------------------------------------------- */
typedef struct {
	uint8_t row;
	uint8_t col;
	uint8_t length;
	bool	horizontal;
} Ship;

/* -------------------------------------------------------------------------
 *  Extern global game state
 * ------------------------------------------------------------------------- */
extern Ship	playerFleet[NUM_SHIPS];

extern uint8_t playerRemaining;
extern uint8_t enemyRemaining;

extern uint8_t selRow;			/* current cursor row */
extern uint8_t selCol;			/* current cursor col */
extern uint8_t ghostShipIdx;	/* index of ship being placed */
extern bool	ghostHorizontal;	/* orientation of ship placement */

/* -------------------------------------------------------------------------
 *  Function prototypes
 * ------------------------------------------------------------------------- */

/* ADC (joystick) */
void		adc_init(void);
uint16_t	adc_read(uint8_t ch);

/* Button */
void		button_init(void);
bool		button_is_pressed(void);

/* Random (16-bit LFSR) */
void		srand16(uint16_t seed);
uint16_t	rand16(void);

/* Random int using rand16() */
uint16_t	rand_int(uint16_t min, uint16_t max);

/* Random float using rand16() */
float		rand_float(float min, float max);

/* Returns true randomly, `probability` (0.0 - 1.0) of the time */
float		rand_true(float probability);

/* Drawing primitives */
void	draw_cell(uint8_t row, uint8_t col, uint16_t colour, uint16_t originX);
void	draw_cursor(uint8_t row, uint8_t col, uint16_t originX);

/* Text/UI helpers */
void	header_place(void);
void	header_play(void);
void	status_msg(const char *msg);

/* Board reset: clears both occupied and attacked bitmaps, resets counters */
void	board_reset(void);

// Can a ship of length len fit at (row,col) without overlapping?
// occupiedBitmap = playerOccupiedBitmap or aiOccupiedBitmap.
bool	ship_can_fit(const uint8_t *occupiedBitmap, uint8_t row, uint8_t col, uint8_t len, bool horizontal);

/* Ship placement helpers */
void	ghost_update(uint8_t row, uint8_t col, bool horizontal, bool draw);
void	player_place_current_ship(uint8_t row, uint8_t col, bool horizontal, uint8_t len);

/* GUI screen builders */
void	 gui_draw_main_menu(void);
void	 gui_draw_multiplayer_button(uint16_t text_color, uint16_t border_color);
void	 gui_draw_singleplayer_button(uint16_t text_color, uint16_t border_color);
void     gui_draw_settings_gear(uint16_t color);
void	 gui_animate_title_letter_v(void);
void	 gui_draw_placement(void);
void	 gui_draw_play_screen(void);
void	 gui_draw_settings_screen(const bool *sounds, const AIDifficulty *difficulty);
void	 gui_draw_sound_toggle_button(uint16_t text_color, uint16_t border_color, const bool *sound);
void	 gui_draw_difficulty_button(uint16_t text_color, uint16_t border_color, const AIDifficulty *difficulty);
void	 gui_draw_settings_back(uint16_t color);
void	 gui_draw_lose_screen();
void	 gui_draw_win_screen();

/* UART communication helpers */
void	 uart_init(void);
int		 uart_putchar(char c, FILE *stream);
uint8_t  uart_char_available(void);
char	 uart_getchar(void);

#endif /* BATTLESHIP_UTILS_H */
