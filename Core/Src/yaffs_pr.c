#include "yaffs_guts.h"
#include "yaffsfs.h"
#include "spi_nand.h"


#include <stdio.h>

#define PAGE_TAGS_OFFSET	0x820


void yaffs_sizes(void)
{
	printf("sizeof(struct yaffs_dev)=%d\n", sizeof(struct yaffs_dev));
	printf("sizeof(struct yaffs_obj)=%d\n", sizeof(struct yaffs_obj));
	printf("sizeof(struct yaffs_block_info)=%d\n", sizeof(struct yaffs_block_info));

}

void yaffsfs_SetError(int err)
{
	errno = err;
}
void yaffs_bug_fn(const char *file_name, int line_no)
{
	printf("Yaffs bug at %s, line %d\n", file_name, line_no);
}

void yaffsfs_Lock(void)
{

}
void yaffsfs_Unlock(void)
{

}

int yaffsfs_CheckMemRegion(const void *addr, size_t size, int write_request)
{
	(void) write_request;

	if (!addr)
		return -1;
	return 0;
}


void *yaffsfs_malloc(size_t size)
{
	return malloc(size);
}

void yaffsfs_free(void *ptr)
{
		free(ptr);
}

u32 yaffsfs_CurrentTime(void)
{
	return 0;
}


u32 yaffs_trace_mask= 0;

static struct yaffs_dev this_dev;


static int yaffs_spi_nand_write_chunk (struct yaffs_dev *dev, int nand_chunk,
			   const u8 *data, int data_len,
			   const u8 *oob, int oob_len)
{
	int ret;
	struct spi_nand_buffer_op op[2];
	int n_ops = 0;

	if (data && data_len) {
		op[n_ops].offset = 0;
		op[n_ops].buffer = (uint8_t *)data;
		op[n_ops].nbytes = data_len;
		n_ops++;
	}
	if (oob && oob_len) {
		op[n_ops].offset = PAGE_TAGS_OFFSET;
		op[n_ops].buffer = (uint8_t *)oob;
		op[n_ops].nbytes = oob_len;
		n_ops++;
	}

	ret =  spi_nand_write_page(nand_chunk, op, n_ops, NULL);

	if (ret < 0)
		return YAFFS_FAIL;

	return YAFFS_OK;
}



static int yaffs_spi_nand_read_chunk (struct yaffs_dev *dev, int nand_chunk,
			   u8 *data, int data_len,
			   u8 *oob, int oob_len,
			   enum yaffs_ecc_result *ecc_result)
{
	int ret;
	struct spi_nand_buffer_op op[2];
	int n_ops = 0;
	uint8_t status;

	if (data && data_len) {
		op[n_ops].offset = 0;
		op[n_ops].buffer = (uint8_t *)data;
		op[n_ops].nbytes = data_len;
		n_ops++;
	}
	if (oob && oob_len) {
		op[n_ops].offset = PAGE_TAGS_OFFSET;
		op[n_ops].buffer = (uint8_t *)oob;
		op[n_ops].nbytes = oob_len;
		n_ops++;
	}

	ret =  spi_nand_read_page(nand_chunk, op, n_ops, &status);

	/* */
	if (ecc_result) {
		uint8_t ecc_status = (status >>4) & 7; /* Just the ECC status bits. */

		if (ecc_status == 0 || ecc_status == 1)
			*ecc_result = YAFFS_ECC_RESULT_NO_ERROR;
		else if (ecc_status == 2)
			*ecc_result = YAFFS_ECC_RESULT_UNFIXED;
		else
			*ecc_result = YAFFS_ECC_RESULT_FIXED;
	}
	if (ret < 0)
		return YAFFS_FAIL;

	return YAFFS_OK;
}

static int yaffs_spi_nand_erase_block (struct yaffs_dev *dev, int block_no)
{
	int ret;
	uint8_t status;

	ret = spi_nand_erase_block(block_no, &status);

	if (ret < 0 || (status & 0x04))
		return YAFFS_FAIL;

	return YAFFS_OK;
}


static int yaffs_spi_nand_mark_bad_block(struct yaffs_dev *dev, int block_no)
{
	int ret = 0;
	uint8_t status;

	(void) status;
	//ret = spi_nand_mark_block_bad(block_no, &status);

	if (ret < 0)
		return YAFFS_FAIL;

	return YAFFS_OK;
}

static int yaffs_spi_nand_check_bad_block (struct yaffs_dev *dev, int block_no)
{
	int ret;
	uint32_t is_ok;

	ret = spi_nand_check_block_ok(block_no, &is_ok);

	if (ret == 0 && is_ok)
		return YAFFS_OK;
	return YAFFS_FAIL;
}

static int yaffs_spi_nand_initialise (struct yaffs_dev *dev)
{
	int ret;

	ret = spi_nand_init();
	if (ret < 0)
		return YAFFS_FAIL;
	return YAFFS_OK;
}

int yaffs_spi_nand_deinitialise (struct yaffs_dev *dev)\
{
	return YAFFS_OK;
}

