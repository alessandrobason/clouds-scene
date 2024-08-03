#ifndef VIRTUAL_MEMORY_HEADER
#define VIRTUAL_MEMORY_HEADER

#include "collatypes.h"

void *vmemInit(usize size, usize *out_padded_size);
bool vmemRelease(void *base_ptr);
bool vmemCommit(void *ptr, usize num_of_pages);
usize vmemGetPageSize(void);
usize vmemPadToPage(usize byte_count);

#endif // VIRTUAL_MEMORY_HEADER