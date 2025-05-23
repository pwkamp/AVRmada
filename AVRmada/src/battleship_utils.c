/* -------------------------------------------------------------------------
 * battleship_utils.c
 * Helper and hardware abstraction layer routines for Battleship
 *
 * v2.0
 * Copyright (c) 2025 Peter Kamp
 * ------------------------------------------------------------------------- */

#ifndef F_CPU
#define F_CPU		16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "eeprom.h"
#include "battleship_utils.h"
#include "strings.h"

/* -------------------------------------------------------------------------
 *  CONSTANTS
 * ------------------------------------------------------------------------- */
const uint8_t SHIP_LENGTHS[NUM_SHIPS] = {5, 4, 3, 3, 2};

/* -------------------------------------------------------------------------
 *  BOARD BITMAPS
 * ------------------------------------------------------------------------- */
uint8_t playerOccupiedBitmap   [BITMAP_SIZE];		// Where all of the player's ships actually are, set from the start of the game
uint8_t playerAttackedAtBitmap [BITMAP_SIZE];		// All of the squares the player has been attacked at, up to now	
uint8_t enemyConfirmedHitBitmap[BITMAP_SIZE];		// All of the *confirmed* enemy ship squares that have been hit, up to now
uint8_t enemyAttackedAtBitmap  [BITMAP_SIZE];		// All of the squares the enemy player has been attacked at, up to now	

/* -------------------------------------------------------------------------
 *  PSEUDO-RANDOM GENERATOR (16-bit LFSR)
 * ------------------------------------------------------------------------- */
static uint16_t lfsr; // LFSR internal state

/**
 * Seed the LFSR with a nonzero value.
 */
void srand16(uint16_t seed) {
	lfsr = seed ? seed : 0xACE1u; // Default seed if 0 provided
}

/**
 * Generate a pseudo-random 16-bit number.
 */
uint16_t rand16(void) {
	// Standard Galois 16-bit LFSR feedback polynomial
	lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
	return lfsr;
}

/**
 * Generate a pseudo-random integer between min and max (inclusive).
 */
uint16_t rand_int(uint16_t min, uint16_t max) {
	uint32_t range = (uint32_t)max - (uint32_t)min + 1;
	return (uint16_t)(rand16() % range + min);
}

/**
 * Generate a pseudo-random float between min and max (inclusive).
 */
float rand_float(float min, float max) {
		return min + ((float)rand16() / 65535.0f) * (max - min);
}

/**
 * Returns true randomly, `probability` (from 0.0 to 1.0) of the time, else false
 */
float rand_bool(float probability) {
	return (rand_float(0.0, 1.0) <= probability);
}

/* -------------------------------------------------------------------------
 *  ADC HELPERS (Joystick Reading)
 * ------------------------------------------------------------------------- */
/**
 * Initialize ADC hardware (AVCC ref, enable, prescaler 128).
 */
void adc_init(void) {
	ADMUX  = (1 << REFS0);					// AVCC reference
	ADCSRA = (1 << ADEN)  | (1 << ADPS2)
		   | (1 << ADPS1) | (1 << ADPS0);	// ADC enable, prescaler 128
	DIDR0  = (1 << ADC0D) | (1 << ADC1D);	// Disable digital input buffer for ADC0, ADC1
}

/**
 * Read ADC value from channel `ch`.
 */
uint16_t adc_read(uint8_t ch) {
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);	// Select channel
	_delay_us(10);							// Short settle delay
	ADCSRA |= (1 << ADSC);					// Start conversion
	while (ADCSRA & (1 << ADSC));			// Wait for completion
	return ADC;
}

/* -------------------------------------------------------------------------
 *  BUTTON HELPERS
 * ------------------------------------------------------------------------- */
/**
 * Initialize button on PD2 with pull-up enabled.
 */
void button_init(void) {
	DDRD  &= ~(1 << PD2); // Input
	PORTD |=  (1 << PD2); // Enable pull-up
}

/**
 * Check if button is pressed (active low).
 */
bool button_is_pressed(void) {
	return !(PIND & (1 << PD2));
}

/* -------------------------------------------------------------------------
 *  DRAWING PRIMITIVES
 * ------------------------------------------------------------------------- */
/**
 * Draw a single cell at (row, col) with given color and X origin.
 */
