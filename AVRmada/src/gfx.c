/* ---------------------------------------------------------------------------
 * Graphics Driver for TFT Display using ILI9341 Controller
 *
 * This file provides:
 * - Basic SPI communication setup
 * - Low-level ILI9341 display control
 * - Drawing primitives (pixels, lines, rectangles, circles, etc.)
 * - Text rendering support with customizable fonts
 *
 * v1.1 - Refactor Directories
 * Copyright (c) 2025 Peter Kamp
 * --------------------------------------------------------------------------- */

#include "gfx.h"
#include <stdbool.h>

#ifndef F_CPU
#define F_CPU		16000000UL
#endif

// ILI9341 Command Constants
#define MADCTL_MY	0x80  // Row Address Order
#define MADCTL_MX	0x40  // Column Address Order
#define MADCTL_MV	0x20  // Row/Column Exchange (X and Y swap)
#define MADCTL_BGR	0x08  // BGR Color Order (instead of RGB)

// ---------------------------------------------------------------------------
// Font Data Section
// ---------------------------------------------------------------------------
//
// Standard 5x7 pixel ASCII font used for rendering text characters.
// Each character is represented as 5 bytes (5x7 pixel grid).
//
static const uint8_t font5x7_data[] = {
	// ' ' (0x20)
	0x00,0x00,0x00,0x00,0x00,
	// '!' (0x21)
	0x00,0x00,0x5F,0x00,0x00,
	// '"' (0x22)
	0x00,0x07,0x00,0x07,0x00,
	// '#' (0x23)
	0x14,0x7F,0x14,0x7F,0x14,
	// '$' (0x24)
	0x24,0x2A,0x7F,0x2A,0x12,
	// '%' (0x25)
	0x23,0x13,0x08,0x64,0x62,
	// '&' (0x26)
	0x36,0x49,0x55,0x22,0x50,
	// ''' (0x27)
	0x00,0x05,0x03,0x00,0x00,
	// '(' (0x28)
	0x00,0x1C,0x22,0x41,0x00,
	// ')' (0x29)
	0x00,0x41,0x22,0x1C,0x00,
	// '*' (0x2A)
	0x14,0x08,0x3E,0x08,0x14,
	// '+' (0x2B)
	0x08,0x08,0x3E,0x08,0x08,
	// ',' (0x2C)
	0x00,0x50,0x30,0x00,0x00,
	// '-' (0x2D)
	0x08,0x08,0x08,0x08,0x08,
	// '.' (0x2E)
	0x00,0x60,0x60,0x00,0x00,
	// '/' (0x2F)
	0x20,0x10,0x08,0x04,0x02,
	// '0' (0x30)
	0x3E,0x51,0x49,0x45,0x3E,
	// '1' (0x31)
	0x00,0x42,0x7F,0x40,0x00,
	// '2' (0x32)
	0x42,0x61,0x51,0x49,0x46,
	// '3' (0x33)
	0x21,0x41,0x45,0x4B,0x31,
	// '4' (0x34)
	0x18,0x14,0x12,0x7F,0x10,
	// '5' (0x35)
	0x27,0x45,0x45,0x45,0x39,
	// '6' (0x36)
	0x3C,0x4A,0x49,0x49,0x30,
	// '7' (0x37)
	0x01,0x71,0x09,0x05,0x03,
	// '8' (0x38)
	0x36,0x49,0x49,0x49,0x36,
	// '9' (0x39)
	0x06,0x49,0x49,0x29,0x1E,
	// ':' (0x3A)
	0x00,0x36,0x36,0x00,0x00,
	// ';' (0x3B)
	0x00,0x56,0x36,0x00,0x00,
	// '<' (0x3C)
	0x08,0x14,0x22,0x41,0x00,
	// '=' (0x3D)
	0x14,0x14,0x14,0x14,0x14,
	// '>' (0x3E)
	0x00,0x41,0x22,0x14,0x08,
	// '?' (0x3F)
	0x02,0x01,0x51,0x09,0x06,
	// '@' (0x40)
	0x32,0x49,0x79,0x41,0x3E,
	// 'A' (0x41)
	0x7E,0x11,0x11,0x11,0x7E,
	// 'B' (0x42)
	0x7F,0x49,0x49,0x49,0x36,
	// 'C' (0x43)
	0x3E,0x41,0x41,0x41,0x22,
	// 'D' (0x44)
	0x7F,0x41,0x41,0x22,0x1C,
	// 'E' (0x45)
	0x7F,0x49,0x49,0x49,0x41,
	// 'F' (0x46)
	0x7F,0x09,0x09,0x09,0x01,
	// 'G' (0x47)
	0x3E,0x41,0x49,0x49,0x7A,
	// 'H' (0x48)
	0x7F,0x08,0x08,0x08,0x7F,
	// 'I' (0x49)
	0x00,0x41,0x7F,0x41,0x00,
	// 'J' (0x4A)
	0x20,0x40,0x41,0x3F,0x01,
	// 'K' (0x4B)
	0x7F,0x08,0x14,0x22,0x41,
	// 'L' (0x4C)
	0x7F,0x40,0x40,0x40,0x40,
	// 'M' (0x4D)
	0x7F,0x02,0x0C,0x02,0x7F,
	// 'N' (0x4E)
	0x7F,0x04,0x08,0x10,0x7F,
	// 'O' (0x4F)
	0x3E,0x41,0x41,0x41,0x3E,
	// 'P' (0x50)
	0x7F,0x09,0x09,0x09,0x06,
	// 'Q' (0x51)
	0x3E,0x41,0x51,0x21,0x5E,
	// 'R' (0x52)
	0x7F,0x09,0x19,0x29,0x46,
	// 'S' (0x53)
	0x46,0x49,0x49,0x49,0x31,
	// 'T' (0x54)
	0x01,0x01,0x7F,0x01,0x01,
	// 'U' (0x55)
	0x3F,0x40,0x40,0x40,0x3F,
	// 'V' (0x56)
	0x1F,0x20,0x40,0x20,0x1F,
	// 'W' (0x57)
	0x3F,0x40,0x38,0x40,0x3F,
	// 'X' (0x58)
	0x63,0x14,0x08,0x14,0x63,
	// 'Y' (0x59)
	0x07,0x08,0x70,0x08,0x07,
	// 'Z' (0x5A)
	0x61,0x51,0x49,0x45,0x43,
	// '[' (0x5B)
	0x00,0x7F,0x41,0x41,0x00,
	// '\' (0x5C)
	0x02,0x04,0x08,0x10,0x20,
	// ']' (0x5D)
	0x00,0x41,0x41,0x7F,0x00,
	// '^' (0x5E)
	0x04,0x02,0x01,0x02,0x04,
	// '_' (0x5F)
	0x40,0x40,0x40,0x40,0x40,
	// '`' (0x60)
	0x00,0x03,0x05,0x00,0x00,
	// 'a' (0x61)
	0x20,0x54,0x54,0x54,0x78,
	// 'b' (0x62)
	0x7F,0x48,0x44,0x44,0x38,
	// 'c' (0x63)
	0x38,0x44,0x44,0x44,0x20,
	// 'd' (0x64)
	0x38,0x44,0x44,0x48,0x7F,
	// 'e' (0x65)
	0x38,0x54,0x54,0x54,0x18,
	// 'f' (0x66)
	0x08,0x7E,0x09,0x01,0x02,
	// 'g' (0x67)
	0x0C,0x52,0x52,0x52,0x3E,
	// 'h' (0x68)
	0x7F,0x08,0x04,0x04,0x78,
	// 'i' (0x69)
	0x00,0x44,0x7D,0x40,0x00,
	// 'j' (0x6A)
	0x20,0x40,0x44,0x3D,0x00,
	// 'k' (0x6B)
	0x7F,0x10,0x28,0x44,0x00,
	// 'l' (0x6C)
	0x00,0x41,0x7F,0x40,0x00,
	// 'm' (0x6D)
	0x7C,0x04,0x18,0x04,0x78,
	// 'n' (0x6E)
	0x7C,0x08,0x04,0x04,0x78,
	// 'o' (0x6F)
	0x38,0x44,0x44,0x44,0x38,
	// 'p' (0x70)
	0x7C,0x14,0x14,0x14,0x08,
	// 'q' (0x71)
	0x08,0x14,0x14,0x18,0x7C,
	// 'r' (0x72)
	0x7C,0x08,0x04,0x04,0x08,
	// 's' (0x73)
	0x48,0x54,0x54,0x54,0x20,
	// 't' (0x74)
	0x04,0x3F,0x44,0x40,0x20,
	// 'u' (0x75)
	0x3C,0x40,0x40,0x20,0x7C,
	// 'v' (0x76)
	0x1C,0x20,0x40,0x20,0x1C,
	// 'w' (0x77)
	0x3C,0x40,0x30,0x40,0x3C,
	// 'x' (0x78)
	0x44,0x28,0x10,0x28,0x44,
	// 'y' (0x79)
	0x0C,0x50,0x50,0x50,0x3C,
	// 'z' (0x7A)
	0x44,0x64,0x54,0x4C,0x44,
	// '{' (0x7B)
	0x00,0x08,0x36,0x41,0x00,
	// '|' (0x7C)
	0x00,0x00,0x7F,0x00,0x00,
	// '}' (0x7D)
	0x00,0x41,0x36,0x08,0x00,
	// '~' (0x7E)
	0x02,0x01,0x02,0x04,0x02
};

