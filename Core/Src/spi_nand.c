#include "spi_nand.h"
#include "gpio.h"
#include "spi.h"

#define nand_spi hspi3

static int spi_nand_transaction(uint8_t *header,
								 uint32_t header_size,
								 uint32_t data_is_tx,
								 uint8_t *data,
								 uint32_t data_size)
{
	int ret = 0;

	gpio_NAND_CS(0);
	if (header && header_size)
		ret = HAL_SPI_Transmit(&nand_spi, header, header_size, 10);
	if (data_is_tx && data && data_size)
		ret = HAL_SPI_Transmit(&nand_spi, header, header_size, 10);
	else if (data && data_size)
		ret = HAL_SPI_Receive(&nand_spi, header, header_size, 10);
	gpio_NAND_CS(1);

	return ret;
}

static int spi_nand_no_addr_transaction(uint8_t op_code,
									    uint32_t n_dummy_bytes,
										uint32_t data_is_tx,
										uint8_t *data,
										uint32_t data_size)
{
	uint8_t header[2];
	uint32_t header_size;

	header[0] = op_code;
	header_size = 1 + n_dummy_bytes;
	return spi_nand_transaction(header, header_size,
						 	    data_is_tx,
								data, data_size);
}

static int spi_nand_no_data_transaction(uint8_t op_code)
{
	return spi_nand_transaction(&op_code, 1,
						 	    0, NULL, 0);
}

static int spi_nand_addr_transaction(uint8_t op_code,
								uint32_t address,
								uint32_t n_address_bytes,
								uint32_t n_dummy_bytes,
								uint32_t data_is_tx,
								uint8_t *data,
								uint32_t data_size)
{
	uint8_t header[5];
	uint32_t header_size;

	header[0]= op_code;
	header_size = 1;

	if (n_address_bytes == 1) {
		header[1] = address;
		header_size = 2;
	} else if (n_address_bytes == 2) {
		header[1] = address;
		header[2] = address >> 8;
		header_size = 3;
	} else if (n_address_bytes == 3) {
		header[1] = address;
		header[2] = address >> 8;
		header[3] = address >> 16;
		header_size = 4;
	}

	header_size += n_dummy_bytes;

	return spi_nand_transaction(header, header_size,
						 	    data_is_tx,
								data, data_size);
}


int spi_nand_get_feature(uint32_t address,
						 uint8_t *data)
{
	return spi_nand_addr_transaction(0x0f,
									 address, 1,
									 0, 0, data, 1);
}

int spi_nand_set_feature(uint32_t address,
						 uint8_t data)
{
	return spi_nand_addr_transaction(0x0f,
									 address, 1,
									 0, 1, &data, 1);
}


int spi_nand_status(uint8_t *status)
{
	return spi_nand_get_feature(0xc0, status);
}



int spi_nand_reset(void)
{
	return spi_nand_no_data_transaction(0xff);
}

int spi_nand_read_id(uint8_t id[2])
{
	return spi_nand_no_addr_transaction(0xff,
										1, 0, id, 2);
}

int spi_nand_erase_block(uint32_t block)
{
	return spi_nand_addr_transaction(0xd8,
									block, 3,
									0,
									0, NULL, 0);
}

int spi_nand_write_enable(int enable)
{
	return spi_nand_no_data_transaction(
								enable ? 0x06 : 0x04);
}

int spi_nand_page_read_to_cache(uint32_t page)
{
	return spi_nand_addr_transaction(0x13,
									page, 3,
									0,
									0, NULL, 0);
}

int spi_nand_read_from_cache(uint32_t offset, uint8_t *buffer, uint32_t buffer_size)
{
	return spi_nand_addr_transaction(0x03,
									offset, 2,
									1,
									0, buffer, buffer_size);
}

int spi_nand_program_load(uint32_t offset, const uint8_t *buffer, uint32_t buffer_size)
{
	return spi_nand_addr_transaction(0x02,
									offset, 2,
									0,
									1, (uint8_t *)buffer, buffer_size);
}


int spi_nand_program_execute(uint32_t page)
{
	return spi_nand_addr_transaction(0x10,
									page, 3,
									0,
									0, NULL, 0);
}

int spi_nand_init(void)
{
	spi_nand_reset();
	return spi_nand_reset();
}

#if 1

#include <stdio.h>
void spi_nand_test(void)
{
	uint8_t id[2] = {0xAA,0xAA};
	int ret;

	printf("\n\nspi_nand_test\n\n");
	printf("Calling spi_nand_init()...");
	ret = spi_nand_init();
	printf("returned %d\n", ret);

	printf("Calling spi_nand_read_id()...");
	ret = spi_nand_read_id(id);
	printf("returned %d, id 0x%02x 0x%02x\n", ret, id[0], id[1]);
	printf("\n\nEnd of spi_nand_test()\n\n");
}

#endif

