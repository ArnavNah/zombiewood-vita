#ifndef PORT_PERFORMANCE_H
#define PORT_PERFORMANCE_H
#ifdef PORT_PERF_TRACE
void performance_init(void);
void performance_begin(void);
void performance_input_done(void);
void performance_game_done(void);
void performance_present_done(void);
void performance_frame_done(void);
void performance_draw(unsigned microseconds, unsigned vertices);
#else
static inline void performance_init(void) {}
static inline void performance_begin(void) {}
static inline void performance_input_done(void) {}
static inline void performance_game_done(void) {}
static inline void performance_present_done(void) {}
static inline void performance_frame_done(void) {}
static inline void performance_draw(unsigned microseconds, unsigned vertices) {}
#endif
#endif
