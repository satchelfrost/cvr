#ifndef CVR_H_
#define CVR_H_

#include <stdbool.h>
#include "ext/raylib-5.0/raylib.h"

/* Initialize window and vulkan context */
bool init_window(int width, int height, const char *title);

/* Close window and vulkan context */
void close_window();

bool window_should_close();
bool draw();
void clear_background(Color color);
void begin_mode_3d(Camera3D camera);
// void EndMode3D(void);

#endif // CVR_H_
