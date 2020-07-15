
#ifndef __SPI_NAND_H__
#define __SPI_NAND_H__
#include <stdint.h>

struct spi_nand_buffer_op {
	uint32_t offset;
	uint8_t *buffer;
	uint32_t nbytes;
};

int spi_nand_read_page(uint32_t page,
					   struct spi_nand_buffer_op *ops,
					   uint32_t n_ops,
					   uint8_t *statusptr);

int spi_nand_write_page(uint32_t page,
		   	   	   	    struct spi_nand_buffer_op *ops,
		   	   	   	    uint32_t n_ops,
		   	   	   	    uint8_t *statusptr);

int spi_nand_erase_block(uint32_t block, uint8_t *statusptr);

int spi_nand_check_block_ok(uint32_t block, uint32_t *is_ok);

int spi_nand_mark_block_bad(uint32_t block, uint8_t *status);

int spi_nand_init(void);
int spi_nand_reset(void);


void spi_nand_test(void);

#endif /* __SPI_NAND_H__ */