// Font objects (bitmap, width, height, first char, char count)
Font font5x7  = { font5x7_data, 5, 7, 0x20, 96 };
Font fontLarge = { font5x7_data, 5, 7, 0x20, 96 };

// ---------------------------------------------------------------------------
// SPI Communication Setup
// ---------------------------------------------------------------------------

/**
 * Initialize the SPI interface for communication with the ILI9341.
 * Sets up MOSI and SCK as outputs, MISO as input.
 */
void spi_init(void) {
	SPI_DDR |= (1 << SPI_MOSI) | (1 << SPI_SCK); // MOSI, SCK as output
	SPI_DDR &= ~(1 << SPI_MISO);				 // MISO as input
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0); // SPI Enable, Master, Clock /16
	SPSR |= (1 << SPI2X);						 // Double SPI Speed
}

// ---------------------------------------------------------------------------
// ILI9341 Command and Data Transmission
// ---------------------------------------------------------------------------

/**
 * Send a single command byte to the ILI9341 display.
 */
void ili9341_send_command(uint8_t cmd) {
	DC_COMMAND();   // Set D/C line low for command
	SPI_TRANSFER(cmd); // Send command
}

/**
 * Send a command byte followed by a sequence of data bytes to the ILI9341.
 *
 * cmd - Command byte to send
 * data - Pointer to array of data bytes
 * len - Number of data bytes to send
 */
