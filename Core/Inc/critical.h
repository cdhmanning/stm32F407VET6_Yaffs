#ifndef __CRITICAL_H__
#define __CRITICAL_H__
#include <stdint.h>
#include "stm32f4xx_hal.h"


static inline uint32_t critical_lock(void)
{
	uint32_t ret = __get_PRIMASK();

	asm volatile("" ::: "memory");

	__set_PRIMASK(1);	/* Disables interrupts. */
	__DMB();			/* Make sure memory is sane before progressing. */
	return ret;
}

static inline void critical_unlock(uint32_t crit_flags)
{
	asm volatile("" ::: "memory");

	__DMB();		/* Make sure memory is sane before progressing. */
	__set_PRIMASK(crit_flags);
}

#endif
