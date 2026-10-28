#ifndef PORT_PCM_STREAM_H
#define PORT_PCM_STREAM_H
#include <stddef.h>
#include <string.h>

/* Keep Android's interleaved PCM byte stream intact across arbitrary writes.
 * Two buffers keep the preceding asynchronous Vita output alive until the next
 * sceAudioOutOutput call has waited for it. Callers serialize all operations. */
enum { PCM_FRAMES = 1024, PCM_BLOCK_MAX = PCM_FRAMES * 2 * sizeof(short) };
typedef struct {
    unsigned char blocks[2][PCM_BLOCK_MAX] __attribute__((aligned(64)));
    size_t block_bytes, pending;
    unsigned head, queued;
} PcmStream;
typedef int (*PcmOutput)(void *, const void *);

static void pcm_reset(PcmStream *s, unsigned channels) {
    s->block_bytes = PCM_FRAMES * channels * sizeof(short);
    s->pending = 0;
    s->head = s->queued = 0;
}
static int pcm_drain(PcmStream *s, PcmOutput output, void *context) {
    while (s->queued) {
        int result = output(context, s->blocks[s->head]);
        if (result < 0) return result;
        s->head = (s->head + 1) % 2;
        --s->queued;
    }
    return 0;
}
static size_t pcm_write(PcmStream *s, const void *data, size_t length,
                        int playing, PcmOutput output, void *context, int *error) {
    size_t done = 0;
    *error = 0;
    while (done < length) {
        if (playing && (*error = pcm_drain(s, output, context)) < 0) break;
        if (s->queued == 2) break; /* Paused/stopped queue is full: short write. */
        size_t count = s->block_bytes - s->pending;
        if (count > length - done) count = length - done;
        unsigned tail = (s->head + s->queued) % 2;
        memcpy(s->blocks[tail] + s->pending, (const unsigned char *)data + done, count);
        s->pending += count;
        done += count;
        if (s->pending == s->block_bytes) {
            s->pending = 0;
            ++s->queued;
        }
    }
    if (playing && *error >= 0) *error = pcm_drain(s, output, context);
    return done;
}
#endif
