/* ---------------------------------------------------------------------------
 * eeprom.h - Function and Constant Definitions for EEPROM Image reading/writing
 *
 * v2.0
 * Copyright (c) 2025 Peter Kamp and Brendan Brooks
 * --------------------------------------------------------------------------- */

#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>
#include <avr/eeprom.h>

#define IMG_WIDTH          45
#define IMG_HEIGHT         45
#define IMG_PIXELS         (IMG_WIDTH * IMG_HEIGHT)
#define IMG_BYTES          ((IMG_PIXELS + 1) >> 1)  // Two pixels per byte
#define EEPROM_IMAGE_ADDR  ((uint16_t)0x0000)		// EEPROM base address

/* Populate (and/or clear) the EEPROM image region.
 * - If FLASH_IMAGE is defined, this copies the PROGMEM image to EEPROM.
 * - If CLEAR_EEPROM is defined, this zeroes out IMG_BYTES bytes.
 * - Can do both if both are defined.
 */
void initEepromImage(void);

/* Draw the image from EEPROM at (x, y) at an integer scale. */
void displayImage(int16_t x, int16_t y, uint8_t scale);

#endif // EEPROM_H_
