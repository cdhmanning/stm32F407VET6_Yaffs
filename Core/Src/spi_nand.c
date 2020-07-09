#include "spi_nand.h"
#include "gpio.h"
#include "spi.h"
#include <stdlib.h>
#include <string.h>

#define ID_MANUFACTURER_MICRON 	0x2c
#define ID_MICRON_1Gbit_3V3		0x14

#define MAX_ADDRESS_BYTES 	3
#define MAX_DUMMY_BYTES		2

#define STATUS_OIP				0x01

#define nand_spi hspi3

#define PAGES_PER_BLOCK			64
#define PAGE_OOB_OFFSET			2048


/*
 * Note that all functions of the form spi_nand_cmd_xxx
 * issue NAND commands.
 * In general these should be static and not called directly.
 */
#if 1
#include <stdio.h>
#define dprintf printf
#else
#define dprintf(...) do { } while (0)
#endif



void print_buffer(const uint8_t *buffer, uint32_t buffer_size)
{
	uint32_t i;

	for(i = 0; i < buffer_size; i++)
		printf(" %02x%s", buffer[i], (i+1) & 0xf ? "" : "\n");
}



/* Base transaction handler and
 * three variants of commands to perform transaction.
 */
static int spi_nand_transaction(uint8_t *header,
								 uint32_t header_size,
								 uint32_t data_is_tx,
								 uint8_t *data,
								 uint32_t data_size)
{
	int ret = 0;

	/*
	 * Call the gpio_NAND_CS function twice to ensure it has settled
	 * before the SPI transaction starts.
	 */
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

	return ret;
}

static int spi_nand_no_addr_transaction(uint8_t op_code,
									    uint32_t n_dummy_bytes,
										uint32_t data_is_tx,
										uint8_t *data,
										uint32_t data_size)
{
	uint8_t header[MAX_DUMMY_BYTES + 1];
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
						 	    0,
								NULL, 0);
}

static int spi_nand_addr_transaction(uint8_t op_code,
								uint32_t address,
								uint32_t n_address_bytes,
								uint32_t n_dummy_bytes,
								uint32_t data_is_tx,
								uint8_t *data,
								uint32_t data_size)
{
	uint8_t header[1 /* space for opcode */
				   + MAX_ADDRESS_BYTES
				   + MAX_DUMMY_BYTES];
	uint32_t header_size;

	if (n_address_bytes > MAX_ADDRESS_BYTES ||
		n_dummy_bytes > MAX_DUMMY_BYTES)
		return -1;

	header[0]= op_code;
	header_size = 1;

	/* Addresses are MSB first */
	if (n_address_bytes == 1) {
		header[1] = (uint8_t) (address >> 0);
		header_size = 2;
	} else if (n_address_bytes == 2) {
		header[1] = (uint8_t) (address >> 8);
		header[2] = (uint8_t) (address >> 0);
		header_size = 3;
	} else if (n_address_bytes == 3) {
		header[1] = (uint8_t) (address >> 16);
		header[2] = (uint8_t) (address >> 8);
		header[3] = (uint8_t) (address >> 0);
		header_size = 4;
	}

	header_size += n_dummy_bytes;

	return spi_nand_transaction(header, header_size,
						 	    data_is_tx,
								data, data_size);
}

/*
 * Commands used.
 */

static int spi_nand_cmd_reset(void)
{
	return spi_nand_no_data_transaction(0xff);
}


static int spi_nand_cmd_get_feature(uint32_t address,
									uint8_t *data)
{
	return spi_nand_addr_transaction(0x0f,
									 address, 1,
									 0, 0,
									 data, 1);
}

static int spi_nand_cmd_set_feature(uint32_t address,
						 	 	 	uint8_t data)
{
	return spi_nand_addr_transaction(0x1f,
									 address, 1,
									 0, 1,
									 &data, 1);
}

int spi_nand_cmd_read_id(uint8_t id[2])
{
	return spi_nand_no_addr_transaction(0x9f,
										1, 0,
										id, 2);
}

int spi_nand_cmd_read_array_to_cache(uint32_t page)
{
	return spi_nand_addr_transaction(0x13,
									page, 3,
									0,
									0, NULL, 0);
}

static int spi_nand_cmd_read_from_cache(uint32_t offset, uint8_t *buffer, uint32_t buffer_size)
{
	return spi_nand_addr_transaction(0x03,
									offset, 2,
									1,
									0, buffer, buffer_size);
}

int spi_nand_cmd_write_enable(int enable)
{
	return spi_nand_no_data_transaction(enable ? 0x06 : 0x04);
}

int spi_nand_cmd_erase_block(uint32_t block)
{
	return spi_nand_addr_transaction(0xd8,
									block, 3,
									0,
									0, NULL, 0);
}

static int spi_nand_cmd_program_execute(uint32_t page)
{
	return spi_nand_addr_transaction(0x10,
									page, 3,
									0,
									0, NULL, 0);
}

static int spi_nand_cmd_program_load(uint32_t offset, const uint8_t *buffer, uint32_t buffer_size)
{
	return spi_nand_addr_transaction(0x02,
									offset, 2,
									0,
									1, (uint8_t *)buffer, buffer_size);
}