void ili9341_send_command_bytes(uint8_t cmd, const uint8_t *data, uint8_t len) {
	DC_COMMAND();
	SPI_TRANSFER(cmd);
	DC_DATA();
	for (uint8_t i = 0; i < len; i++) {
		SPI_TRANSFER(data[i]);
	}
}

/**
 * Send a single 8-bit data byte to the ILI9341.
 */
void ili9341_send_data(uint8_t data) {
	DC_DATA();
	SPI_TRANSFER(data);
}

/**
 * Send a single 16-bit data word (high byte first) to the ILI9341.
 */
void ili9341_send_data16(uint16_t data) {
	DC_DATA();
	SPI_TRANSFER(data >> 8);   // High byte
	SPI_TRANSFER(data & 0xFF); // Low byte
}

// ---------------------------------------------------------------------------
// ILI9341 Display Initialization
// ---------------------------------------------------------------------------

/**
 * Initialize the ILI9341 display controller.
 * Performs hardware reset, followed by standard initialization commands.
 */
void ili9341_init(void) {
	// Hardware Reset Sequence
	RST_LOW();
	_delay_ms(50);
	RST_HIGH();
	_delay_ms(150);

	// Software Reset
	ili9341_send_command(0x01);  // Software reset
	_delay_ms(150);

	// Exit Sleep Mode
	ili9341_send_command(0x11);  // Sleep out
	_delay_ms(150);

	// Initialization command sequence
	ili9341_send_command_bytes(0xEF, (uint8_t[]){0x03, 0x80, 0x02}, 3);
	ili9341_send_command_bytes(0xCF, (uint8_t[]){0x00, 0xC1, 0x30}, 3);
	ili9341_send_command_bytes(0xED, (uint8_t[]){0x64, 0x03, 0x12, 0x81}, 4);
	ili9341_send_command_bytes(0xE8, (uint8_t[]){0x85, 0x00, 0x78}, 3);
	ili9341_send_command_bytes(0xCB, (uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5);
	ili9341_send_command_bytes(0xF7, (uint8_t[]){0x20}, 1);
	ili9341_send_command_bytes(0xEA, (uint8_t[]){0x00, 0x00}, 2);

	// Power and VCOM Settings
	ili9341_send_command_bytes(0xC0, (uint8_t[]){0x23}, 1);	   // Power control 1
	ili9341_send_command_bytes(0xC1, (uint8_t[]){0x10}, 1);	   // Power control 2
	ili9341_send_command_bytes(0xC5, (uint8_t[]){0x3E, 0x28}, 2); // VCOM control 1
	ili9341_send_command_bytes(0xC7, (uint8_t[]){0x86}, 1);	   // VCOM control 2

	// Memory Access Control and Pixel Format
	ili9341_send_command_bytes(0x36, (uint8_t[]){MADCTL_MV | MADCTL_BGR}, 1); // Memory Access Control
	ili9341_send_command_bytes(0x3A, (uint8_t[]){0x55}, 1); // Pixel format = 16-bit color

	// Frame Rate and Display Function Control
	ili9341_send_command_bytes(0xB1, (uint8_t[]){0x00, 0x18}, 2);
	ili9341_send_command_bytes(0xB6, (uint8_t[]){0x08, 0x82, 0x27}, 3);

	// Gamma Correction
	ili9341_send_command_bytes(0xF2, (uint8_t[]){0x00}, 1); // 3Gamma Function Disable
	ili9341_send_command_bytes(0x26, (uint8_t[]){0x01}, 1); // Gamma curve selected
	ili9341_send_command_bytes(0xE0, (uint8_t[]){
		0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
		0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00
	}, 15);
	ili9341_send_command_bytes(0xE1, (uint8_t[]){
		0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
		0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F
	}, 15);

	// Display ON
	ili9341_send_command(0x29); // Display ON
	_delay_ms(150);
}

/*-----------------------------------------------------------
  Graphics Drawing Functions
-----------------------------------------------------------*/
void ili9341_set_addr_window(uint16_t x0, uint16_t y0,
uint16_t x1, uint16_t y1)
{
	uint8_t data[4];

	// COLUMN address = X
	data[0] = x0 >> 8;
	data[1] = x0 & 0xFF;
	data[2] = x1 >> 8;
	data[3] = x1 & 0xFF;
	ili9341_send_command_bytes(0x2A, data, 4);

	// ROW address = Y
	data[0] = y0 >> 8;
	data[1] = y0 & 0xFF;
	data[2] = y1 >> 8;
	data[3] = y1 & 0xFF;
	ili9341_send_command_bytes(0x2B, data, 4);

	// prepare to write RAM
	ili9341_send_command(0x2C);
}


// ---------------------------------------------------------------------------
// Basic Drawing Primitives
// ---------------------------------------------------------------------------

/**
 * Draw a single pixel at (x, y) with the specified color.
 */
void drawPixel(uint16_t x, uint16_t y, uint16_t color) {
	if (x >= SCREEN_X || y >= SCREEN_Y)
		return;  // Ignore pixels outside the screen

	ili9341_set_addr_window(x, y, x, y); // Set address window to a single pixel

	DC_DATA();
	SPI_TRANSFER(color >> 8);   // Send high byte
	SPI_TRANSFER(color & 0xFF); // Send low byte
}

/**
 * Fill the entire screen with a single color.
 */
void fillScreen(uint16_t color) {
	uint32_t totalPixels = (uint32_t)SCREEN_X * (uint32_t)SCREEN_Y;
	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;
	uint32_t blocks = totalPixels / 8;  // Send 8 pixels at a time for efficiency
	uint32_t remainder = totalPixels % 8;

	ili9341_set_addr_window(0, 0, SCREEN_X - 1, SCREEN_Y - 1);

	DC_DATA();
	for (uint32_t j = 0; j < blocks; j++) {
		SPI_TRANSFER(hi); SPI_TRANSFER(lo); SPI_TRANSFER(hi); SPI_TRANSFER(lo);
		SPI_TRANSFER(hi); SPI_TRANSFER(lo); SPI_TRANSFER(hi); SPI_TRANSFER(lo);
		SPI_TRANSFER(hi); SPI_TRANSFER(lo); SPI_TRANSFER(hi); SPI_TRANSFER(lo);
		SPI_TRANSFER(hi); SPI_TRANSFER(lo); SPI_TRANSFER(hi); SPI_TRANSFER(lo);
	}
	for (uint32_t j = 0; j < remainder; j++) {
		SPI_TRANSFER(hi);
		SPI_TRANSFER(lo);
	}
}

/**
 * Draw a fast horizontal line from (x, y) with width w and color.
 */
void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
	if (y < 0 || y >= SCREEN_Y)
		return;

	if (x < 0) {
		w += x;
		x = 0;
	}
	if (x + w > SCREEN_X)
		w = SCREEN_X - x;
	if (w <= 0)
		return;

	ili9341_set_addr_window(x, y, x + w - 1, y);

	DC_DATA();
	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;
	for (int16_t i = 0; i < w; i++) {
		SPI_TRANSFER(hi);
		SPI_TRANSFER(lo);
	}
}