void draw_cell(uint8_t row, uint8_t col, uint16_t colour, uint16_t originX) {
	int16_t x = originX + col * CELL_SIZE_PX;
	int16_t y = GRID_Y_PX + row * CELL_SIZE_PX;

	fillRect(x, y, CELL_SIZE_PX, CELL_SIZE_PX, colour);
	drawRect(x, y, CELL_SIZE_PX, CELL_SIZE_PX, CLR_BLACK); // Outline
}

/**
 * Draw a highlight cursor box around a cell at (row, col).
 */
void draw_cursor(uint8_t row, uint8_t col, uint16_t originX) {
	int16_t x = originX + col * CELL_SIZE_PX;
	int16_t y = GRID_Y_PX  + row * CELL_SIZE_PX;

	drawRect(x,	y, CELL_SIZE_PX, CELL_SIZE_PX, CLR_CURSOR);
	drawRect(x+1,  y+1, CELL_SIZE_PX - 2, CELL_SIZE_PX - 2, CLR_CURSOR);
}

/* -------------------------------------------------------------------------
 *  SIMPLE TEXT HELPERS
 * ------------------------------------------------------------------------- */
/**
 * Draw placement mode header.
 */
void header_place(void) {
	fillRect(0, 0, 320, HEADER_HEIGHT_PX, CLR_BLACK);
	drawString_P(20, 10, PLACE_YOUR_SHIPS, CLR_WHITE, CLR_BLACK, 2, &font5x7, 0);
}

/**
 * Draw play mode header.
 */
void header_play(void) {
	fillRect(0, 0, 320, HEADER_HEIGHT_PX, CLR_BLACK);
	drawString_P(20, 10, YOUR_BOARD, CLR_WHITE, CLR_BLACK, 2, &font5x7, 0);
	drawString_P(ENEMY_GRID_X_PX + 10, 10, ENEMY_BOARD, CLR_WHITE, CLR_BLACK, 2, &font5x7, 0);
}

/**
 * Display a status message at the bottom of the screen.
 */
void status_msg(const char *msg) {
	fillRect(0, STATUS_Y_PX, 320, 30, CLR_BLACK);
	drawString(10, STATUS_Y_PX + 5, msg, CLR_WHITE, CLR_BLACK, 2, &font5x7, 0);
}

/* -------------------------------------------------------------------------
 *  BOARD UTILITY ROUTINES
 * ------------------------------------------------------------------------- */
/**
 * Reset both player and enemy grids to empty.
 */
void board_reset(void) {
	memset(playerOccupiedBitmap,     0, BITMAP_SIZE);
	memset(playerAttackedAtBitmap,   0, BITMAP_SIZE);
	memset(enemyConfirmedHitBitmap,  0, BITMAP_SIZE);
	memset(enemyAttackedAtBitmap,	 0, BITMAP_SIZE);
	playerRemaining = 0;
	enemyRemaining  = 0;
}

/**
 * Check if a ship can fit starting at (row,col) with given length and orientation.
 */
bool ship_can_fit(const uint8_t *occupiedBitmap, uint8_t row, uint8_t col, uint8_t len, bool horizontal) {
	if (horizontal) {
		if (col + len > GRID_COLS) return false;
		for (uint8_t x = 0; x < len; ++x) {
			if (BITMAP_GET(occupiedBitmap, row, col + x)) return false;
		}
	} else {
		if (row + len > GRID_ROWS) return false;
		for (uint8_t y = 0; y < len; ++y) {
			if (BITMAP_GET(occupiedBitmap, row + y, col)) return false;
		}
	}
	return true;
}

/* -------------------------------------------------------------------------
 *  SHIP PLACEMENT HELPERS
 * ------------------------------------------------------------------------- */
/**
 * Draw or erase ghost preview of ship placement at (row, col).
 */
void ghost_update(uint8_t row, uint8_t col, bool horizontal, bool draw) {
	uint8_t len = SHIP_LENGTHS[ghostShipIdx];
	bool valid = ship_can_fit(playerOccupiedBitmap, row, col, len, horizontal);
	uint16_t colour = valid ? CLR_GHOST_OK : CLR_GHOST_BAD;

	for (uint8_t k = 0; k < len; ++k) {
		uint8_t r = row + (horizontal ? 0 : k);
		uint8_t c = col + (horizontal ? k : 0);

		if (r >= GRID_ROWS || c >= GRID_COLS)
			continue;

		if (draw) {
			draw_cell(r, c, colour, PLAYER_GRID_X_PX);
		} else {
			uint16_t base = BITMAP_GET(playerOccupiedBitmap, r, c) ? CLR_SHIP : CLR_CYAN;
			draw_cell(r, c, base, PLAYER_GRID_X_PX);
		}
	}
}

