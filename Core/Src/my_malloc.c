#include <stdlib.h>

/* This malloc does the following:
 * * Uses a linked list to hold blocks of memory.
 * * 8-byte header to hold a size and a pointer to next.
 * * Always allocated in blocks of 8 bytes.
 * * malloc searches the list looking for a block
 *  of the requested size or the smallest size bigger than that,
 *  then carves a piece off and leaves the rest
 */

#if 0

struct mheader {
	struct mheader *next;
	struct uint32_t size;
};

static uint64_t my_heap[128 * 1024/8];
static struct mheader *heap_list;

static void initialise(void)
{
	static int initialised;
	struct mheader *init_header;

	if(initialised)
		return;

	init_header = (struct mheader *)my_heap;
	init_header->size = sizeof(my_heap);
	init_header->next = NULL; /* End of list. */

}

static void
void *malloc(size_t size_arg)
{
	uint32_alloc_size;
	struct mheader *best;
	struct mheader *mh;


	initialise();
	alloc_size = (size_arg + sizeof(struct mheader) + 7) & (~7);

	/* Searching the list searching for the best:
	 * ie. the one that fits with the least waste.
	 */
	best = NULL;


	while(1) {

	}

	return NULL;
}

void free(void *ptr)
{

}
#endif
