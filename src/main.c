#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

typedef struct {
    GLFWwindow *window;
    VkInstance instance;
} App;

App app = {0};

bool init_window();
bool init_vulkan();
bool main_loop();
bool cleanup();
bool create_instance();

bool init_window()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    app.window = glfwCreateWindow(800, 600, "C Vulkan Renderer", NULL, NULL);
    return true;
}

bool main_loop()
{
    while(!glfwWindowShouldClose(app.window))
        glfwPollEvents();
    return true;
}

bool init_vulkan()
{
    if (!create_instance()) {
        nob_log(NOB_ERROR, "failed to create vulkan instance");
        return false;
    }

    return true;
}

bool cleanup()
{
    vkDestroyInstance(app.instance, NULL);
    glfwDestroyWindow(app.window);
    glfwTerminate();
    return true;
}

bool create_instance()
{
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "C + Vulkan = Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_ci = {0};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo = &app_info;
    uint32_t glfw_ext_count = 0;
    const char **glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    instance_ci.enabledExtensionCount = glfw_ext_count;
    instance_ci.ppEnabledExtensionNames = glfw_exts;

    // Checked to see if instance extensions were available
    uint32_t avail_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, NULL);
    VkExtensionProperties avail_exts[avail_ext_count];
    vkEnumerateInstanceExtensionProperties(NULL, &avail_ext_count, avail_exts);
    size_t unsatisfied_exts = glfw_ext_count;
    for (size_t i = 0; i < glfw_ext_count; i++) {
        for (size_t j = 0; j < avail_ext_count; j++) {
            if (strcmp(glfw_exts[i], avail_exts[j].extensionName) == 0)
                unsatisfied_exts--;
        }
    }
    if (unsatisfied_exts) {
        nob_log(NOB_ERROR, "unsatisfied instance extensions");
        return false;
    }

    VkResult result = vkCreateInstance(&instance_ci, NULL, &app.instance);
    return result == VK_SUCCESS;
}

int main()
{
    if (!init_window()) return 1;
    if (!init_vulkan()) return 1;
    if (!main_loop()) return 1;
    if (!cleanup()) return 1;

    return 0;
}