int yaffs_spi_nand_load_driver(const char *name,
							   uint32_t start_block,
							   uint32_t end_block)
{
	struct yaffs_dev *dev = &this_dev;
	char *name_copy = strdup(name);
	struct yaffs_param *param;
	struct yaffs_driver *drv;


	if(!name_copy) {
		free(name_copy);
		return YAFFS_FAIL;
	}

	param = &dev->param;
	drv = &dev->drv;

	memset(dev, 0, sizeof(*dev));

	param->name = name_copy;

	param->total_bytes_per_chunk = 2048;
	param->chunks_per_block = 64;
	param->spare_bytes_per_chunk = 32;
	param->no_tags_ecc = 1;
	param->n_reserved_blocks = 5;
	param->start_block = start_block;
	param->end_block = end_block;
	param->use_nand_ecc = 1;
	param->is_yaffs2 = 1;
	param->inband_tags = 0;

	param->n_caches = 5;
	param->disable_soft_del = 1;

	drv->drv_write_chunk_fn = yaffs_spi_nand_write_chunk;
	drv->drv_read_chunk_fn = yaffs_spi_nand_read_chunk;
	drv->drv_erase_fn = yaffs_spi_nand_erase_block;
	drv->drv_mark_bad_fn = yaffs_spi_nand_mark_bad_block;
	drv->drv_check_bad_fn = yaffs_spi_nand_check_bad_block;
	drv->drv_initialise_fn = yaffs_spi_nand_initialise;
	drv->drv_deinitialise_fn = yaffs_spi_nand_deinitialise;



	dev->driver_context = (void *) 1;	// Used to identify the device in fstat.

	yaffs_add_device(dev);

	return YAFFS_OK;
}



static uint8_t local_buffer[1000];

void fill_local_buffer(void)
{
	int i;
	uint32_t *local_buffer_u32 = (uint32_t *)local_buffer;

	for (i = 0; i < 250; i++)
		local_buffer_u32[i] = i * 10;
}

void create_a_file(const char *name, int size)
{
	int i;
	int h;

	h = yaffs_open(name, O_CREAT | O_RDWR | O_TRUNC, 0666);

	while (size > 0) {
			i = size;
			if (i > sizeof(local_buffer))
				i = sizeof(local_buffer);
			yaffs_write(h, local_buffer, i);
			size -= i;
	}
	yaffs_close(h);
}

void read_a_file(const char *name)
{
	int h;
	int size;
	int ret;
	int start = HAL_GetTick();
	int n_reads = 0;

	h = yaffs_open(name, O_RDONLY, 0);
	size = yaffs_lseek(h, 0, SEEK_END);
	yaffs_lseek(h, 0, SEEK_SET);

	while(yaffs_read(h, local_buffer, sizeof(local_buffer)) > 0)
		n_reads++;

	printf("Reading file %s, handle %d, size %d took %d reads and %d milliseconds\n",
			name, h, size, n_reads, HAL_GetTick() - start);
	yaffs_close(h);
}

void yaffs_call_all_funcs(void)
{
	int h;
	uint8_t b[200];
	yaffs_DIR *d;
	struct yaffs_dirent *de;
	int ret;
	int l;
	int start;

	(void) ret;
	ret = yaffs_spi_nand_load_driver("/m", 0, 200);

	start = HAL_GetTick();
	ret = yaffs_mount("/m");
	printf("Mounting /m returned %d, took %d msec\n", ret, HAL_GetTick() - start);

	fill_local_buffer();
	printf("Start writing 10 Mbytes\n");
	start = HAL_GetTick();
	create_a_file("/m/0", 1000000);
	create_a_file("/m/1", 1000000);
	create_a_file("/m/2", 1000000);
	create_a_file("/m/3", 1000000);
	create_a_file("/m/4", 1000000);
	create_a_file("/m/5", 1000000);
	create_a_file("/m/6", 1000000);
	create_a_file("/m/7", 1000000);
	create_a_file("/m/8", 1000000);
	create_a_file("/m/9", 1000000);
	printf("End writing 10 Mbytes, took %d milliseconds\n",
			HAL_GetTick() - start);

	printf("Start reading 10 Mbytes\n");
	start = HAL_GetTick();
	read_a_file("/m/0");
	read_a_file("/m/1");
	read_a_file("/m/2");
	read_a_file("/m/3");
	read_a_file("/m/4");
	read_a_file("/m/5");
	read_a_file("/m/6");
	read_a_file("/m/7");
	read_a_file("/m/8");
	read_a_file("/m/9");
	printf("End reading 10 Mbytes, took %d milliseconds\n",
			HAL_GetTick() - start);

	h = yaffs_open("/m/a", O_RDWR, 0);

	if(h >= 0) {
		l = yaffs_lseek(h, 0, SEEK_END);
		printf("File /m/a exists, size %d\n", l);
		yaffs_close(h);
	}
	h = yaffs_open("/m/a", O_CREAT | O_TRUNC | O_RDWR, 0666);
	printf("opening file /m/a returned %d\n", h);
	ret = yaffs_write(h, b, sizeof(b)-20);
	printf("writing %d bytes returned %d\n", sizeof(b)-20, ret);
	ret = yaffs_lseek(h, 0, SEEK_END);
	printf("lseek to end returned %d\n", ret);
	ret = yaffs_lseek(h, 0, 0);
	printf("lseek to 0 returned %d\n", ret);

	ret = yaffs_read(h, b, sizeof(b));
	printf("reading %d bytes returned %d\n", sizeof(b), ret);

	ret = yaffs_ftruncate(h,0);
	ret = yaffs_unlink("/m/a");

	ret = yaffs_mkdir("/m/d", 0666) ;

	d = yaffs_opendir("/m/d") ;
	de = yaffs_readdir(d) ;
	(void) de;
	yaffs_rewinddir(d) ;
	yaffs_closedir(d) ;
	ret = yaffs_rmdir("/m/d") ;

	printf(" free space %ld\n", yaffs_freespace("/m/a"));
	printf(" total space %ld\n", yaffs_totalspace("/m/a"));
}

void yaffs_test(void)
{
	printf("<<<<<<<<<<<<<<<<<<<<< Start Yaffs test\n");
	yaffs_call_all_funcs();
	printf(">>>>>>>>>>>>>>>>>>>>> End Yaffs test\n");
}
