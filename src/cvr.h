#ifndef CVR_H_
#define CVR_H_

#include <stdbool.h>
#include "cvr_camera.h"
#include "color.h"
#include "cvr_input.h"

/* Initialize window and vulkan context */
bool init_window(int width, int height, const char *title);

/* Close window and vulkan context */
void close_window();

bool window_should_close();
bool draw();
void clear_background(Color color);
void begin_mode_3d(CVR_Camera camera);
// void EndMode3D(void);
bool is_key_pressed(int key);
void poll_input_events();

#endif // CVR_H_
