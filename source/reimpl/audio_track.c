#include "audio_track.h"
#include "pcm_stream.h"
#include "utils/logger.h"
#include <psp2/audioout.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

enum { AUDIO_FRAMES = PCM_FRAMES };
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct { int port, channels, play_state; unsigned writes; }
    audio = { .port = -1, .channels = 2, .play_state = 1 };
static PcmStream stream;

static int channel_count(int mask) {
    if (mask == 2 || mask == 4) return 1; // Legacy/current Android mono masks.
    if (mask == 3 || mask == 12) return 2;
    return 0;
}

static int audio_output(void *context, const void *buffer) {
    int result = sceAudioOutOutput(audio.port, buffer);
    if (result >= 0 && !audio.writes++)
        l_info("[AUDIO] Continuous PCM output started (%u bytes per block)", (unsigned)stream.block_bytes);
    return result;
}

jint audio_track_sample_rate(jmethodID id, va_list args) { return 48000; }

jint audio_track_min_buffer(jmethodID id, va_list args) {
    (void)va_arg(args, jint);
    int channels = channel_count(va_arg(args, jint));
    int encoding = va_arg(args, jint);
    return encoding == 2 && channels ? AUDIO_FRAMES * channels * 2 : -2;
}

jobject audio_track_create(jmethodID id, va_list args) {
    (void)va_arg(args, jint); // Android stream type.
    int rate = va_arg(args, jint);
    int channels = channel_count(va_arg(args, jint));
    int encoding = va_arg(args, jint);
    (void)va_arg(args, jint); // Requested queue capacity.
    int mode = va_arg(args, jint);
    if (encoding != 2 || mode != 1 || !channels) return NULL;
    pthread_mutex_lock(&audio_mutex);
    if (audio.port >= 0) {
        sceAudioOutOutput(audio.port, NULL);
        sceAudioOutReleasePort(audio.port);
    }
    audio.channels = channels;
    audio.play_state = 1;
    audio.writes = 0;
    pcm_reset(&stream, channels);
    audio.port = sceAudioOutOpenPort(rate == 48000 ? SCE_AUDIO_OUT_PORT_TYPE_MAIN : SCE_AUDIO_OUT_PORT_TYPE_BGM,
        AUDIO_FRAMES, rate, channels == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
    l_info("[AUDIO] AudioTrack port=%d rate=%d channels=%d", audio.port, rate, channels);
    jobject result = audio.port >= 0 ? malloc(1) : NULL;
    if (!result && audio.port >= 0) {
        sceAudioOutReleasePort(audio.port);
        audio.port = -1;
    }
    pthread_mutex_unlock(&audio_mutex);
    return result;
}

void audio_track_play(jmethodID id, va_list args) {
    pthread_mutex_lock(&audio_mutex);
    if (audio.port >= 0) {
        audio.play_state = 3;
        pcm_drain(&stream, audio_output, NULL);
    }
    pthread_mutex_unlock(&audio_mutex);
}

void audio_track_pause(jmethodID id, va_list args) {
    pthread_mutex_lock(&audio_mutex);
    audio.play_state = 2;
    pthread_mutex_unlock(&audio_mutex);
}

void audio_track_stop(jmethodID id, va_list args) {
    pthread_mutex_lock(&audio_mutex);
    audio.play_state = 1;
    if (audio.port >= 0) sceAudioOutOutput(audio.port, NULL);
    pcm_reset(&stream, audio.channels);
    pthread_mutex_unlock(&audio_mutex);
}

void audio_track_release(jmethodID id, va_list args) {
    pthread_mutex_lock(&audio_mutex);
    if (audio.port >= 0) {
        sceAudioOutOutput(audio.port, NULL);
        sceAudioOutReleasePort(audio.port);
    }
    audio.port = -1;
    audio.play_state = 1;
    pcm_reset(&stream, audio.channels);
    pthread_mutex_unlock(&audio_mutex);
}

jint audio_track_play_state(jmethodID id, va_list args) {
    pthread_mutex_lock(&audio_mutex);
    int state = audio.play_state;
    pthread_mutex_unlock(&audio_mutex);
    return state;
}

jint audio_track_write(jmethodID id, va_list args) {
    jbyteArray bytes = va_arg(args, jbyteArray);
    jint offset = va_arg(args, jint), length = va_arg(args, jint);
    if (!bytes || offset < 0 || length < 0) return -2;
    jsize available = jni->GetArrayLength(&jni, bytes);
    if (offset > available || length > available - offset) return -2;
    pthread_mutex_lock(&audio_mutex);
    if (audio.port < 0) { pthread_mutex_unlock(&audio_mutex); return -3; }
    jbyte *data = jni->GetByteArrayElements(&jni, bytes, NULL);
    if (!data) { pthread_mutex_unlock(&audio_mutex); return -2; }
    int error;
    int done = (int)pcm_write(&stream, data + offset, length,
                             audio.play_state == 3, audio_output, NULL, &error);
    jni->ReleaseByteArrayElements(&jni, bytes, data, JNI_ABORT);
    pthread_mutex_unlock(&audio_mutex);
    return done ? done : (error < 0 ? -3 : 0);
}

#ifdef PORT_TEST_INPUT
#include "config.h"
#include <stdio.h>
typedef struct { const unsigned char *expected; size_t offset, block; int failed, reject; } TestSink;
static int test_output(void *context, const void *data) {
    TestSink *sink = context;
    if (sink->reject) return -1;
    if (memcmp(data, sink->expected + sink->offset, sink->block)) sink->failed = 1;
    sink->offset += sink->block;
    return 0;
}
void audio_stream_test(void) {
    static PcmStream test;
    static unsigned char input[PCM_BLOCK_MAX * 5];
    // Independent nonzero left/right sample patterns expose swaps and silence.
    for (unsigned i=0; i<sizeof(input)/4; ++i) {
        short lr[2] = {(short)(i*17+1), (short)(-((int)i*23+9))};
        memcpy(input+i*4, lr, sizeof(lr));
    }
    unsigned passed=0;
    for (unsigned channels=1; channels<=2; ++channels) {
        pcm_reset(&test, channels);
        TestSink sink={input, 0, test.block_bytes, 0, 0};
        size_t done=0;
        int error=0;
        const unsigned chunks[]={1,3,101,4095,7,2049,13};
        unsigned n=0;
        while(done<sizeof(input)) {
            size_t count=chunks[n++%7];
            if(count>sizeof(input)-done) count=sizeof(input)-done;
            size_t wrote=pcm_write(&test,input+done,count,1,test_output,&sink,&error);
            if(wrote!=count || error<0) { sink.failed=1; break; }
            done+=wrote;
        }
        if(!sink.failed && sink.offset==sizeof(input) && !test.pending && !test.queued) ++passed;
    }
    pcm_reset(&test,2);
    TestSink sink={input,0,test.block_bytes,0,0};
    int error=0;
    size_t first=pcm_write(&test,input,sizeof(input),0,test_output,&sink,&error);
    if(first==test.block_bytes*2 && !sink.offset && test.queued==2) ++passed;
    if(!pcm_drain(&test,test_output,&sink) && !sink.failed && sink.offset==first) ++passed;
    size_t rest=pcm_write(&test,input+first,sizeof(input)-first,1,test_output,&sink,&error);
    if(rest==sizeof(input)-first && !sink.failed && sink.offset==sizeof(input)) ++passed;
    pcm_reset(&test,2);
    sink=(TestSink){input,0,test.block_bytes,0,1};
    size_t accepted=pcm_write(&test,input,test.block_bytes,1,test_output,&sink,&error);
    if(accepted==test.block_bytes && error<0 && test.queued==1 && !sink.offset) ++passed;
    sink.reject=0;
    if(!pcm_drain(&test,test_output,&sink) && !sink.failed && sink.offset==test.block_bytes) ++passed;
    pcm_reset(&test,2);
    if(!test.queued && !test.pending) ++passed;
    FILE *report=fopen(DATA_PATH "audio-stream-test.txt","w");
    if(report) { fprintf(report,"PCM stream tests: %u/8 passed\n",passed); fclose(report); }
}
#endif
