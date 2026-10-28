#pragma once
#include <falso_jni/FalsoJNI.h>
#include <stdarg.h>

jobject audio_track_create(jmethodID id, va_list args);
jint audio_track_min_buffer(jmethodID id, va_list args);
jint audio_track_sample_rate(jmethodID id, va_list args);
jint audio_track_write(jmethodID id, va_list args);
jint audio_track_play_state(jmethodID id, va_list args);
void audio_track_play(jmethodID id, va_list args);
void audio_track_pause(jmethodID id, va_list args);
void audio_track_stop(jmethodID id, va_list args);
void audio_track_release(jmethodID id, va_list args);
#ifdef PORT_TEST_INPUT
void audio_stream_test(void);
#endif
