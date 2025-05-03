/* -------------------------------------------------------------------------
 * battleship_utils.c
 * Helper and hardware abstraction layer routines for Battleship
 *
 * v1.1 - Refactor Directories
 * Copyright (c) 2025 Peter Kamp
 * ------------------------------------------------------------------------- */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gfx.h"
#include "battleship_utils.h"

/* -------------------------------------------------------------------------
 *  CONSTANTS
 * ------------------------------------------------------------------------- */
const uint8_t SHIP_LENGTHS[NUM_SHIPS] = {5, 4, 3, 3, 2};

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

/* -------------------------------------------------------------------------
 *  ADC HELPERS (Joystick Reading)
 * ------------------------------------------------------------------------- */
/**
 * Initialize ADC hardware (AVCC ref, enable, prescaler 128).
 */
void adc_init(void) {
	ADMUX  = (1 << REFS0);							 // AVCC reference
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // ADC enable, prescaler 128
	DIDR0  = (1 << ADC0D) | (1 << ADC1D);			   // Disable digital input buffer for ADC0, ADC1
}

/**
 * Read ADC value from channel `ch`.
 */
uint16_t adc_read(uint8_t ch) {
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F); // Select channel
	_delay_us(10);						// Short settle delay
	ADCSRA |= (1 << ADSC);				 // Start conversion
	while (ADCSRA & (1 << ADSC));		   // Wait for completion
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
	int16_t y = GRID_Y_PX + row * CELL_SIZE_PX;

	drawRect(x, y, CELL_SIZE_PX, CELL_SIZE_PX, CLR_CURSOR);
	drawRect(x+1, y+1, CELL_SIZE_PX-2, CELL_SIZE_PX-2, CLR_CURSOR);
}

/* -------------------------------------------------------------------------
 *  SIMPLE TEXT HELPERS
 * ------------------------------------------------------------------------- */
/**
 * Draw placement mode header.
 */
void header_place(void) {
	fillRect(0, 0, 320, HEADER_HEIGHT_PX, CLR_BLACK);
	drawString(20, 10, "Place Your Ships", CLR_WHITE, CLR_BLACK, 2, &font5x7, 0);
}

/**
 * Draw play mode header.
 */
void header_play(void) {
	fillRect(0, 0, 320, HEADER_HEIGHT_PX, CLR_BLACK);
	drawString(20, 10, "Your Board", CLR_WHITE, CLR_BLACK, 2, &font5x7, 0);
	drawString(ENEMY_GRID_X_PX + 10, 10, "Enemy Board", CLR_WHITE, CLR_BLACK, 2, &font5x7, 0);
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
	for (uint8_t r = 0; r < GRID_ROWS; ++r) {
		for (uint8_t c = 0; c < GRID_COLS; ++c) {
			playerGrid[r][c] = (Cell){r, c, false, false};
			enemyGrid [r][c] = playerGrid[r][c];
		}
	}
	playerRemaining = 0;
	enemyRemaining  = 0;
}

/**
 * Check if a ship can fit starting at (row,col) with given length and orientation.
 */
