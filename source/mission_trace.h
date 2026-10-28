#ifndef MISSION_TRACE_H
#define MISSION_TRACE_H
#ifdef PORT_MISSION_TRACE
void mission_trace_init(void);
void mission_trace_frame(unsigned frame,unsigned draws,unsigned vertices,unsigned fbo,int written);
int mission_trace_force_present(void);
void mission_trace_after_present(void);
#else
static inline void mission_trace_init(void) {}
static inline void mission_trace_frame(unsigned frame,unsigned draws,unsigned vertices,unsigned fbo,int written) {}
static inline int mission_trace_force_present(void) { return 0; }
static inline void mission_trace_after_present(void) {}
#endif
#endif
