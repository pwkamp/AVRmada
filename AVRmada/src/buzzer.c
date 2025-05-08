/* ---------------------------------------------------------------------------
 * buzzer.c - Implementation for PWM Driven Passive Buzzer
 *
 * Provides the ability to play predetermined sounds via a passive buzzer
 * over PWM. Sounds are build up on Square Wave Tones or
 * Simulated Triangle/Sawtooth waves (mostly for 'noise' generation)
 *
 * v2.0
 * Copyright (c) 2025 Peter Kamp
 * --------------------------------------------------------------------------- */
#include <avr/io.h>
#include "buzzer.h"
#include <util/delay.h>

/* -------------------------------------------------------------------------
 *  Sound building block prototypes
 * ------------------------------------------------------------------------- */
void play_waveform(Note note);
void play_tone(uint16_t frequency);
void stop_tone(void);
void delay_variable(uint16_t time);

void play_hit_sound(void);
void play_miss_sound(void);
void play_enemy_hit_sound(void);
void play_enemy_miss_sound(void);

/**
 * Play either the hit or miss sound effect depending on hit status, only if
 * sounds are enabled
 */
void play_attack_sound(const bool *hit, const bool *soundsEnabled) {
	if (!hit || !soundsEnabled || !*soundsEnabled) return;
	*hit ? play_hit_sound() : play_miss_sound();
}

/**
 * Hit sound effect
 */
void play_hit_sound() {
	for (uint16_t f = 250; f <= 3000; f += 6) {
		play_waveform((Note){f, 5, WAVEFORM_SQUARE});
	}
	_delay_ms(500);
	for (uint16_t f = 1000; f >= 200; f -= 10) {
		play_waveform((Note){f, 1, WAVEFORM_TRIANGLE});
	}
}

/**
 * Miss sound effect
 */
void play_miss_sound() {
	for (uint16_t f = 250; f <= 3000; f += 6) {
		play_waveform((Note){f, 5, WAVEFORM_SQUARE});
	}
	_delay_ms(500);
	play_waveform((Note){300, 300, WAVEFORM_SQUARE});
	play_waveform((Note){287, 700, WAVEFORM_SQUARE});
}

void play_enemy_attack_sound(const bool *hit, const bool *soundsEnabled) {
	if (!hit || !soundsEnabled || !*soundsEnabled) return;
	*hit ? play_enemy_hit_sound() : play_enemy_miss_sound();
}

void play_enemy_hit_sound(void) {
	play_waveform((Note){523, 100, WAVEFORM_SQUARE}); // C5
	play_waveform((Note){415, 100, WAVEFORM_SQUARE}); // G#4 (dissonance)
	play_waveform((Note){370, 200, WAVEFORM_SQUARE}); // F#4 (falls)
}

void play_enemy_miss_sound(void) {
	play_waveform((Note){659, 100, WAVEFORM_SQUARE}); // E5
	_delay_ms(60); // short pause
	play_waveform((Note){659, 100, WAVEFORM_SQUARE}); // E5 again
}

void play_radar_sound(const bool *hit, const bool *soundsEnabled) {
	if (!hit || !soundsEnabled || !*soundsEnabled) return;
	// Radar sweep: rising tone over ~1 second
	for (uint16_t f = 400; f <= 1000; f += 20) {
		play_waveform((Note){f, 15, WAVEFORM_SQUARE});
	}
	
	_delay_ms(100);
	
	for (uint16_t f = 400; f <= 1000; f += 20) {
		play_waveform((Note){f, 15, WAVEFORM_SQUARE});
	}
	
	_delay_ms(100);
	
	for (uint16_t f = 400; f <= 1000; f += 20) {
		play_waveform((Note){f, 15, WAVEFORM_SQUARE});
	}

	_delay_ms(200); // Slight pause before lock-on
	
	if (*hit) {
		// Lock-on: a two-tone chirp
		play_waveform((Note){523, 150, WAVEFORM_SQUARE}); // C5
		play_waveform((Note){659, 300, WAVEFORM_SQUARE}); // E5 (rising and holding)
	} else {
		// No Lock-on (Radar Miss)
		play_waveform((Note){330, 180, WAVEFORM_SQUARE}); // E4
		play_waveform((Note){262, 300, WAVEFORM_SQUARE}); // C4 (lower, minor feel)
	}
}