/**
 * Draw a fast vertical line from (x, y) with height h and color.
 */
void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
	if (x < 0 || x >= SCREEN_X)
		return;

	if (y < 0) {
		h += y;
		y = 0;
	}
	if (y + h > SCREEN_Y)
		h = SCREEN_Y - y;
	if (h <= 0)
		return;

	ili9341_set_addr_window(x, y, x, y + h - 1);

	DC_DATA();
	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;
	for (int16_t i = 0; i < h; i++) {
		SPI_TRANSFER(hi);
		SPI_TRANSFER(lo);
	}
}

/**
 * Draw a general line from (x0, y0) to (x1, y1) using Bresenham's algorithm.
 */
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
	if (y0 == y1) {
		// Horizontal fast path
		int16_t w = x1 - x0;
		if (w < 0) { x0 += w + 1; w = -w; }
		drawFastHLine(x0, y0, w + 1, color);
		return;
	}
	if (x0 == x1) {
		// Vertical fast path
		int16_t h = y1 - y0;
		if (h < 0) { y0 += h + 1; h = -h; }
		drawFastVLine(x0, y0, h + 1, color);
		return;
	}

	// General Bresenham line algorithm
	int16_t dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
	int16_t dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
	int16_t err = dx + dy, e2;

	while (1) {
		drawPixel(x0, y0, color);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

/**
 * Convert 8-bit RGB components to 16-bit 5-6-5 color format.
 */
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
	uint16_t red   = (r >> 3) & 0x1F; // 5 bits
	uint16_t green = (g >> 2) & 0x3F; // 6 bits
	uint16_t blue  = (b >> 3) & 0x1F; // 5 bits
	return (red << 11) | (green << 5) | blue;
}

