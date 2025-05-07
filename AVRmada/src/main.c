/* -------------------------------------------------------------------------
 * main.c - Two-player Battleship over UART
 *
 * v2.0 - Bitmap Board representation for SRAM Optimization
 * Copyright (c) 2025 Peter Kamp
 * --------------------------------------------------------------------------- */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "gfx.h"
#include "battleship_utils.h"
#include "buzzer.h"
#include "singleplayer.h"

/* -------------------------------------------------------------------------
 *  GAME SETTINGS
 * ------------------------------------------------------------------------- */
bool soundsEnabled	 = true;

/* -------------------------------------------------------------------------
 *  GLOBAL GAME STATE
 * ------------------------------------------------------------------------- */
Ship   playerFleet[NUM_SHIPS];
Ship   enemyFleet [NUM_SHIPS];

uint8_t lastEnemyRow = 0;
uint8_t lastEnemyCol = 0;

uint8_t selRow, selCol;					// Current selection cursor
uint8_t ghostShipIdx;					// Ship being placed
bool	ghostHorizontal;				// Ship placement orientation
uint8_t playerRemaining, enemyRemaining;

static int8_t pendingRow = -1;			// Row of pending outgoing shot
static int8_t pendingCol = -1;			// Col of pending outgoing shot

/* -------------------------------------------------------------------------
 *  LOCAL STATE
 * ------------------------------------------------------------------------- */
static uint32_t systemTime	  = 0;		// System milliseconds ticker
static uint32_t nextMoveAllowed = 0;	// Joystick move repeat throttle

static bool	buttonLatch	 = false;		// Prevent multiple button presses
static bool	overButtonLatch = false;
static uint8_t overTapCount	= 0;

/* -------------------------------------------------------------------------
 *  UART (printf redirected)
 * ------------------------------------------------------------------------- */
static FILE uart_stdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

/* -------------------------------------------------------------------------
 *  NETWORK PROTOCOL CONSTANTS
 * ------------------------------------------------------------------------- */
#define RX_MAX 32

static char	rxBuf[RX_MAX];			// RX buffer
static uint8_t rxIdx = 0;			// Current RX buffer index

static uint16_t selfToken	= 0;	// Local token (based on finish time)
static uint16_t peerToken	= 0;	// Remote peer token

static uint16_t resendTick   = 0;	// ms since last packet sent
static uint16_t postReadyLeft= 0;	// How long to keep sending READY after sync

/* -------------------------------------------------------------------------
 *  GAME STATES
 * ------------------------------------------------------------------------- */

// Game mode: determined by button selection in the main menu
static enum {
	GM_NONE,			// Default gMode is GM_NONE
	GM_MULTIPLAYER,
	GM_SINGLEPLAYER,
	GM_SETTINGS_GEAR	// Hovering over setting gear in main menu
} gMode;

// Game states: initially GS_RESET and is determined throughout the game loop
static enum {
	GS_RESET,
	GS_MAINMENU,
	GS_SETTINGS,
	GS_NEWGAME,
	GS_PLACING,
	GS_WAIT,
	GS_MYTURN,
	GS_WAITRES,
	GS_ENEMYTURN,
	GS_OVER
} gState;

// Network states
static enum {
	NS_IDLE,
	NS_WAIT_READY,
	NS_DECIDE,
	NS_MY_TURN,
	NS_PEER_TURN,
	NS_WAIT_RES,
	NS_GAME_OVER
} nState;

/* -------------------------------------------------------------------------
 *  PROTOCOL TRANSMISSION HELPERS
 * ------------------------------------------------------------------------- */
static inline void tx_ready(void) {
	if (gMode == GM_SINGLEPLAYER) {
		sp_on_tx_ready(selfToken);
	} else {
		 printf("READY %u\n", selfToken);
	}
}

static inline void tx_attack(uint8_t r, uint8_t c) {
	if (gMode == GM_SINGLEPLAYER) {
		sp_on_tx_attack(r, c);
	} else {
		printf("A %u %u\n", r, c);
	}
}

static inline void tx_result(uint8_t r, uint8_t c, bool hit) {
	if (gMode == GM_SINGLEPLAYER) {
		sp_on_tx_result(r, c, hit);
	} else {
		printf("R %u %u %c\n", r, c, hit ? 'H' : 'M');
	}
}

