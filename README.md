# AVRmada (Battleship for the ATMega328P)

> **Authors:** Peter Kamp and Brendan Brooks
> **Version:** 1.1 
> **Copyright:** 2025

A two-player Battleship game designed to run on bare-metal AVR C (no Arduino libraries), featuring:
- **Joystick-controlled ship placement**
- **Multiplayer over UART serial communication**
- **Graphics on a TFT LCD (ILI9341 controller)**
- **Custom lightweight graphics library (`gfx`) for embedded displays**

---

## Project Structure

| File | Description |
|:---|:---|
| `main.c` | Core game loop, multiplayer state machine, UART communication handling. |
| `battleship_utils.c` | Helper functions for board management, joystick and button input, ship placement, and drawing. |
| `battleship_utils.h` | Data structures, constants, and function prototypes shared across the project. |
| `gfx.c` | Low-level graphics driver for the TFT screen (ILI9341 controller). Supports pixel drawing, lines, rectangles, circles, text rendering, etc. |
| `gfx.h` | Header file for `gfx.c`, including screen size definitions, control macros, and graphics function prototypes. |

---

## Features

- **Graphical Grids:** Player and enemy boards are drawn on the TFT display.
- **Joystick Navigation:** Move a cursor around the board to place ships and select shots.
- **Button Input:** Short press to place a ship, long press to rotate it.
- **Ghost Preview:** Visual feedback showing valid/invalid ship placements.
- **Networking:** Serial communication (UART) automatically synchronizes attacks and results.
- **Randomized AI Fleet:** When starting, the enemy’s fleet is randomly placed.

---

## Hardware Requirements

- **Microcontroller:** ATmega328P (or equivalent AVR microcontroller)
- **Display:** TFT LCD with ILI9341 controller
- **Joystick:** 2-axis analog joystick connected to ADC0 and ADC1
- **Button:** Single digital input button (connected to PD2)
- **Serial/UART:** TX/RX connected between devices for multiplayer communication
- **Clock:** 16 MHz external oscillator (F_CPU is assumed to be 16 MHz)

---

## Connections

| Signal | ATmega328P Pin |
|:---|:---|
| TFT DC | PB1 |
| TFT RESET | PB0 |
| TFT MOSI | PB3 |
| TFT SCK | PB5 |
| Joystick X | ADC0 (PC0) |
| Joystick Y | ADC1 (PC1) |
| Button | PD2 |
| UART TX | PD1 |
| UART RX | PD0 |

---

## Game Controls

- **Move Cursor:** Tilt the joystick (left/right/up/down)
- **Rotate Ship (Placement Phase):** Hold button for >500 ms
- **Place Ship / Fire at Enemy:** Tap button quickly

---

## Multiplayer Setup
- Connect two devices via their UART ports (cross TX/RX).
- After ship placement, devices exchange random tokens to decide who goes first.
- Players take turns firing at each other’s grids.
- Results (hit/miss) are communicated automatically and update the display.
- If a device does not respond within a timeout, the game automatically resets.

---

## Graphics and Fonts

- Resolution: 320x240 pixels
- Color Format: RGB565 (16-bit)
- Text Font: Built-in 5x7 pixel font
- Basic graphics operations like drawPixel(), fillScreen(), drawLine(), drawRect(), drawCircle(), and text rendering (drawString()) are implemented for efficiency on low-resource devices.

---

## Future Improvements

- [x] Implement joystick and button input handling
- [x] Build lightweight graphics library for TFT (ILI9341)
- [x] Add ship placement ghost preview (valid/invalid indicators)
- [x] Create multiplayer Battleship core logic over UART
- [x] Handle UART retransmissions and peer timeouts
- [ ] Add sound effects on hit/miss (e.g., simple buzzer output)
- [ ] Improve text prompts (sunken battleship indicator, etc.)
- [ ] Add single-player mode (AI opponent with basic strategy)
- [ ] Add single-player vs multi-player selector menu
- [ ] Improve UI with animations for hits and sunk ships
- [ ] Implement a settings menu (e.g., difficulty, colors, etc.)

---

## Notes
- No Arduino libraries required: All low-level register control and peripherals are manually configured and self contained within the project.
- Robust serial communication: Includes retransmissions and timeout handling to ensure multiplayer gameplay even under occasional packet loss.
- The graphics library (gfx.c) can be reused for other TFT projects as a lightweight alternative to full libraries like Adafruit GFX.