/**
 * Finalize placing the current ship onto the player's grid.
 */
void player_place_current_ship(uint8_t row, uint8_t col, bool horizontal, uint8_t len) {
	playerFleet[ghostShipIdx] = (Ship){row, col, len, horizontal};

	for (uint8_t k = 0; k < len; ++k) {
		uint8_t r = row + (horizontal ? 0 : k);
		uint8_t c = col + (horizontal ? k : 0);

		BITMAP_SET(playerOccupiedBitmap, r, c);
		++playerRemaining;
		draw_cell(r, c, CLR_SHIP, PLAYER_GRID_X_PX);
	}
}

/* -------------------------------------------------------------------------
 *  STATIC GUI BUILDERS
 * ------------------------------------------------------------------------- */

/*
 * Draw the initial main menu screen.
 */
void gui_draw_main_menu(void) {
	fillScreen(CLR_MM_BG);

	// Title
	drawString_P(67, 15, A_RMADA,  CLR_WHITE, CLR_MM_BG, 5, &font5x7, 0);
	drawString_P(162,55, COURSE_NUM, CLR_WHITE, CLR_MM_BG, 2, &font5x7, 0);

	// Buttons & gear
	gui_draw_multiplayer_button(CLR_LIGHT_GRAY, CLR_DARK_GRAY);
	gui_draw_singleplayer_button(CLR_LIGHT_GRAY, CLR_DARK_GRAY);

	// Draw the gear icon in the bottom-right corner
	gui_draw_settings_gear(CLR_LIGHT_GRAY);

	// Animate 'V'
	gui_animate_title_letter_v();
}

/*
 * Draw or redraw the multiplayer button on the main menu screen.
 */
void gui_draw_multiplayer_button(uint16_t text_color, uint16_t border_color) {
	fillRectBorder(60, 95, 200, 50, 5, border_color);
	drawString_P(74, 109, MULTIPLAYER, text_color, CLR_MM_BG, 3, &font5x7, 0);
}

/*
 * Draw or redraw the singleplayer button on the main menu screen.
 */
void gui_draw_singleplayer_button(uint16_t text_color, uint16_t border_color) {
	fillRectBorder(60, 168, 200, 50, 5, border_color);
	drawString_P(89, 182, VERSUS_AI, text_color, CLR_MM_BG, 3, &font5x7, 0);
}

/*
 * Draw or redraw the gear icon on the main menu screen.
 */
void gui_draw_settings_gear(uint16_t color) {

	// Center of the gear
	uint16_t x_center = 303;
	uint16_t y_center = 223;

	// Larger circle
	fillCircle(x_center, y_center, 8, color);

	// Bottom and top spokes; left and right spokes
	fillRect(x_center-1, y_center-12, 3, 24, color);
	fillRect(x_center-12, y_center-1, 24, 3, color);

	// Top-left and bottom-right spokes
	fillTriangle(x_center-9, y_center-8, x_center+10, y_center+8, x_center+8, y_center+9, color);
	fillTriangle(x_center-9, y_center-8, x_center+10, y_center+8, x_center-7, y_center-9, color);

	// Top-right and bottom-left spokes
	fillTriangle(x_center+10, y_center-8, x_center-9, y_center+8, x_center-7, y_center+9, color);
	fillTriangle(x_center+10, y_center-8, x_center-9, y_center+8, x_center+8, y_center-9, color);

	// Hollow out the middle (to the background color)
	fillCircle(x_center, y_center, 4, CLR_MM_BG);
}

/*
 * Animate the title screen's letter 'V' to fade in slowly.
 */
void gui_animate_title_letter_v(void) {
	for (uint8_t i = 0; i < 255; i += 3) {
		drawString_P(93, 15, V_CHAR, rgb(i, i, i), CLR_MM_BG, 5, &font5x7, 0);
	}
}

// Settings Menu

/*
 * Draw the Settings screen.
 */
void gui_draw_settings_screen(const bool *sounds, const AIDifficulty *difficulty) {

	// Black background (main menu's background color in header file)
	fillScreen(CLR_MM_BG);

	// Start the title text "Settings"
	drawString_P(59, 28, SETTINGS,   CLR_WHITE, CLR_MM_BG, 5, &font5x7, 0);

	// Draw button textures
	gui_draw_sound_toggle_button(CLR_NONE, CLR_DARK_GRAY, sounds);
	gui_draw_difficulty_button(CLR_NONE, CLR_DARK_GRAY, difficulty);
	gui_draw_settings_back(CLR_LIGHT_GRAY);

}