// Singleplayer Override
void net_inject_line(const char *line);

/* -------------------------------------------------------------------------
 *  INCOMING LINE HANDLERS
 * ------------------------------------------------------------------------- */

/**
 * Handle a READY packet received from peer.
 */
static void on_ready(uint16_t tok) {
	peerToken = tok;
	if (nState == NS_WAIT_READY)
		nState = NS_DECIDE;
}

/**
 * Handle an ATTACK packet received from peer.
 */
static void on_attack(uint8_t r, uint8_t c) {
	if (r >= GRID_ROWS || c >= GRID_COLS)
		return; // Ignore invalid coordinates

	// Was this cell already attacked?
	bool first_time = !BITMAP_GET(playerAttackBitmap, r, c);
	BITMAP_SET(playerAttackBitmap, r, c);

	bool hit = BITMAP_GET(playerOccupiedBitmap, r, c);

	if (first_time) {
		// First time being attacked here ? update grid visually
		draw_cell(r, c, hit ? CLR_HIT : CLR_MISS, PLAYER_GRID_X_PX);

		if (hit && --playerRemaining == 0) {
			// Game over (you lose)
			tx_result(r, c, hit);
			nState = NS_GAME_OVER;
			gState = GS_OVER;
			status_msg("You lose ? tap twice");
			play_lose_sound(&soundsEnabled);
		} else {
			// Otherwise, send result and hand turn to you
			tx_result(r, c, hit);
			nState = NS_MY_TURN;
			gState = GS_MYTURN;
			status_msg("Your turn");
			nextMoveAllowed = systemTime;
			gui_draw_play_screen();
			draw_cursor(selRow, selCol, ENEMY_GRID_X_PX);
			play_enemy_attack_sound(&hit, &soundsEnabled);
		}
	} else {
		// Duplicate attack (already attacked here) ? still must reply
		tx_result(r, c, hit);
	}
}

/**
 * Handle a RESULT packet received from peer (outcome of our shot).
 */
static void on_result(uint8_t r, uint8_t c, bool hit) {
	if (pendingRow < 0)
		return; // Ignore stray result if we don't have a pending shot

	// 1) If sounds enabled, play hit or miss sound depending on outcome
	play_attack_sound(&hit, &soundsEnabled);

	// 2) Erase pending cyan cell
	draw_cell(pendingRow, pendingCol, CLR_NAVY, ENEMY_GRID_X_PX);
	pendingRow = pendingCol = -1;

	// 3) Paint final outcome
	draw_cell(r, c, hit ? CLR_HIT : CLR_MISS, ENEMY_GRID_X_PX);

	// 4) Mark in our enemy bitmaps
	if (hit) BITMAP_SET(enemyOccupiedBitmap, r, c);

	// 5) Redraw selection cursor
	draw_cursor(selRow, selCol, ENEMY_GRID_X_PX);

	// 6) Check for game end or enemy turn
	if (hit && --enemyRemaining == 0) {
		nState = NS_GAME_OVER;
		gState = GS_OVER;
		status_msg("You win! ? tap twice");
		play_win_sound(&soundsEnabled);
	} else {
		nState = NS_PEER_TURN;
		gState = GS_ENEMYTURN;
		status_msg("Enemy turn");
	}
}

/**
 * Parse a full incoming line from UART.
 */
static void parse_line(char *l) {
	if (!strncmp(l, "READY", 5)) {
		uint16_t t;
		if (sscanf(l + 5, "%u", &t) == 1)
			on_ready(t);
	} else if (l[0] == 'A') {
		uint8_t r, c;
		if (sscanf(l + 1, "%hhu %hhu", &r, &c) == 2)
			on_attack(r, c);
	} else if (l[0] == 'R') {
		uint8_t r, c;
		char h;
		if (sscanf(l + 1, "%hhu %hhu %c", &r, &c, &h) == 3)
			on_result(r, c, h == 'H');
	}
}

/* -------------------------------------------------------------------------
 *  NETWORK TICK HANDLER
 * ------------------------------------------------------------------------- */

/**
 * Handles UART receive, retransmissions, and peer timeouts.
 * Call this function every 1ms.
 */
