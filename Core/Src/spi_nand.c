#include "spi_nand.h"
#include "gpio.h"
#include "spi.h"

#define ID_MANUFACTURER_MICRON 	0x2c
#define ID_MICRON_1Gbit_3V3		0x14

#define STATUS_OIP				0x01

#define nand_spi hspi3

#if 1
#include <stdio.h>
#define dprintf printf
#else
#define dprintf(...) do { } while (0)
#endif

static int spi_nand_transaction(uint8_t *header,
								 uint32_t header_size,
								 uint32_t data_is_tx,
								 uint8_t *data,
								 uint32_t data_size)
{
	int ret = 0;

	gpio_NAND_CS(0);
	gpio_NAND_CS(0);
	/* Send transaction header (opcode, address, dummy) */
	if (header && header_size)
		ret = HAL_SPI_Transmit(&nand_spi, header, header_size, 10);

	/* Send or receive data */
	if (data_is_tx && data && data_size)
		ret = HAL_SPI_Transmit(&nand_spi, data, data_size, 10);
	else if (data && data_size)
		ret = HAL_SPI_Receive(&nand_spi, data, data_size, 10);
	gpio_NAND_CS(1);
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
	return spi_nand_addr_transaction(0x1f,
									 address, 1,
									 0, 1, &data, 1);
}

int spi_nand_get_block_lock(uint8_t *lock)
{
	return spi_nand_get_feature(0xA0, lock);
}

int spi_nand_set_block_lock(uint8_t lock)
{
	return spi_nand_set_feature(0xA0, lock);
}

int spi_nand_get_configuration(uint8_t *cfg)
{
	return spi_nand_get_feature(0xB0, cfg);
}

int spi_nand_set_configuration(uint8_t cfg)
{
	return spi_nand_set_feature(0xB0, cfg);
}

int spi_nand_get_status(uint8_t *status)
{
	return spi_nand_get_feature(0xc0, status);
}

int spi_nand_wait_not_busy(const char *label)
{
	int n = 0;
	uint8_t status;
	while(1) {
		spi_nand_get_status(&status);
		if ((status & STATUS_OIP) == 0)
			break;
		n++;
	}
	if (label)
			dprintf("%s took %d status reads while busy\n", label, n);
	return n;
}

int spi_nand_cmd_reset(void)
{
	return spi_nand_no_data_transaction(0xff);
}

int spi_nand_reset(void)
{
	int ret = spi_nand_cmd_reset();
	spi_nand_wait_not_busy("reset");
	return ret;
}

int spi_nand_read_id(uint8_t id[2])
{
	return spi_nand_no_addr_transaction(0x9f,
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
	int ret;
	uint8_t block_lock0;
	uint8_t block_lock1;
	uint8_t id[2];
	uint8_t status;

	/* Read status a couple of times to clear the bus. */
	spi_nand_get_status(&status);
	spi_nand_get_status(&status);

	/* Reset to get the right starting state.
	 */
	ret = spi_nand_reset();

	/* Check it is the expected part: */
	ret = spi_nand_read_id(id);
	if (id[0] != ID_MANUFACTURER_MICRON ||
		id[1] != ID_MICRON_1Gbit_3V3) {

		dprintf("Got wrong id: expected %02X,%02X got %02X,%02X\n",
				ID_MANUFACTURER_MICRON, ID_MICRON_1Gbit_3V3,
				id[0], id[1]);
		return -1;
	}

	/*
	 * If need be, reconfigure block_lock to disable the
	 * WP# and HOLD# pins.
	 */
	ret = spi_nand_get_block_lock(&block_lock0);
	if ((block_lock0 & 0x2) == 0) {
		block_lock1 = block_lock0 | 0x2;
		ret = spi_nand_set_block_lock(block_lock1);
		ret = spi_nand_get_block_lock(&block_lock1);
		dprintf("Disabling WPE and HOLD#, block lock changed from %02x to %02x\n",
			 block_lock0, block_lock1);
	}

	return ret;
}

#if 1

#include <stdio.h>
void spi_nand_test(void)
{
	uint8_t status;
	uint8_t config;

	int ret;

	printf("\n\nspi_nand_test\n\n");

	printf("spi_nand_init()...\n");
	ret = spi_nand_init();
	printf("spi_nand_init()... returned %d\n", ret);


	printf("Calling spi_nand_get_configuration()...");
	ret = spi_nand_get_configuration(&config);
	printf("returned %d, configuration %02x\n", ret, config);

	printf("Calling spi_nand_get_status()...");
	ret = spi_nand_get_status(&status);
	printf("returned %d, status %02x\n", ret, status);

	printf("\n\nEnd of spi_nand_test()\n\n");
}

#endif