/*
 * Draw or redraw the sound toggle button on the main menu screen.
 */
void gui_draw_sound_toggle_button(uint16_t text_color, uint16_t border_color, const bool *sound) {
	fillRectBorder(60, 95, 200, 50, 5, border_color);
	if (sound && *sound) {
		if (text_color != CLR_NONE) drawString_P(74, 109, SOUNDS_ON, text_color, CLR_MM_BG, 3, &font5x7, 0);
		drawString_P(74, 109, SOUNDS_ON, CLR_GREEN, CLR_MM_BG, 3, &font5x7, 0);
	} else {
		if (text_color != CLR_NONE) drawString_P(74, 109, SOUNDS_OFF, text_color, CLR_MM_BG, 3, &font5x7, 0);
		drawString_P(74, 109, SOUNDS_OFF, CLR_RED, CLR_MM_BG, 3, &font5x7, 0);
	}

}

/*
 * Draw or redraw the difficulty button on the settings screen.
 */
void gui_draw_difficulty_button(uint16_t text_color, uint16_t border_color, const AIDifficulty *difficulty) {
	fillRectBorder(60, 168, 200, 50, 5, border_color);
	if (difficulty) {
		switch (*difficulty) {
			case AI_EASY:
				if (text_color != CLR_NONE) {
					drawString_P(74, 182, AI, text_color, CLR_MM_BG, 3, &font5x7, 0);
					drawString_P(132, 185, LIEUTENANT, text_color, CLR_MM_BG, 2, &font5x7, 0);
				}
				drawString_P(74, 182, AI, CLR_GREEN, CLR_MM_BG, 3, &font5x7, 0);
				drawString_P(132, 185, LIEUTENANT, CLR_GREEN, CLR_MM_BG, 2, &font5x7, 0);
				break;
			case AI_MEDIUM:
				if (text_color != CLR_NONE) {
					drawString_P(74, 182, AI, text_color, CLR_MM_BG, 3, &font5x7, 0);
					drawString_P(132, 185, CAPTAIN, text_color, CLR_MM_BG, 2, &font5x7, 0);
				}
				drawString_P(74, 182, AI, CLR_YELLOW, CLR_MM_BG, 3, &font5x7, 0);
				drawString_P(132, 185, CAPTAIN, CLR_YELLOW, CLR_MM_BG, 2, &font5x7, 0);
				break;
			case AI_HARD:
				if (text_color != CLR_NONE) {
					drawString_P(74, 182, AI, text_color, CLR_MM_BG, 3, &font5x7, 0);
					drawString_P(132, 185, ADMIRAL, text_color, CLR_MM_BG, 2, &font5x7, 0);
				}
				drawString_P(74, 182, AI, CLR_RED, CLR_MM_BG, 3, &font5x7, 0);
				drawString_P(132, 185, ADMIRAL, CLR_RED, CLR_MM_BG, 2, &font5x7, 0);
				break;
		}
	}
}

/*
 * Draw or redraw the back icon on the settings screen.
 */
void gui_draw_settings_back(uint16_t color) {

	// Center of the gear
	uint16_t x_center = 17;
	uint16_t y_center = 223;

	// Bottom and top spokes; left and right spokes
	fillRect(x_center, y_center-6, 8, 12, color);

	// Top-left and bottom-right spokes
	fillTriangle(x_center-10, y_center, x_center, y_center+10, x_center, y_center-10, color);
	//fillTriangle(x_center-9, y_center-8, x_center+10, y_center+8, x_center-7, y_center-9, color);
}

/**
 * Draw the initial ship placement screen.
 */
void gui_draw_placement(void) {
	header_place();
	status_msg("Use stick to place");

	// Clear artifacts
	fillRect(0, 200, 320, 10, CLR_BLACK);

	for (uint8_t r = 0; r < GRID_ROWS; ++r) {
		for (uint8_t c = 0; c < GRID_COLS; ++c) {
			draw_cell(r, c, CLR_CYAN, PLAYER_GRID_X_PX);
			draw_cell(r, c, CLR_NAVY, ENEMY_GRID_X_PX);
		}
	}

	// Redraw placed ships
	for (uint8_t i = 0; i < ghostShipIdx; ++i) {
		Ship *s = &playerFleet[i];
		for (uint8_t k = 0; k < s->length; ++k) {
			draw_cell(
				s->row + (s->horizontal ? 0 : k),
				s->col + (s->horizontal ? k : 0),
				CLR_SHIP,
				PLAYER_GRID_X_PX
			);
		}
	}

	// Ghost preview
	ghost_update(selRow, selCol, ghostHorizontal, true);
}

