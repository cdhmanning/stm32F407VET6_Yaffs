#include "cdc_print_sink.h"
#include "usbd_cdc_if.h"

#include <stdint.h>

static volatile uint8_t  tx_buffer[256];
static volatile uint32_t tx_head;
static volatile uint32_t tx_tail;

#define tx_buffer_mask (sizeof(tx_buffer) - 1)

static inline uint32_t tx_n_holding(void)
{
	return (tx_head - tx_tail) & tx_buffer_mask;
}

static inline uint32_t tx_has_space(void)
{
	return (tx_head + 1 - tx_tail) & tx_buffer_mask;
}


static void tx_buffer_copy(uint8_t *buffer, uint32_t buffer_size)
{
	while (buffer_size & 3) {
		*buffer = tx_buffer[tx_tail & tx_buffer_mask];
		buffer++;
		tx_tail++;
		buffer_size--;
	}

	while (buffer_size) {
		*buffer = tx_buffer[tx_tail & tx_buffer_mask];
		buffer++;
		tx_tail++;
		*buffer = tx_buffer[tx_tail & tx_buffer_mask];
		buffer++;
		tx_tail++;
		*buffer = tx_buffer[tx_tail & tx_buffer_mask];
		buffer++;
		tx_tail++;
		*buffer = tx_buffer[tx_tail & tx_buffer_mask];
		buffer++;
		tx_tail++;
		buffer_size-=4;
	}
}

static inline uint32_t tx_fetch(uint8_t *buffer, uint32_t buffer_size)
{
	uint32_t holding = tx_n_holding();

	if (holding < buffer_size)
		buffer_size = holding;

	tx_buffer_copy(buffer, buffer_size);

	return buffer_size;
}


static uint32_t cdc_print_sink_initialised;
static uint32_t cdc_sending;
static uint8_t pc_buffer[64];
static uint32_t pc_buffer_holding;

static void pc_flush(void)
{
	int ret;

	if (!cdc_print_sink_initialised)
		return;

	if (cdc_sending)
		return;

	cdc_sending = 1;

	if (!pc_buffer_holding)
		pc_buffer_holding = tx_fetch(pc_buffer, sizeof(pc_buffer));

	if (!pc_buffer_holding) {
		cdc_sending = 0;
		return;
	}

	ret = CDC_Transmit_FS(pc_buffer, pc_buffer_holding);

	if (ret == 0) {
			pc_buffer_holding = 0;
	}
}

static inline void tx_add(uint8_t ch)
{
	while (!tx_has_space()) {
		/* Spin. */
	}
	tx_buffer[tx_head &tx_buffer_mask] = ch;
	tx_head++;

	if (tx_n_holding() >= sizeof(pc_buffer)) {
		pc_flush();
	}
}

int __io_putchar(int ch)
{

	uint8_t x = (uint8_t) ch;
#if 0
	uint32_t cflags;

	cflags = critical_lock();
	if (pc_buffer_holding < sizeof(pc_buffer)) {
		pc_buffer[pc_buffer_holding] = x;
		pc_buffer_holding++;
	}
	critical_unlock(cflags);
#else
	tx_add(x);
#endif
	return ch;
}

void cdc_print_sink_tick(void)
{
	pc_flush();
}

void cdc_print_sink_init(void)
{
	cdc_print_sink_initialised = 1;
}

void cdc_print_sink_deinit(void)
{
	cdc_print_sink_initialised = 0;
}

void cdc_print_sink_transmit_complete(void)
{
	cdc_sending = 0;
	pc_flush();
}

