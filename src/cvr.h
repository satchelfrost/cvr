#ifndef CVR_H_
#define CVR_H_

#include <stdbool.h>

/* Initialize window and vulkan context */
bool init_window(int width, int height, const char *title);

/* Close window and vulkan context */
void close_window();

bool window_should_close();
bool draw();

#endif // CVR_H_