/*-----------------------------------------------------------
  Shape Drawing Functions
-----------------------------------------------------------*/

// ---------------------------------------------------------------------------
// Rectangle Drawing Functions
// ---------------------------------------------------------------------------

/**
 * Draw the outline of a rectangle.
 */
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
	if (w <= 0 || h <= 0)
		return;
	drawFastHLine(x, y, w, color);				 // Top edge
	drawFastHLine(x, y + h - 1, w, color);		  // Bottom edge
	drawFastVLine(x, y, h, color);				  // Left edge
	drawFastVLine(x + w - 1, y, h, color);		  // Right edge
}

/**
 * Fill a rectangle with a solid color.
 */
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
	if (w <= 0 || h <= 0)
		return;
	for (int16_t i = 0; i < h; i++) {
		drawFastHLine(x, y + i, w, color);
	}
}

/**
 * Fill *just* the border of a rectangle using 4 strokes, each of thickness `border_size`
 * (the *center* of the drawn strokes are aligned with the original rectangle)
 */
void fillRectBorder(int16_t rect_x, int16_t rect_y, int16_t rect_w, int16_t rect_h, int16_t border_size, uint16_t color) {

	// Stroke alignment coordinates
	int16_t x_left   = rect_x - border_size/2;
	int16_t x_right  = rect_x + rect_w - border_size/2;
	int16_t y_top	= rect_y - border_size/2;
	int16_t y_bottom = rect_y + rect_h - border_size/2;
	int16_t bigger_w = rect_w + border_size;
	int16_t bigger_h = rect_h + border_size;

	// Fill the 4 strokes
	fillRect(x_left, y_top, bigger_w, border_size, color);		// Top stroke
	fillRect(x_right, y_top, border_size, bigger_h, color);		// Right stroke
	fillRect(x_left, y_bottom, bigger_w, border_size, color);	// Bottom stroke
	fillRect(x_left, y_top, border_size, bigger_h, color);		// Left stroke
}