static void net_tick(void) {
	/* --- UART Receiving --- */
	while (uart_char_available()) {
		char c = uart_getchar();
		if (c == '\n' || c == '\r') {
			if (rxIdx) {
				rxBuf[rxIdx] = '\0';
				parse_line(rxBuf);
				rxIdx = 0;
			}
		} else if (rxIdx < RX_MAX - 1) {
			rxBuf[rxIdx++] = c;
		}
	}

	/* --- READY packet retransmission logic --- */
	bool needReady = (nState == NS_WAIT_READY) || (postReadyLeft > 0);
	if (needReady && ++resendTick >= 500) {
		resendTick = 0;
		tx_ready();
	}

	/* --- Attack retransmission logic --- */
	if (nState == NS_WAIT_RES && ++resendTick >= 100) {
		tx_attack(pendingRow, pendingCol);
		resendTick = 0;
	}

	/* --- Peer timeout while waiting for their move --- */
	if (nState == NS_PEER_TURN) {
		if (++resendTick >= 120000) { // 2 minutes timeout
			status_msg("Peer lost ? reset");
			_delay_ms(000);
			handle_reset();
		}
	}

	/* --- Decrease post-ready extra countdown --- */
	if (postReadyLeft)
		postReadyLeft--;
}

/* -------------------------------------------------------------------------
 *  RESET PROTOCOL & DRAW INITIAL MAIN MENU SCREEN
 * ------------------------------------------------------------------------- */

void handle_reset(void) {
	board_reset();
	ghostShipIdx	= 0;
	ghostHorizontal = true;
	selRow = selCol = GRID_ROWS / 2;
	nState		  = NS_IDLE;
	peerToken	   = 0;
	resendTick	  = 0;
	postReadyLeft   = 0;

	gui_draw_main_menu();

	gState = GS_MAINMENU;
}

/* -------------------------------------------------------------------------
 *  MAIN MENU SCREEN - USER SELECTS SINGLE OR MULTIPLAYER MODE, OR SETTINGS
 * ------------------------------------------------------------------------- */
static void handle_main_menu(void) {
	/* --- Select single/multiplayer mode with the joystick, and update button textures --- */
	uint16_t x = adc_read(0);
	uint16_t y = adc_read(1);

	switch (gMode) {
		// No button currently selected
		case GM_NONE:
			if (y < JOY_MIN_RAW) {			   // Joystick up, go to multiplayer button
				gMode = GM_MULTIPLAYER;
				gui_draw_multiplayer_button(CLR_WHITE, CLR_CYAN);
			} else if (y > JOY_MAX_RAW) {		// Joystick down, go to singleplayer button
				gMode = GM_SINGLEPLAYER;
				gui_draw_singleplayer_button(CLR_WHITE, CLR_GREEN);
			}
			break;

		// Multiplayer button currently selected
		case GM_MULTIPLAYER:
			if (y > JOY_MAX_RAW) {			   // Joystick down, go to singleplayer button (and fade out multiplayer button)
				gMode = GM_SINGLEPLAYER;
				gui_draw_singleplayer_button(CLR_WHITE, CLR_GREEN);
				gui_draw_multiplayer_button(CLR_LIGHT_GRAY, CLR_DARK_GRAY);
			}
			break;

		// Singleplayer button currently selected
		case GM_SINGLEPLAYER:
			if (y < JOY_MIN_RAW) {			   // Joystick up, go to multiplayer button (and fade out singleplayer button)
				gMode = GM_MULTIPLAYER;
				gui_draw_multiplayer_button(CLR_WHITE, CLR_CYAN);
				gui_draw_singleplayer_button(CLR_LIGHT_GRAY, CLR_DARK_GRAY);
			}
			else if (x > JOY_MAX_RAW) {		 // Joystick right, go to settings gear icon (and fade out singleplayer button)
				gMode = GM_SETTINGS_GEAR;
				gui_draw_settings_gear(CLR_WHITE);
				gui_draw_singleplayer_button(CLR_LIGHT_GRAY, CLR_DARK_GRAY);
			}
			break;

		// Settings gear currently selected
		case GM_SETTINGS_GEAR:
			if (x < JOY_MIN_RAW) {			   // Joystick left, go to singleplayer button (and fade out settings gear)
				gMode = GM_SINGLEPLAYER;
				gui_draw_singleplayer_button(CLR_WHITE, CLR_GREEN);
				gui_draw_settings_gear(CLR_LIGHT_GRAY);
				_delay_ms(200);
			}
			break;
	}

	/* --- If user presses the joystick after selecting a gamemode, start a game --- */
	if (button_is_pressed() && (gMode == GM_MULTIPLAYER || gMode == GM_SINGLEPLAYER)) {
		gState = GS_NEWGAME;
	}
	/* --- If user presses the joystick after selecting the settings gear, go to settings --- */
	else if (button_is_pressed() && (gMode == GM_SETTINGS_GEAR)) {
		gState = GS_SETTINGS;
	}
}

