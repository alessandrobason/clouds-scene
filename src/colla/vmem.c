#include "vmem.h"

#include <assert.h>

#include "tracelog.h"

static usize vmem__page_size = 0;
static void vmem__update_page_size(void);

// platform generic functions

usize vmemGetPageSize(void) {
    if (!vmem__page_size) {
        vmem__update_page_size();
    }
    return vmem__page_size;
}

usize vmemPadToPage(usize byte_count) {
    if (!vmem__page_size) {
        vmem__update_page_size();
    }

    if (byte_count == 0) {
        return vmem__page_size;
    }

    // bit twiddiling, vmem__page_size MUST be a power of 2
    usize padding = vmem__page_size - (byte_count & (vmem__page_size - 1));
    if (padding == vmem__page_size) {
        padding = 0;
    }
    return byte_count + padding;
}

#if COLLA_WIN

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void *vmemInit(usize size, usize *out_padded_size) {
    usize alloc_size = vmemPadToPage(size);

    void *ptr = VirtualAlloc(NULL, alloc_size, MEM_RESERVE, PAGE_NOACCESS);

    if (out_padded_size) {
        *out_padded_size = alloc_size;
    }

    return ptr;
}

bool vmemRelease(void *base_ptr) {
    return VirtualFree(base_ptr, 0, MEM_RELEASE);
}

bool vmemCommit(void *ptr, usize num_of_pages) {
    usize page_size = vmemGetPageSize();

    void *new_ptr = VirtualAlloc(ptr, num_of_pages * page_size, MEM_COMMIT, PAGE_READWRITE);
    
    if (!new_ptr) {
        debug("ERROR: failed to commit memory: %lu\n", GetLastError());
    }

    return new_ptr != NULL;
}

static void vmem__update_page_size(void) {
    SYSTEM_INFO info = {0};
    GetSystemInfo(&info);
    vmem__page_size = info.dwPageSize;
}

// #elif COLLA_LIN
#else

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

typedef struct {
    usize len;
} vmem__header;

void *vmemInit(usize size, usize *out_padded_size) {
    size += sizeof(vmem__header);
    usize alloc_size = vmemPadToPage(size);

    vmem__header *header = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (header == MAP_FAILED) {
        fatal("could not map %zu memory: %s", size, strerror(errno));
    }

    if (out_padded_size) {
        *out_padded_size = alloc_size;
    }

    header->len = alloc_size;

    return header + 1;
}

bool vmemRelease(void *base_ptr) {
    if (!base_ptr) return false;
    vmem__header *header = (vmem__header *)base_ptr - 1;

    int res = munmap(header, header->len);
    if (res == -1) {
        err("munmap failed: %s", strerror(errno));
    }
    return res != -1;
}

bool vmemCommit(void *ptr, usize num_of_pages) {
    // mmap doesn't need a commit step
    (void)ptr;
    (void)num_of_pages;
    return true;
}

static void vmem__update_page_size(void) {
    long lin_page_size = sysconf(_SC_PAGE_SIZE);

    if (lin_page_size < 0) {
        fatal("could not get page size: %s", strerror(errno));
    }

    vmem__page_size = (usize)lin_page_size;
}

#endif