/*
 * spi_nand.c
 *
 *  Created on: 21/06/2020
 *      Author: charles
 */

#if 0

static int spi_band_transaction(unsigned op_code,
								unsigned address,
								unsigned n_address_bytes,
								unsigned n_dummy_bytes,
								unsigned char *data_bytes,
								unsigned n_data_bytes)
{

}

static int spi_nand_status(unsigned *status)
{

}


int spi_nand_init(void)
{

}

int spi_nand_reset(void)
{

}

int spi_nand_read_id(void)
{

}

int spi_nand_erase_block(unsigned block)
{

}

int spi_nand_write_enable(int enable)
{

}

int spi_nand_page_read_to_cache(unsigned page)
{

}

int spi_nand_read_from_cache(uint8_t *buffer, unsigned buffer_size, unsigned offset)
{

}

int spi_nand_program_load(const uint8_t *buffer, unsigned buffer_size, unsigned offset)
{

}


int spi_nand_program_execute(unsigned page)
{

}

int spi_nand_test(void)
{

}

#endif