/**
 * Draw the full play screen showing both grids.
 */
void gui_draw_play_screen(void) {
	for (uint8_t r = 0; r < GRID_ROWS; ++r) {
		for (uint8_t c = 0; c < GRID_COLS; ++c) {
			/* --- Player board --- */
			uint16_t pcol = BITMAP_GET(playerOccupiedBitmap, r, c)
						  ? CLR_SHIP : CLR_CYAN;
			if (BITMAP_GET(playerAttackedAtBitmap, r, c))
				pcol = BITMAP_GET(playerOccupiedBitmap, r, c) ? CLR_HIT : CLR_MISS;
			draw_cell(r, c, pcol, PLAYER_GRID_X_PX);

			/* --- Enemy board --- */
			uint16_t ecol = !BITMAP_GET(enemyAttackedAtBitmap, r, c)
							? CLR_NAVY
							: (BITMAP_GET(enemyConfirmedHitBitmap, r, c)
							? CLR_HIT : CLR_MISS);

			draw_cell(r, c, ecol, ENEMY_GRID_X_PX);
		}
	}

	header_play();
	status_msg("Your Turn");
	// draw_cursor(lastEnemyRow, lastEnemyCol, ENEMY_GRID_X_PX);
}

void gui_draw_lose_screen() {
	fillScreen(CLR_BLACK);
	displayImage(140, 60, 4);
	
	drawString_P(7, 20, THIS_NOT_THIS, CLR_RED, CLR_BLACK, 3, &font5x7, 0);
	drawString_P(7, 50, NOT_VERY_GOOD, CLR_RED, CLR_BLACK, 3, &font5x7, 0);
	drawString_P(7, 80, YOU_LOSE, CLR_RED, CLR_BLACK, 3, &font5x7, 0);
	drawString_P(7, 150, PRESS_2X, CLR_WHITE, CLR_BLACK, 3, &font5x7, 0);
	drawString_P(7, 180, TO_CONTINUE, CLR_WHITE, CLR_BLACK, 3, &font5x7, 0);
}

void gui_draw_win_screen() {
	fillScreen(CLR_BLACK);
	displayImage(140, 60, 4);
	
	drawString_P(7, 20, THIS_IS_THIS, CLR_GREEN, CLR_BLACK, 3, &font5x7, 0);
	drawString_P(7, 50, VERY_GOOD, CLR_GREEN, CLR_BLACK, 3, &font5x7, 0);
	drawString_P(7, 80, YOU_WIN, CLR_GREEN, CLR_BLACK, 3, &font5x7, 0);
	drawString_P(7, 150, PRESS_2X, CLR_WHITE, CLR_BLACK, 3, &font5x7, 0);
	drawString_P(7, 180, TO_CONTINUE, CLR_WHITE, CLR_BLACK, 3, &font5x7, 0);
}

/* -------------------------------------------------------------------------
 *  UART HELPERS
 * ------------------------------------------------------------------------- */
#define UART_BAUD 9600UL

/**
 * Initialize UART for 9600 baud, 8N1 configuration.
 */
void uart_init(void) {
	uint16_t ubrr = (F_CPU / 16UL / UART_BAUD) - 1;
	UBRR0H = ubrr >> 8;
	UBRR0L = ubrr & 0xFF;
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); /* 8 data bits, no parity, 1 stop bit */
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);   /* Enable receiver and transmitter */
}

/**
 * Send a character over UART (supports '\n' translation to '\r\n').
 */
int uart_putchar(char c, FILE *stream) {
	if (c == '\n')
		uart_putchar('\r', stream);

	while (!(UCSR0A & (1 << UDRE0))); // Wait until ready
	UDR0 = c;
	return 0;
}

/**
 * Return true if a character has been received (non-blocking).
 */
uint8_t uart_char_available(void) {
	return (UCSR0A & (1 << RXC0));
}

/**
 * Read a received character (blocking).
 */
char uart_getchar(void) {
	return UDR0;
}