bool ship_can_fit(Cell B[][GRID_COLS], uint8_t row, uint8_t col,
				  uint8_t len, bool horizontal) {
	if (horizontal) {
		if (col + len > GRID_COLS) return false;
		for (uint8_t x = col; x < col + len; ++x)
			if (B[row][x].occupied) return false;
	} else {
		if (row + len > GRID_ROWS) return false;
		for (uint8_t y = row; y < row + len; ++y)
			if (B[y][col].occupied) return false;
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
	bool valid = ship_can_fit(playerGrid, row, col, len, horizontal);
	uint16_t colour = valid ? CLR_GHOST_OK : CLR_GHOST_BAD;

	for (uint8_t k = 0; k < len; ++k) {
		uint8_t r = row + (horizontal ? 0 : k);
		uint8_t c = col + (horizontal ? k : 0);

		if (r >= GRID_ROWS || c >= GRID_COLS)
			continue;

		if (draw) {
			draw_cell(r, c, colour, PLAYER_GRID_X_PX);
		} else {
			uint16_t base = playerGrid[r][c].occupied ? CLR_SHIP : CLR_CYAN;
			draw_cell(r, c, base, PLAYER_GRID_X_PX);
		}
	}
}

/**
 * Finalize placing the current ship onto the player's grid.
 */
void player_place_current_ship(uint8_t row, uint8_t col,
								bool horizontal, uint8_t len) {
	playerFleet[ghostShipIdx] = (Ship){row, col, len, horizontal};

	for (uint8_t k = 0; k < len; ++k) {
		uint8_t r = row + (horizontal ? 0 : k);
		uint8_t c = col + (horizontal ? k : 0);

		playerGrid[r][c].occupied = true;
		++playerRemaining;
		draw_cell(r, c, CLR_SHIP, PLAYER_GRID_X_PX);
	}
}

/**
 * Randomly place the enemy's ships on their grid.
 */
void enemy_place_random(void) {
	for (uint8_t i = 0; i < NUM_SHIPS; ++i) {
		uint8_t len = SHIP_LENGTHS[i];
		for (uint8_t tries = 0; tries < 100; ++tries) {
			bool horizontal = rand16() & 1u;
			uint8_t row = rand16() % GRID_ROWS;
			uint8_t col = rand16() % GRID_COLS;

			if (!ship_can_fit(enemyGrid, row, col, len, horizontal))
				continue;

			enemyFleet[i] = (Ship){row, col, len, horizontal};

			if (horizontal) {
				for (uint8_t x = col; x < col + len; ++x)
					enemyGrid[row][x].occupied = true;
			} else {
				for (uint8_t y = row; y < row + len; ++y)
					enemyGrid[y][col].occupied = true;
			}

			enemyRemaining += len;
			break;
		}
	}
}

/* -------------------------------------------------------------------------
 *  STATIC GUI BUILDERS
 * ------------------------------------------------------------------------- */

/*
 * Draw the initial main menu screen.
 */
void gui_draw_main_menu(void) {

	// Black background (main menu's background color in header file)
	fillScreen(CLR_MM_BG);

	// Start the title text ("A Rmada" is missing its letter V!)
	drawString(67, 15, "A Rmada",   CLR_WHITE, CLR_MM_BG, 5, &font5x7, 0);
	drawString(162, 55, "ECE:3360", CLR_WHITE, CLR_MM_BG, 2, &font5x7, 0);

	// Draw button textures
	gui_draw_multiplayer_button(CLR_LIGHT_GRAY, CLR_DARK_GRAY);
	gui_draw_singleplayer_button(CLR_LIGHT_GRAY, CLR_DARK_GRAY);
	
	// Draw the gear icon in the bottom-right corner
	gui_draw_settings_gear(CLR_LIGHT_GRAY);

	// The letter "V" slowly fades into the title
	gui_animate_title_letter_v();

}

/*
 * Draw or redraw the multiplayer button on the main menu screen.
 */
void gui_draw_multiplayer_button(uint16_t text_color, uint16_t border_color) {
	fillRectBorder(60, 95, 200, 50, 5, border_color);
	drawString(74, 109, "Multiplayer", text_color, CLR_MM_BG, 3, &font5x7, 0);
}

/*
 * Draw or redraw the singleplayer button on the main menu screen.
 */
void gui_draw_singleplayer_button(uint16_t text_color, uint16_t border_color) {
	fillRectBorder(60, 168, 200, 50, 5, border_color);
	drawString(89, 182, "Versus AI", text_color, CLR_MM_BG, 3, &font5x7, 0);
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

	for (uint8_t i=0; i<255; i+=3) {
		drawString(93, 15, "V", rgb(i, i, i), CLR_MM_BG, 5, &font5x7, 0);		// The letter "V" slowly fades into the title
	}

}


/**
 * Draw the initial ship placement screen.
 */
void gui_draw_placement(void) {

	header_place();
	status_msg("Use stick to place");

	// Remove visual artifacts from the main menu screen not being drawn over
	fillRect(0, 200, 320, 10, CLR_BLACK);

	for (uint8_t r = 0; r < GRID_ROWS; ++r) {
		for (uint8_t c = 0; c < GRID_COLS; ++c) {
			draw_cell(r, c, CLR_CYAN, PLAYER_GRID_X_PX); // Player grid
			draw_cell(r, c, CLR_NAVY, ENEMY_GRID_X_PX);  // Enemy grid
		}
	}

	// Redraw already placed ships
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

	// Draw ghost preview of next ship
	ghost_update(selRow, selCol, ghostHorizontal, true);
}

/**
 * Draw the full play screen showing both grids.
 */
void gui_draw_play_screen(void) {
	for (uint8_t r = 0; r < GRID_ROWS; ++r) {
		for (uint8_t c = 0; c < GRID_COLS; ++c) {
			/* --- Player board --- */
			uint16_t pcol = playerGrid[r][c].occupied ? CLR_SHIP : CLR_CYAN;
			if (playerGrid[r][c].attacked)
				pcol = playerGrid[r][c].occupied ? CLR_HIT : CLR_MISS;
			draw_cell(r, c, pcol, PLAYER_GRID_X_PX);

			/* --- Enemy board --- */
			Cell *e = &enemyGrid[r][c];
			uint16_t ecol = !e->attacked ? CLR_NAVY : (e->occupied ? CLR_HIT : CLR_MISS);
			draw_cell(r, c, ecol, ENEMY_GRID_X_PX);
		}
	}

	header_play();
	status_msg("Your Turn");
	// draw_cursor(lastEnemyRow, lastEnemyCol, ENEMY_GRID_X_PX);
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
