#include <stddef.h>

void *sf_open(const char *path, int mode, void *sfinfo) {
    return NULL;
}

int sf_close(void *sndfile) {
    return 0;
}

long long sf_seek(void *sndfile, long long frames, int whence) {
    return 0;
}

long long sf_read_short(void *sndfile, short *ptr, long long items) {
    return 0;
}