// ---------------------------------------------------------------------------
// Circle Drawing Functions
// ---------------------------------------------------------------------------

/**
 * Draw the outline of a circle using midpoint circle algorithm.
 */
void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	if (r <= 0)
		return;

	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	// Draw initial points on axes
	drawPixel(x0, y0 + r, color);
	drawPixel(x0, y0 - r, color);
	drawPixel(x0 + r, y0, color);
	drawPixel(x0 - r, y0, color);

	// Draw rest of the circle
	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		drawPixel(x0 + x, y0 + y, color);
		drawPixel(x0 - x, y0 + y, color);
		drawPixel(x0 + x, y0 - y, color);
		drawPixel(x0 - x, y0 - y, color);
		drawPixel(x0 + y, y0 + x, color);
		drawPixel(x0 - y, y0 + x, color);
		drawPixel(x0 + y, y0 - x, color);
		drawPixel(x0 - y, y0 - x, color);
	}
}

/**
 * Fill a circle with a solid color.
 */
void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	if (r <= 0)
		return;

	drawFastVLine(x0, y0 - r, 2 * r + 1, color);  // Draw center vertical line

	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		drawFastVLine(x0 + x, y0 - y, 2 * y + 1, color);
		drawFastVLine(x0 - x, y0 - y, 2 * y + 1, color);
		drawFastVLine(x0 + y, y0 - x, 2 * x + 1, color);
		drawFastVLine(x0 - y, y0 - x, 2 * x + 1, color);
	}
}

// ---------------------------------------------------------------------------
// Triangle Drawing Functions
// ---------------------------------------------------------------------------

/**
 * Draw the outline of a triangle.
 */
void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
				  int16_t x2, int16_t y2, uint16_t color) {
	drawLine(x0, y0, x1, y1, color);
	drawLine(x1, y1, x2, y2, color);
	drawLine(x2, y2, x0, y0, color);
}

/**
 * Helper function to swap two int16_t values.
 */
static void swapInt16(int16_t* a, int16_t* b) {
	int16_t t = *a;
	*a = *b;
	*b = t;
}

