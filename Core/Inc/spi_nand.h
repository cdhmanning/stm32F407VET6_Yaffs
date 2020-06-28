
#ifndef __SPI_NAND_H__
#define __SPI_NAND_H__
#include <stdint.h>

int spi_nand_init(void);
int spi_nand_reset(void);

int spi_nand_read_id(uint8_t id[2]);
int spi_nand_write_enable(int enable);
int spi_nand_page_read_to_cache(uint32_t page);
int spi_nand_read_from_cache(uint32_t offset, uint8_t *buffer, uint32_t buffer_size);
int spi_nand_program_load(uint32_t offset, const uint8_t *buffer, uint32_t buffer_size);
int spi_nand_program_execute(uint32_t page);
int spi_nand_erase_block(uint32_t block);

void spi_nand_test(void);

#endif /* __SPI_NAND_H__ */