/*
 * Getting and setting specific features
 * Feature 0xA0: lock bits and nHOLD/nWP enable.
 * Feature 0xB0: Configuration.
 * Feature 0xc0: Status.
 */
static int spi_nand_get_block_lock(uint8_t *lock)
{
	return spi_nand_cmd_get_feature(0xA0, lock);
}

static int spi_nand_set_block_lock(uint8_t lock)
{
	return spi_nand_cmd_set_feature(0xA0, lock);
}

static int spi_unlock_all_blocks(void)
{
	/* Unlock all and disable nWP/nHOLD */
	return spi_nand_set_block_lock(0x2);
}

static int spi_lock_all_blocks(void)
{
	/* Lock all and disable nWP/nHOLD */
	return spi_nand_set_block_lock(0x7C | 0x2);
}

/*
 * Configuration functions.
 */
static int spi_nand_get_configuration(uint8_t *cfg)
{
	return spi_nand_cmd_get_feature(0xB0, cfg);
}

static int spi_nand_set_configuration(uint8_t cfg)
{
	return spi_nand_cmd_set_feature(0xB0, cfg);
}

/* Status is read only. */
static int spi_nand_get_status(uint8_t *status)
{
	return spi_nand_cmd_get_feature(0xc0, status);
}

/*
 * Wait until status is no longer busy and pass back status.
 */
static int spi_nand_wait_not_busy(const char *label, uint8_t *statusptr)
{
	int n = 0;
	uint8_t status;
	int ret;

	while(1) {
		ret = spi_nand_get_status(&status);
		if ((status & STATUS_OIP) == 0)
			break;
		n++;
	}
	if (label)
		dprintf("%s took %d status reads while busy\n", label, n);
	if (statusptr)
		*statusptr = status;

	return ret;
}



/*
 * Higher level commands
 */
int spi_nand_reset(void)
{
	int ret = spi_nand_cmd_reset();
	spi_nand_wait_not_busy("reset", NULL);
	return ret;
}

int spi_nand_ecc_enable(unsigned enable)
{
	int ret;
	uint8_t config0;
	uint8_t config1;


	/* If need be, reconfigure the configuration byte to enable ECC. */
	ret = spi_nand_get_configuration(&config0);

	if (enable)
		config1 = config0 | 0x10;
	else
		config1 = config0 & ~0x10;

	if (config0 != config1) {
		ret = spi_nand_set_configuration(config1);
		ret = spi_nand_get_configuration(&config1);
	}

	dprintf("%s ECC, configuration changed from %02x to %02x\n",
			enable ? "Enable" : "Disable",
			 config0, config1);

	return ret;
}

static int spi_nand_check_id(void)
{
	int ret;
	uint8_t id[2];

	/* Check it is the expected part: */
	ret = spi_nand_cmd_read_id(id);
	if (id[0] != ID_MANUFACTURER_MICRON ||
		id[1] != ID_MICRON_1Gbit_3V3) {

		dprintf("Got wrong id: expected %02X,%02X got %02X,%02X\n",
				ID_MANUFACTURER_MICRON, ID_MICRON_1Gbit_3V3,
				id[0], id[1]);
		return -1;
	}
	return ret;
}

int spi_nand_init(void)
{
	int ret;
	uint8_t status;

	/* Read status a couple of times to clear the bus. */
	spi_nand_get_status(&status);
	spi_nand_get_status(&status);

	/* Reset to get the right starting state. */
	ret = spi_nand_reset();

	ret = spi_nand_check_id();

	ret = spi_nand_cmd_write_enable(0);
	ret = spi_lock_all_blocks();
	ret = spi_nand_ecc_enable(1);

	return ret;
}

int spi_nand_read_page(uint32_t page, uint32_t offset,
					   uint8_t *buffer, uint32_t buffer_size,
					   uint8_t *statusptr)
{
	int ret;
	uint8_t status;

	ret = spi_nand_cmd_read_array_to_cache(page);
	ret = spi_nand_wait_not_busy(NULL /* "reading" */, &status);

	ret = spi_nand_cmd_read_from_cache(offset, buffer, buffer_size);

	if (statusptr)
		*statusptr = status;

	return ret;
}

int spi_nand_write_page(uint32_t page, uint32_t offset,
					   uint8_t *buffer, uint32_t buffer_size,
					   uint8_t *statusptr)
{
	int ret;
	uint8_t status;

	ret = spi_nand_cmd_write_enable(1);
	ret = spi_unlock_all_blocks();
	ret = spi_nand_cmd_program_load(offset, buffer, buffer_size);
	ret = spi_nand_cmd_program_execute(page);

	ret = spi_nand_wait_not_busy("writing", &status);

	if (statusptr)
		*statusptr = status;

	return ret;
}

int spi_nand_erase_block(uint32_t block, uint8_t *statusptr)
{
	int ret;
	uint8_t status;

	ret = spi_nand_cmd_write_enable(1);
	ret = spi_unlock_all_blocks();
	ret = spi_nand_cmd_erase_block(block);

	ret = spi_nand_wait_not_busy("erasing", &status);

	if (statusptr)
		*statusptr = status;

	return ret;
}