/**
 * Fill a triangle with a solid color.
 */
void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
				  int16_t x2, int16_t y2, uint16_t color) {
	// Sort coordinates by ascending Y
	if (y0 > y1) { swapInt16(&y0, &y1); swapInt16(&x0, &x1); }
	if (y1 > y2) { swapInt16(&y1, &y2); swapInt16(&x1, &x2); }
	if (y0 > y1) { swapInt16(&y0, &y1); swapInt16(&x0, &x1); }

	if (y0 == y2) {
		// Degenerate case (all points on the same line)
		int16_t minx = x0 < x1 ? x0 : x1;
		if (x2 < minx) minx = x2;
		int16_t maxx = x0 > x1 ? x0 : x1;
		if (x2 > maxx) maxx = x2;
		drawFastHLine(minx, y0, maxx - minx + 1, color);
		return;
	}

	int16_t dx01 = x1 - x0, dy01 = y1 - y0;
	int16_t dx02 = x2 - x0, dy02 = y2 - y0;
	int16_t dx12 = x2 - x1, dy12 = y2 - y1;
	int32_t sa = 0, sb = 0;
	int16_t y, last = (y1 == y2) ? y1 : (y1 - 1);

	// Upper part of the triangle
	for (y = y0; y <= last; y++) {
		int16_t a = x0 + sa / dy01;
		int16_t b = x0 + sb / dy02;
		sa += dx01;
		sb += dx02;
		if (a > b) swapInt16(&a, &b);
		drawFastHLine(a, y, b - a + 1, color);
	}

	// Lower part of the triangle
	sa = (int32_t)dx12 * (y - y1);
	sb = (int32_t)dx02 * (y - y0);
	for (; y <= y2; y++) {
		int16_t a = x1 + sa / dy12;
		int16_t b = x0 + sb / dy02;
		sa += dx12;
		sb += dx02;
		if (a > b) swapInt16(&a, &b);
		drawFastHLine(a, y, b - a + 1, color);
	}
}

// ---------------------------------------------------------------------------
// Rounded Rectangle Functions
// ---------------------------------------------------------------------------

/**
 * Draw the outline of a rounded rectangle.
 */
void drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h,
				   int16_t r, uint16_t color) {
	// Straight edges
	drawFastHLine(x0 + r, y0, w - 2 * r, color);		  // Top
	drawFastHLine(x0 + r, y0 + h - 1, w - 2 * r, color);   // Bottom
	drawFastVLine(x0, y0 + r, h - 2 * r, color);		   // Left
	drawFastVLine(x0 + w - 1, y0 + r, h - 2 * r, color);   // Right

	// Rounded corners
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		drawPixel(x0 + r - x, y0 + r - y, color);				   // Top-left
		drawPixel(x0 + r - y, y0 + r - x, color);
		drawPixel(x0 + w - r - 1 + x, y0 + r - y, color);			// Top-right
		drawPixel(x0 + w - r - 1 + y, y0 + r - x, color);
		drawPixel(x0 + r - x, y0 + h - r - 1 + y, color);			// Bottom-left
		drawPixel(x0 + r - y, y0 + h - r - 1 + x, color);
		drawPixel(x0 + w - r - 1 + x, y0 + h - r - 1 + y, color);	// Bottom-right
		drawPixel(x0 + w - r - 1 + y, y0 + h - r - 1 + x, color);
	}
}

/**
 * Fill a rounded rectangle with a solid color.
 */
void fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h,
				   int16_t r, uint16_t color) {
	fillRect(x0 + r, y0, w - 2 * r, h, color);  // Center rectangle
	fillCircleHelper(x0 + r, y0 + r, r, 1, h - 2 * r, color);		// Left corners
	fillCircleHelper(x0 + w - r - 1, y0 + r, r, 2, h - 2 * r, color); // Right corners
}

/**
 * Helper function for filling circle corners.
 */
