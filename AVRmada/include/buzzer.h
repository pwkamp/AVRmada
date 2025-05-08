/* ---------------------------------------------------------------------------
 * buzzer.h - Header for PWM Driven Passive Buzzer
 *
 * Provides the ability to play predetermined sounds via a passive buzzer
 * over PWM.
 *
 * v2.0
 * Copyright (c) 2025 Peter Kamp
 * --------------------------------------------------------------------------- */

#ifndef BUZZER_H
#define BUZZER_H

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <stdint.h>
#include <stdbool.h>
#include <util/delay.h>

#define BUZZER_PIN PB2
#define WAVEFORM_SQUARE   0
#define WAVEFORM_TRIANGLE 1
#define WAVEFORM_SAWTOOTH 2

typedef struct {
	uint16_t frequency;
	uint16_t duration;
	uint8_t waveform;
} Note;

// Sound effect prototypes
void play_attack_sound(const bool *hit, const bool *soundsEnabled);
void play_win_sound(const bool *soundsEnabled);
void play_lose_sound(const bool *soundsEnabled);
void play_enemy_attack_sound(const bool *hit, const bool *soundsEnabled);

void play_radar_sound(const bool *hit, const bool *soundsEnabled);

#endif