void play_win_sound(const bool *soundsEnabled) {
	if (!soundsEnabled || !*soundsEnabled) return;
	// Simple ascending major-like notes
	play_waveform((Note){523, 150, WAVEFORM_SQUARE}); // C5
	play_waveform((Note){587, 150, WAVEFORM_SQUARE}); // D5
	play_waveform((Note){659, 150, WAVEFORM_SQUARE}); // E5
	play_waveform((Note){698, 150, WAVEFORM_SQUARE}); // F5
	play_waveform((Note){784, 300, WAVEFORM_SQUARE}); // G5 (hold)

	// Optional resolution flourish
	play_waveform((Note){880, 150, WAVEFORM_SQUARE}); // A5
	play_waveform((Note){784, 300, WAVEFORM_SQUARE}); // Back to G5
}

void play_lose_sound(const bool *soundsEnabled) {
	if (!soundsEnabled || !*soundsEnabled) return;
	
	play_waveform((Note){293, 150, WAVEFORM_SQUARE});
	play_waveform((Note){430, 150, WAVEFORM_SQUARE});
	play_waveform((Note){293, 150, WAVEFORM_SQUARE});

	// Slide
	uint16_t start_freq = 293;
	uint16_t end_freq = 310;
	uint16_t slide_duration = 400;
	uint8_t step_size = 1; // 1 Hz steps for smoothness
	uint16_t steps = (end_freq - start_freq) / step_size;
	if (steps == 0) steps = 1;
	uint16_t step_delay = slide_duration / steps;

	for (uint16_t f = start_freq; f <= end_freq; f += step_size) {
		play_tone(f);
		delay_variable(step_delay);
	}
	stop_tone();
}



/**
 * Play a buzzer tone at a given frequency
 */
void play_tone(uint16_t frequency) {
	DDRB |= (1 << BUZZER_PIN);
	TCCR1A = (1 << COM1B1) | (1 << WGM11);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS10);
	uint16_t top = (F_CPU / frequency) - 1;
	ICR1 = top;
	OCR1B = top / 2;
}

/**
 * Turn off currently playing a buzzer tone
 */
void stop_tone(void) {
	TCCR1A = 0;
	TCCR1B = 0;
	PORTB &= ~(1 << BUZZER_PIN);
}

/**
 * Convenient wrapper for play_tone()
 * Uses a 'Note' with a frequency, waveform type, and duration
 */
void play_waveform(Note note) {
	if (note.frequency == 0) {
		stop_tone();
		delay_variable(note.duration);
		return;
	}
	uint16_t start = note.frequency;
	uint16_t end = note.frequency * 2;
	uint8_t step_size = 5;
	uint16_t steps = (end - start) / step_size;
	if (!steps) steps = 1;
	uint16_t step_delay = note.duration / steps;

	switch (note.waveform) {
		case WAVEFORM_SQUARE:
		play_tone(note.frequency);
		delay_variable(note.duration);
		stop_tone();
		break;
		case WAVEFORM_TRIANGLE:
		for (uint16_t f = start; f <= end; f += step_size) {
			play_tone(f);
			delay_variable(step_delay);
		}
		for (uint16_t f = end; f > start; f -= step_size) {
			play_tone(f);
			delay_variable(step_delay);
		}
		stop_tone();
		break;
		case WAVEFORM_SAWTOOTH:
		for (uint16_t f = start; f <= end; f += step_size) {
			play_tone(f);
			delay_variable(step_delay);
		}
		stop_tone();
		break;
	}
}

void delay_variable(uint16_t time) {
	while (time--) _delay_ms(1);
}