void fillCircleHelper(int16_t x0, int16_t y0, int16_t r,
					  uint8_t corners, int16_t delta, uint16_t color) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		if (corners & 0x1) {
			drawFastVLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
			drawFastVLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
		}
		if (corners & 0x2) {
			drawFastVLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
			drawFastVLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
		}
	}
}

// ---------------------------------------------------------------------------
// Text Rendering Functions
// ---------------------------------------------------------------------------

/**
 * Draw a single character at (x, y) with specified color, background, size, font, and rotation.
 *
 * Rotation options:
 *   0 = 0? normal
 *   1 = 90? clockwise
 *   2 = 180? upside down
 *   3 = 270? clockwise
 */
void drawChar(int16_t x, int16_t y, char c,
			  uint16_t color, uint16_t bg,
			  uint8_t size, const Font *font,
			  uint8_t rotation)
{
	if (c < font->first || c >= font->first + font->count)
		return;  // Skip unsupported characters

	uint16_t idx = (c - font->first) * font->width;  // Index into font bitmap
	uint8_t w = font->width;
	uint8_t h = font->height;

	for (uint8_t i = 0; i < w; i++) {
		uint8_t line = font->bitmap[idx + i];

		for (uint8_t j = 0; j < h; j++, line >>= 1) {
			bool pixelOn = (line & 0x01);
			int16_t dx, dy;

			// Determine pixel offset based on rotation
			switch (rotation & 3) {
				case 1:  // 90 Degrees CW
					dx = h - 1 - j;
					dy = i;
					break;
				case 2:  // 180 Degrees
					dx = w - 1 - i;
					dy = h - 1 - j;
					break;
				case 3:  // 270 Degrees CW
					dx = j;
					dy = w - 1 - i;
					break;
				default: // 0 Degrees
					dx = i;
					dy = j;
			}

			int16_t px = x + dx * size;
			int16_t py = y + dy * size;

			if (pixelOn) {
				if (size == 1)
					drawPixel(px, py, color);
				else
					fillRect(px, py, size, size, color);
			} else if (bg != color) {
				if (size == 1)
					drawPixel(px, py, bg);
				else
					fillRect(px, py, size, size, bg);
			}
		}
	}
}

/**
 * Draw a string at (x, y) using the specified font, with support for newline and rotation.
 *
 * Newline behavior depends on rotation:
 *   0 = move downward on newline
 *   1 = move left on newline
 *   2 = move upward on newline
 *   3 = move right on newline
 */
void drawString(int16_t x, int16_t y, const char *s, uint16_t color, uint16_t bg,
				uint8_t size, const Font *font, uint8_t rotation)
{
	int16_t deltaX = font->width  * size + 1;  // Character width (scaled) + 1px spacing
	int16_t deltaY = font->height * size + 1;  // Character height (scaled) + 1px spacing
	int16_t gap	= size;					 // Extra gap between lines

	// Step direction and newline jump based on rotation
	int16_t stepX, stepY, nlX, nlY;

	switch (rotation & 3) {
		case 0: // 0 Degrees
			stepX = deltaX; stepY = 0;
			nlX = 0; nlY = deltaY + gap;
			break;
		case 1: // 90 Degrees CW
			stepX = 0; stepY = deltaX;
			nlX = -(deltaY + gap); nlY = 0;
			break;
		case 2: // 180 Degrees
			stepX = -deltaX; stepY = 0;
			nlX = 0; nlY = -(deltaY + gap);
			break;
		case 3: // 270 Degrees CW
			stepX = 0; stepY = -deltaX;
			nlX = deltaY + gap; nlY = 0;
			break;
	}

	int16_t startX = x, startY = y;
	int line = 0;
	int16_t cx = startX, cy = startY;

	while (*s) {
		if (*s == '\n') {
			// Newline: move start position
			line++;
			cx = startX + nlX * line;
			cy = startY + nlY * line;
		} else {
			drawChar(cx, cy, *s, color, bg, size, font, rotation);
			cx += stepX;
			cy += stepY;
		}
		s++;
	}
}