/* -------------------------------------------------------------------------
 *  SETTINGS MENU SCREEN
 * ------------------------------------------------------------------------- */
static void handle_settings(void) {
	fillRect(0, 0, 320, 120, CLR_GREEN);		// Just make the screen green for now and block forever

	while (1) {}
}

/* -------------------------------------------------------------------------
 *  START A NEW GAME - DRAW THE BOARD
 * ------------------------------------------------------------------------- */
static void handle_new_game(void) {
	if (gMode == GM_SINGLEPLAYER) sp_reset();
	gui_draw_placement();
	gState = GS_PLACING;
}

/* -------------------------------------------------------------------------
 *  FLEET PLACEMENT SCREEN
 * ------------------------------------------------------------------------- */
static void handle_placing(void) {
	static uint32_t invalidTimer = 0;
	static bool showInvalid	  = false;

	if (showInvalid) {
		if (systemTime - invalidTimer >= 500) {
			showInvalid = false;
			status_msg("Use stick to place");
		} else {
			return;
		}
	}

	/* --- Joystick navigation for placement --- */
	uint16_t x = adc_read(0);
	uint16_t y = adc_read(1);
	bool moved = false;
	uint8_t oldR = selRow, oldC = selCol;

	if (systemTime >= nextMoveAllowed) {
		if		(y < JOY_MIN_RAW && selRow > 0)				{ --selRow; moved = true; }
		else if (y > JOY_MAX_RAW && selRow < GRID_ROWS - 1)	{ ++selRow; moved = true; }
		else if (x < JOY_MIN_RAW && selCol > 0)				{ --selCol; moved = true; }
		else if (x > JOY_MAX_RAW && selCol < GRID_COLS - 1)	{ ++selCol; moved = true; }

		if (moved) {
			uint8_t len = SHIP_LENGTHS[ghostShipIdx];
			if (ghostHorizontal && selCol > GRID_COLS - len) selCol = GRID_COLS - len;
			if (!ghostHorizontal && selRow > GRID_ROWS - len) selRow = GRID_ROWS - len;
			ghost_update(oldR, oldC, ghostHorizontal, false);
			ghost_update(selRow, selCol, ghostHorizontal, true);
			nextMoveAllowed = systemTime + JOY_REPEAT_DELAY_MS;
		}
	}

	/* --- Button handling for placement/rotation --- */
	bool pressed = button_is_pressed();
	if (pressed && !buttonLatch) {
		buttonLatch = true;
		uint32_t holdStart = systemTime;

		// Check for long hold vs short press
		while (button_is_pressed() && (systemTime - holdStart) < 1000) {
			_delay_ms(1);
			systemTime++;
			net_tick(); // Keep network responsive while holding
		}

		if ((systemTime - holdStart) >= 500) {
			/* Long press ? rotate ship */
			ghost_update(selRow, selCol, ghostHorizontal, false);
			ghostHorizontal = !ghostHorizontal;

			uint8_t len = SHIP_LENGTHS[ghostShipIdx];
			if (ghostHorizontal && selCol > GRID_COLS - len) selCol = GRID_COLS - len;
			if (!ghostHorizontal && selRow > GRID_ROWS - len) selRow = GRID_ROWS - len;

			ghost_update(selRow, selCol, ghostHorizontal, true);
		} else {
			/* Short press ? attempt to place ship */
			uint8_t len = SHIP_LENGTHS[ghostShipIdx];
			if (ship_can_fit(playerOccupiedBitmap, selRow, selCol, len, ghostHorizontal)) {
				ghost_update(selRow, selCol, ghostHorizontal, false);
				player_place_current_ship(selRow, selCol, ghostHorizontal, len);

				ghostShipIdx++;

				if (ghostShipIdx < NUM_SHIPS) {
					uint8_t nextLen = SHIP_LENGTHS[ghostShipIdx];

					/* keep the cursor inside the grid */
					if (ghostHorizontal && selCol > GRID_COLS - nextLen)  selCol = GRID_COLS - nextLen;
					if (!ghostHorizontal && selRow > GRID_ROWS - nextLen) selRow = GRID_ROWS - nextLen;

					ghost_update(selRow, selCol, ghostHorizontal, true);   // will show grey or red immediately
				} else if (ghostShipIdx == NUM_SHIPS) {
					// All ships placed ? ready to connect
					selfToken = (uint16_t)systemTime; // Use finishing time as token
					tx_ready();
					nState = NS_WAIT_READY;
					gState = GS_WAIT;
					resendTick = 0;
					peerToken = 0;
					postReadyLeft = 0;
					status_msg("Searching peer...");
				}
			} else {
				// Immediately update ghost for next ship to prevent stale display
				ghost_update(selRow, selCol, ghostHorizontal, true);
				// Invalid placement (overlapping/invalid)
				status_msg("Invalid placement!");
				showInvalid = true;
				invalidTimer = systemTime;
			}
		}
	}
	if (!pressed) buttonLatch = false;
}

