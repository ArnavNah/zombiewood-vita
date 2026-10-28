#ifndef PORT_PRESENTATION_H
#define PORT_PRESENTATION_H
#include <stdbool.h>
void presentation_init(void);
void presentation_begin_frame(void);
void presentation_after_swap(void);
bool presentation_should_present(bool scene_drawn, bool display_written, bool state_stable);
#endif
