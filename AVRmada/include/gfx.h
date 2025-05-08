/* ---------------------------------------------------------------------------
 * gfx.h - Header for TFT Display Graphics Driver (ILI9341)
 *
 * Provides:
 * - SPI and ILI9341 setup
 * - Basic drawing primitives (pixel, line, rectangle, circle, etc.)
 * - Text and font rendering utilities
 *
 * v1.2 - Initial Sound Effects
 * Copyright (c) 2025 Peter Kamp
 * --------------------------------------------------------------------------- */

#ifndef GFX_H
#define GFX_H

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <stdint.h>			// Standard integer types
#include <avr/io.h>			// AVR hardware IO definitions
#include <util/delay.h>		// Delay functions
#include <stdlib.h>			// Standard functions (abs())

/* ---------------------------------------------------------------------------
 * Screen Resolution Constants
 * --------------------------------------------------------------------------- */
#define SCREEN_X			320
#define SCREEN_Y			240

/* ---------------------------------------------------------------------------
 * Pin and Port Assignments
 * --------------------------------------------------------------------------- */

/* Chip Select (CS) for ILI9341 (Unused mostly for our HW implementation) */
#define ILI9341_CS_PORT		PORTB
#define ILI9341_CS_DDR		DDRB
#define ILI9341_CS_PIN		PB2

/* Data/Command (DC) control */
#define ILI9341_DC_PORT		PORTB
#define ILI9341_DC_DDR		DDRB
#define ILI9341_DC_PIN		PB1

/* Reset (RST) control */
#define ILI9341_RST_PORT	PORTB
#define ILI9341_RST_DDR		DDRB
#define ILI9341_RST_PIN		PB0

/* ---------------------------------------------------------------------------
 * SPI Pin Definitions (ATmega328P)
 * --------------------------------------------------------------------------- */
#define SPI_DDR				DDRB
#define SPI_PORT			PORTB
#define SPI_MOSI			PB3
#define SPI_MISO			PB4  // Optional, only needed if reading from display
#define SPI_SCK				PB5

/* ---------------------------------------------------------------------------
 * Control Signal Macros
 * --------------------------------------------------------------------------- */

/* Control Reset (RST) Line */
#define RST_LOW()			(ILI9341_RST_PORT &= ~(1 << ILI9341_RST_PIN))
#define RST_HIGH()			(ILI9341_RST_PORT |=  (1 << ILI9341_RST_PIN))

/* Control Data/Command (DC) Line */
#define DC_COMMAND() do { ILI9341_DC_PORT &= ~(1 << ILI9341_DC_PIN); \
__asm__ volatile("nop\n nop"); } while (0)

#define DC_DATA() do { ILI9341_DC_PORT |= (1 << ILI9341_DC_PIN); \
__asm__ volatile("nop\n nop"); } while (0)

/* ---------------------------------------------------------------------------
 * SPI Transfer Macro
 * --------------------------------------------------------------------------- */

// Send a single byte via SPI
#define SPI_TRANSFER(byte)   \
do {					 \
	SPDR = (byte);		\
	while (!(SPSR & (1 << SPIF))); \
} while (0)

/* ---------------------------------------------------------------------------
 * Font Data Structures
 * --------------------------------------------------------------------------- */

/* Font object structure for bitmap fonts */
typedef struct {
	const uint8_t *bitmap;	// Pointer to raw bitmap data
	uint8_t width;			// Character width in pixels
	uint8_t height;			// Character height in pixels
	char first;				// ASCII code of first character
	uint8_t count;			// Number of characters in the font
} Font;

/* External font declarations */
extern Font font5x7;
extern Font fontLarge;

/* ---------------------------------------------------------------------------
 * Function Prototypes
 * --------------------------------------------------------------------------- */

/* SPI and ILI9341 initialization */
void	spi_init(void);
void	ili9341_init(void);

/* Command and data transmission */
void	ili9341_send_command(uint8_t cmd);
void	ili9341_send_command_bytes(uint8_t cmd, const uint8_t *data, uint8_t len);
void	ili9341_send_data(uint8_t data);
void	ili9341_send_data16(uint16_t data);

/* Low-level graphics primitives */
void	ili9341_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void	drawPixel(uint16_t x, uint16_t y, uint16_t color);
void	fillScreen(uint16_t color);

/* Fast horizontal and vertical line drawing */
void	drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void	drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void	drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/* Shape drawing functions */
void	drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void	fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void	fillRectBorder(int16_t rect_x, int16_t rect_y, int16_t rect_w, int16_t rect_h, int16_t border_size, uint16_t color);
void	drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void	fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void	drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void	fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void	drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color);
void	fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color);
void	fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, uint16_t color);

/* Text rendering functions */
void	drawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size, const Font *font, uint8_t rotation);
void	drawString(int16_t x, int16_t y, const char *s, uint16_t color, uint16_t bg, uint8_t size, const Font *font, uint8_t rotation);
void	drawString_P(int16_t x, int16_t y, const char *s_progmem, uint16_t color, uint16_t bg,
						uint8_t size, const Font *font, uint8_t rotation);

/* Color helper */
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);

#endif  // GFX_H