/* -------------------------------------------------------------------------
 *  WAITING FOR PEER TO FINISH PLACEMENT
 * ------------------------------------------------------------------------- */
/**
 * Handles deciding who goes first after both players place ships.
 */
static void handle_wait_peer(void) {
	if (nState == NS_DECIDE && peerToken) {
		/* Decide turn order based on token */
		bool iStart = (selfToken > peerToken);

		/* Calculate total enemy ship cells */
		{
			uint8_t total_cells = 0;
			for (uint8_t i = 0; i < NUM_SHIPS; ++i)
				total_cells += SHIP_LENGTHS[i];
			enemyRemaining = total_cells;
		}

		/* First turn assignment */
		if (iStart) {
			nState = NS_MY_TURN;
			gState = GS_MYTURN;
		} else {
			nState = NS_PEER_TURN;
			gState = GS_ENEMYTURN;
		}

		selRow = GRID_ROWS / 2;
		selCol = GRID_COLS / 2;

		/* Prepare to flood READY for a short time still */
		postReadyLeft	= 2000;
		resendTick		= 0;
		nextMoveAllowed = systemTime;

		gui_draw_play_screen();
		draw_cursor(selRow, selCol, ENEMY_GRID_X_PX);
		status_msg(iStart ? "Your turn" : "Enemy turn");
	}
}

/* -------------------------------------------------------------------------
 *  PLAYER'S TURN: FIRING
 * ------------------------------------------------------------------------- */
/**
 * Handles joystick navigation and firing at enemy grid.
 */
static void handle_my_turn(void) {
	/* --- Cursor navigation --- */
	uint16_t joyX = adc_read(0);
	uint16_t joyY = adc_read(1);
	bool moved = false;
	uint8_t oldR = selRow, oldC = selCol;

	if (systemTime >= nextMoveAllowed) {
		if		(joyY < JOY_MIN_RAW && selRow > 0)				{ --selRow; moved = true; }
		else if (joyY > JOY_MAX_RAW && selRow < GRID_ROWS-1)	{ ++selRow; moved = true; }
		else if (joyX < JOY_MIN_RAW && selCol > 0)				{ --selCol; moved = true; }
		else if (joyX > JOY_MAX_RAW && selCol < GRID_COLS-1)	{ ++selCol; moved = true; }

		if (moved) {
			// Redraw previous cell background
			bool oldAtt = BITMAP_GET(enemyAttackBitmap, oldR, oldC);
			bool oldOcc = BITMAP_GET(enemyOccupiedBitmap, oldR, oldC);
			uint16_t bg = oldAtt ? (oldOcc ? CLR_HIT : CLR_MISS) : CLR_NAVY;
			draw_cell(oldR, oldC, bg, ENEMY_GRID_X_PX);

			// Draw new cursor
			draw_cursor(selRow, selCol, ENEMY_GRID_X_PX);

			nextMoveAllowed = systemTime + JOY_REPEAT_DELAY_MS;
		}
	}

	/* --- Fire weapon --- */
	bool pressed = button_is_pressed();
	if (pressed && !buttonLatch) {
		buttonLatch = true;

		if (!BITMAP_GET(enemyAttackBitmap, selRow, selCol)) {
			// Fire at unshot square
			BITMAP_SET(enemyAttackBitmap, selRow, selCol);

			draw_cell(selRow, selCol, CLR_PENDING, ENEMY_GRID_X_PX); // Pending color
			pendingRow = selRow;
			pendingCol = selCol;

			tx_attack(selRow, selCol);
			resendTick = 0;

			nState = NS_WAIT_RES;
			gState = GS_WAITRES;
			status_msg("Waiting for result...");
		}
	}
	if (!pressed) buttonLatch = false;
}

