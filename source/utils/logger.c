/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/logger.h"

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <string.h>
#include "config.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <psp2/kernel/sysmem.h>
#include <malloc.h>
#include <pthread.h>

void hardware_log_begin(void) {
#ifdef USE_KUBRIDGE
    sceIoRemove(DATA_PATH "hardware-startup.log");
#endif
}

void hardware_checkpoint(const char *stage) {
#ifdef USE_KUBRIDGE
    SceKernelFreeMemorySizeInfo memory = {.size = sizeof(memory)};
    int result=sceKernelGetFreeMemorySize(&memory);
    struct mallinfo heap=mallinfo();
    char line[384];
    int length=sceClibSnprintf(line,sizeof(line),"%s | memory_result=%d ram=%u cdram=%u phycont=%u heap_used=%u heap_free=%u\n",
        stage,result,memory.size_user,memory.size_cdram,memory.size_phycont,
        (unsigned)heap.uordblks,(unsigned)heap.fordblks);
    if(length>0 && length<(int)sizeof(line)) {
        SceUID fd=sceIoOpen(DATA_PATH "hardware-startup.log",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_APPEND,0666);
        if(fd>=0){sceIoWrite(fd,line,length);sceIoClose(fd);}
    }
#endif
}

#define COLOR_RED    "\x1B[38;5;196m"
#define COLOR_PINK   "\x1B[38;5;212m"
#define COLOR_ORANGE "\x1B[38;5;202m"
#define COLOR_BLUE   "\x1B[38;5;32m"
#define COLOR_GREEN  "\x1B[32m"
#define COLOR_CYAN   "\x1B[36m"

#define COLOR_END    "\033[0m"

static pthread_mutex_t _log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Buffer A is used to adjust the format string.
static char buffer_a[2048];
// Buffer B is used to compile the final log using the updated format string.
static char buffer_b[2048];

void _log_print(int t, const char* fmt, ...) {
    if (pthread_mutex_lock(&_log_mutex)) return;

    switch (t) {
        case LT_DEBUG:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s• debug%s    %s\n",
                            COLOR_PINK, COLOR_END, fmt); break;
        case LT_INFO:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %sℹ info%s     %s\n",
                            COLOR_BLUE, COLOR_END, fmt); break;
        case LT_WARN:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⚠ warning%s  %s\n",
                            COLOR_ORANGE, COLOR_END, fmt); break;
        case LT_ERROR:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⨯ error%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_FATAL:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! fatal%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_SUCCESS:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! success%s  %s\n",
                            COLOR_GREEN, COLOR_END, fmt); break;
        case LT_WAIT:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s… waiting%s  %s\n",
                            COLOR_CYAN, COLOR_END, fmt); break;
        default:
            pthread_mutex_unlock(&_log_mutex);
            return;
    }

    va_list list;
    va_start(list, fmt);
    sceClibVsnprintf(buffer_b, sizeof(buffer_b), buffer_a, list);
    va_end(list);
    sceClibPrintf("%s", buffer_b);
    // Keep startup evidence even if the emulator buffers its host log.
    SceUID fd = sceIoOpen(DATA_PATH "error.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
    if (fd >= 0) {
        sceIoWrite(fd, buffer_b, strlen(buffer_b));
        sceIoClose(fd);
    }

    pthread_mutex_unlock(&_log_mutex);
}
