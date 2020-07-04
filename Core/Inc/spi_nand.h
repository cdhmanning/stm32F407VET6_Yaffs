
#ifndef __SPI_NAND_H__
#define __SPI_NAND_H__
#include <stdint.h>

int spi_nand_init(void);
int spi_nand_reset(void);

void spi_nand_test(void);

#endif /* __SPI_NAND_H__ */