int spi_nand_check_block_ok(uint32_t block, uint32_t *is_ok)
{
	int ret;
	uint8_t buffer[2];
	uint8_t status;
	uint32_t ok;

	ret = spi_nand_read_page(block * PAGES_PER_BLOCK, PAGE_OOB_OFFSET,
							 buffer, sizeof(buffer), &status);
	ok = (buffer[0] == 0xff && buffer[1] == 0xff);

	if (!ok)
		dprintf("block %lu, read bytes returned %d: status %02x, bytes %02x %02x\n",
					block, ret, status, buffer[0], buffer[1] );
	if (is_ok)
		*is_ok = ok;
	return ret;
}

int spi_nand_mark_block_bad(uint32_t block, uint8_t *status)
{
	uint8_t buffer[16] = {0};
	int ret;
	uint8_t config;

	ret = spi_nand_get_configuration(&config);

	if (config & 0x10) {
		/* ECC is enabled, disable. */
		ret = spi_nand_set_configuration(config & ~0x10);
	}

	/* Write 16 bytes of 0x00 to the spare area. */
	ret = spi_nand_write_page(block * PAGES_PER_BLOCK, PAGE_OOB_OFFSET,
						     buffer, sizeof(buffer), status);

	if (config & 0x10) {
		/* ECC was enabled, re-enable. */
		ret = spi_nand_set_configuration(config);
	}

	return ret;
}

void check_bad_blocks_test(void)
{
	int i;
	uint32_t block_ok;
	uint8_t status;
	int n_bad = 0;
	int ret;

	for(i = 0; i < 1024; i++) {
		spi_nand_check_block_ok(i, &block_ok);
		if (!block_ok) {
			printf("Block %d ok? %lu\n", i, block_ok);
			n_bad++;
		}
	}

	printf("Total bad blocks %d\n", n_bad);

#if 0
	spi_nand_mark_block_bad(7, &status);

	for(i = 0; i < 10; i++) {
		spi_nand_check_block_ok(i, &block_ok);
		printf("Block %d ok? %lu\n", i, block_ok);
	}
#endif
	ret = spi_nand_erase_block(7, &status);
	printf("spi_nand_erase_block returned %d, status %02x\n", ret, status);

}


void page_erase_test(void)
{
	uint8_t buffer[16];
	int ret;
	uint8_t status;
	uint32_t page = 5;
	uint32_t offset = 0;

	ret = spi_nand_read_page(page, offset,
						     buffer, sizeof(buffer),
						   	 &status);
	printf("read page returned %d, status %02x\n",
			ret, status);
	print_buffer(buffer, sizeof(buffer));

	ret = spi_nand_erase_block(0, &status);
	printf("block erase returned %d, status %02x\n",
			ret, status);

	ret = spi_nand_read_page(page, offset,
						     buffer, sizeof(buffer),
						   	 &status);
	printf("read page returned %d, status %02x\n",
			ret, status);
	print_buffer(buffer, sizeof(buffer));
}

void page_read_write_test(void)
{
	uint8_t buffer[16];
	int ret;
	uint8_t status;
	uint32_t page = 5;
	uint32_t offset = 0;

	ret = spi_nand_read_page(page, offset,
						     buffer, sizeof(buffer),
						   	 &status);
	printf("read page returned %d, status %02x\n",
			ret, status);
	print_buffer(buffer, sizeof(buffer));

	memcpy(buffer, "hello", 6);

	ret = spi_nand_write_page(page, 3,
						     buffer, sizeof(buffer),
						   	 &status);
	printf("write page returned %d, status %02x\n",
			ret, status);

	ret = spi_nand_read_page(page, offset,
						     buffer, sizeof(buffer),
						   	 &status);
	printf("read page returned %d, status %02x\n",
			ret, status);
	print_buffer(buffer, sizeof(buffer));

}

#if 1

#include <stdio.h>
void malloc_test(uint32_t msize)
{
	char *ptr;

	ptr = malloc(msize);
	printf("malloc of %lu returned %p\n", msize, ptr);
	free(ptr);
}

void spi_nand_test(void)
{
	uint8_t status;
	uint8_t config;
	uint8_t block_lock;
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

	spi_nand_cmd_write_enable(1);

	printf("Write enabled,  spi_nand_get_status()...");
	ret = spi_nand_get_status(&status);
	printf("returned %d, status %02x\n", ret, status);

	spi_nand_cmd_write_enable(0);

	printf("Write disabled,  spi_nand_get_status()...");
	ret = spi_nand_get_status(&status);
	printf("returned %d, status %02x\n", ret, status);

	printf("spi_nand_get_block_lock()...");
	ret = spi_nand_get_block_lock(&block_lock);
	printf("returned %d, block_lock %02x\n", ret, block_lock);

	check_bad_blocks_test();

	page_read_write_test();
	page_erase_test();

	printf("\n\nEnd of spi_nand_test()\n\n");

#if 1
	malloc_test(140*1024);
	malloc_test(100*1024);
	malloc_test(20*1024);
#endif

}

#endif

