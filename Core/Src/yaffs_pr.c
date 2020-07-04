#include "yaffs_guts.h"
#include "yaffsfs.h"


#include <stdio.h>


void yaffs_sizes(void)
{
	printf("sizeof(struct yaffs_dev)=%d\n", sizeof(struct yaffs_dev));
	printf("sizeof(struct yaffs_obj)=%d\n", sizeof(struct yaffs_obj));
	printf("sizeof(struct yaffs_block_info)=%d\n", sizeof(struct yaffs_block_info));

}

void yaffsfs_SetError(int err)
{

}
void yaffs_bug_fn(const char *file_name, int line_no)
{

}

void yaffsfs_Lock(void)
{

}
void yaffsfs_Unlock(void)
{

}

int yaffsfs_CheckMemRegion(const void *addr, size_t size, int write_request)
{
	if (!addr)
		return -1;
	return 0;
}

void *yaffsfs_malloc(size_t size)
{
	return NULL;
}

void yaffsfs_free(void *ptr)
{
}

u32 yaffsfs_CurrentTime(void)
{
	return 0;
}


u32 yaffs_trace_mask= ~0;

void yaffs_call_all_funcs(void)
{
	int h;
	uint8_t b[1];
	yaffs_DIR *d;
	struct yaffs_dirent *de;

	yaffs_mount("/m");
	h = yaffs_open("/m/a", 0, 0);
	yaffs_read(h, b, 1);
	yaffs_write(h, b, 1);
	yaffs_lseek(h, 0, 0);
	yaffs_ftruncate(h,0);
	yaffs_unlink("/m/a");

	yaffs_mkdir("/m/d", 0666) ;
	yaffs_rmdir("/m/d") ;

	d = yaffs_opendir("/m/d") ;
	de = yaffs_readdir(d) ;
	(void) de;
	yaffs_rewinddir(d) ;
	yaffs_closedir(d) ;

	printf(" free space %ld\n", yaffs_freespace("/m/a"));
	printf(" total space %ld\n", yaffs_totalspace("/m/a"));
}

void yaffs_test(void)
{
	yaffs_call_all_funcs();
}
