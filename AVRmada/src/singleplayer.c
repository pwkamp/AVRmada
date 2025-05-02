#include "singleplayer.h"
#include <string.h>
#include <stdio.h>

extern void net_inject_line(const char *line);

/* ------------------------------------------------------------------ */
/* Simple circular queue of pending lines to inject on the next tick  */
/* ------------------------------------------------------------------ */
#define QCAP 4
static char  qbuf[QCAP][32];
static uint8_t qhead = 0, qtail = 0;

static inline bool q_full (void) { return (uint8_t)(qhead + 1) % QCAP == qtail; }
static inline bool q_empty(void) { return qhead == qtail; }

static void q_push(const char *s)
{
	if (q_full()) return;          /* drop if ever over?run – harmless here */
	strncpy(qbuf[qhead], s, 31);
	qbuf[qhead][31] = '\0';
	qhead = (uint8_t)(qhead + 1) % QCAP;
}

/* PUBLIC ------------------------------------------------------------------ */

void sp_reset(void)
{
	qhead = qtail = 0;
}

void sp_tick(void)
{
	if (q_empty()) return;
	net_inject_line(qbuf[qtail]);
	qtail = (uint8_t)(qtail + 1) % QCAP;
}

/* ------------- spoofed TX helpers --------------------------------------- */

void sp_on_tx_ready(uint16_t self_token)
{
	char line[32];
	snprintf(line, sizeof(line), "READY %u", 1);   /* static peer token = 1 */
	q_push(line);
}

void sp_on_tx_attack(uint8_t row, uint8_t col)
{
	char line[32];

	/* 1) send RESULT (always miss) */
	snprintf(line, sizeof(line), "R %u %u M", row, col);
	q_push(line);

	/* 2) opponent fires back at same square */
	snprintf(line, sizeof(line), "A %u %u", row, col);
	q_push(line);
}

void sp_on_tx_result(uint8_t row, uint8_t col, bool hit)
{
	/* not used by echo AI – ignore */
	(void)row; (void)col; (void)hit;
}
