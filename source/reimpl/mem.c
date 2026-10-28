/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/mem.h"
#include "utils/logger.h"

#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <psp2/kernel/clib.h>

void *sceClibMemclr(void *dst, size_t len) {
    return sceClibMemset(dst, 0, len);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offs) {
    l_warn("mmap(%p, %i, %i, %i, %i, %li)", addr, length, prot, flags, fd, offs);

    if (length == 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    void* ret = malloc(length);
    if (!ret) {
        errno = ENOMEM;
        l_error("mmap allocation failed: length=%u caller=%p", (unsigned)length,
            __builtin_return_address(0));
        return MAP_FAILED;
    }
    memset(ret, 0, length);
    return ret;
}

int munmap(void *addr, size_t length) {
    if (addr) free(addr);
    return 0;
}

static void allocation_failed(const char *operation, size_t size, void *caller) {
    struct mallinfo info = mallinfo();
    l_error("[MEMORY] %s failed size=%u caller=%p heap_used=%u heap_free=%u",
        operation, (unsigned)size, caller, (unsigned)info.uordblks, (unsigned)info.fordblks);
}

void *malloc_soloader(size_t size) {
    void *result = malloc(size);
    if (!result && size) allocation_failed("malloc", size, __builtin_return_address(0));
    return result;
}
void *calloc_soloader(size_t count, size_t size) {
    void *result = calloc(count, size);
    if (!result && count && size) allocation_failed("calloc", size, __builtin_return_address(0));
    return result;
}
void *realloc_soloader(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    if (!result && size) allocation_failed("realloc", size, __builtin_return_address(0));
    return result;
}
void *memalign_soloader(size_t alignment, size_t size) {
    void *result = memalign(alignment, size);
    if (!result && size) allocation_failed("memalign", size, __builtin_return_address(0));
    return result;
}
void *memset_soloader(void *ptr, int value, size_t size) {
    // Preserve the fault, but record the Android caller before Vita3K's JIT
    // reports only an address inside the C library's NEON memset routine.
    if (size && (uintptr_t)ptr < 0x10000)
        l_error("[MEMORY] invalid memset destination=%p size=%u caller=%p",
            ptr, (unsigned)size, __builtin_return_address(0));
    return memset(ptr, value, size);
}
