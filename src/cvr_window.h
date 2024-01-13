#ifndef CVR_WINDOW_H_
#define CVR_WINDOW_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct {
    GLFWwindow *handle;
} CVR_Window;

void frame_buff_resized(GLFWwindow* window, int width, int height);

#endif // CVR_WINDOW_H_