/* -------------------------------------------------------------------------
 *  GAME OVER STATE
 * ------------------------------------------------------------------------- */
/**
 * Handles tap-to-reset after a game ends.
 */
static void handle_over(void) {
	bool pressed = button_is_pressed();
	if (pressed && !overButtonLatch) {
		overButtonLatch = true;
		if (++overTapCount >= 2) {
			overTapCount = 0;
			handle_reset();
		}
	}
	if (!pressed)
		overButtonLatch = false;
}

/* -------------------------------------------------------------------------
 *  SINGLEPLAYER HELPERS
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 *  Helper for single-player mode – lets the AI push a complete line straight
 *  into the normal RX parser (avoids touching the UART layer).
 * ------------------------------------------------------------------------- */
void net_inject_line(const char *line)
{
	/* parse_line expects a writable buffer – make a local copy */
	char tmp[RX_MAX];
	strncpy(tmp, line, RX_MAX - 1);
	tmp[RX_MAX - 1] = '\0';
	parse_line(tmp);
}

/* -------------------------------------------------------------------------
 *  MAIN FUNCTION
 * ------------------------------------------------------------------------- */
/**
 * Program entry point.
 * Sets up peripherals, screen, and enters main game loop.
 */
int main(void) {
	/* --- Initialize TFT / SPI (Display setup) --- */
	ILI9341_CS_DDR  |= 1 << ILI9341_CS_PIN;
	ILI9341_DC_DDR  |= 1 << ILI9341_DC_PIN;
	ILI9341_RST_DDR |= 1 << ILI9341_RST_PIN;
	DC_DATA();
	RST_HIGH();

	spi_init();
	ili9341_init();

	uint8_t madctl = 0x28;
	ili9341_send_command_bytes(0x36, &madctl, 1);

	/* --- Initialize Peripherals --- */
	adc_init();
	button_init();
	uart_init();
	stdout = &uart_stdout;   // Redirect printf to UART

	gState = GS_RESET;	   // The initial game state is GS_RESET

	/* ---------------------------------------------------------------------
	 * Main game loop (runs forever)
	 * --------------------------------------------------------------------- */
	while (1) {
		net_tick(); // Process network events (incoming messages, retries)

		/* --- Handle game state --- */
		switch (gState) {
			case GS_RESET:
				handle_reset();				// Reset full protocol and board state; draw the main menu screen; update gState (to GS_MAINMENU) and default gMode (to GM_MULTIPLAYER)
				break;
			case GS_MAINMENU:
				handle_main_menu();			// Allow the user to select between gModes GM_MULTIPLAYER and GM_SINGLEPLAYER; goes to GS_NEWGAME
				break;
			case GS_SETTINGS:
				handle_settings();
				break;
			case GS_NEWGAME:
				handle_new_game();			// Draw initial game screen; goes to GS_PLACING
				break;
			case GS_PLACING:
				handle_placing();
				break;
			case GS_WAIT:
				handle_wait_peer();
				break;
			case GS_MYTURN:
				handle_my_turn();
				break;
			case GS_WAITRES:
				/* Passive – waiting for attack result */
				break;
			case GS_ENEMYTURN:
				/* Passive – waiting for peer's move */
				break;
			case GS_OVER:
				handle_over();
				break;
		}

		/* Flush one queued spoofed packet (single-player only) */
		if (gMode == GM_SINGLEPLAYER) sp_tick();

		_delay_ms(1);   // Tick every 1 ms
		systemTime++;   // Advance system time counter
	}
}
