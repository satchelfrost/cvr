#include <android_native_app_glue.h>
#define XR_USE_PLATFORM_ANDROID 1
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h> // TODO: is this needed?

#define DECL_PFN(pfn) PFN_ ## pfn pfn = NULL
#define LOAD_PFN(xr, pfn) xrGetInstanceProcAddr(xr, #pfn, (PFN_xrVoidFunction*)&pfn)

typedef struct {
    XrInstance instance;
    XrSystemId sys_id;
    bool track_hands;
    bool session_running;
} Xr_Context;

typedef struct {
    struct android_app *app;
    struct android_poll_source *source;
    ANativeWindow* window;
    bool resumed;
    Xr_Context xr_ctx;
} Platform_Data;

static Platform_Data platform = {0};
static const char *platform_exts[] = {"REPLACE THIS WITH AN ACTUAL EXTENSION"};
static void app_handle_cmd(struct android_app *app, int32_t cmd);
static int32_t app_handle_input(struct android_app *app, AInputEvent *event);

extern int main(int argc, char **argv);

void android_main(struct android_app *app)
{
    platform.app = app;
    // JNIEnv* Env;
    // app->activity->vm->AttachCurrentThread(&Env, NULL); // not sure if I should do this
    app->onAppCmd = app_handle_cmd;
    app->onInputEvent = app_handle_input;
    app->userData = &platform;

    char arg0[] = "cvr";
    (void)main(1, (char*[]) {arg0, NULL});

    ANativeActivity_finish(app->activity);

    int poll_result = 0;
    int poll_events = 0;
    while(!app->destroyRequested) {
        while ((poll_result = ALooper_pollAll(0, NULL, &poll_events, (void **)&platform.source)) >= 0) {
            if (platform.source) platform.source->process(app, platform.source);
        }
    }
}

bool init_platform()
{
    DECL_PFN(xrInitializeLoaderKHR);
    XrResult res = LOAD_PFN(XR_NULL_HANDLE, xrInitializeLoaderKHR);
    if (!XR_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "xrGetInstanceProcAddr failed for xrInitializeLoaderKHR");
        return false;
    }

    XrLoaderInitInfoAndroidKHR loader_info = {
        .type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
        .applicationVM = platform.app->activity->vm,
        .applicationContext = platform.app->activity->clazz,
    };
    res = xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loader_info);
    if (!XR_SUCCEEDED(res)) {
        nob_log(NOB_ERROR, "xrInitializeLoaderKHR failed");
        return false;
    }

    return true;
}

void close_platform()
{
    // for (Swapchain swapchain : swapchains_)
    //     xrDestroySwapchain(swapchain.handle);
    //
    // for (auto referenceSpace : initializedRefSpaces_)
    //     xrDestroySpace(referenceSpace.second);
    //
    // if (appSpace_ != XR_NULL_HANDLE)
    //     xrDestroySpace(appSpace_);
    //
    // if (GlobalContext::Inst()->ExtMan()->usingHandtracking_) {
    //     handTrackerLeft_.EndSession();
    //     handTrackerRight_.EndSession();
    // }
    // actionManager.EndSession();

    // if (session_ != XR_NULL_HANDLE)
    //     xrDestroySession(session_);

    if (platform.xr_ctx.instance != XR_NULL_HANDLE)
        xrDestroyInstance(platform.xr_ctx.instance);
}

void poll_input_events()
{
    int poll_result = 0;
    int poll_events = 0;
    while(!platform.app->destroyRequested && platform.xr_ctx.session_running) {
        int timeout = !platform.resumed ? -1 : 0; // -1 waits forever
        while ((poll_result = ALooper_pollAll(timeout, NULL, &poll_events, (void **)&platform.source)) >= 0) {
            if (platform.source) platform.source->process(platform.app, platform.source);
        }
    }
}

void app_handle_cmd(struct android_app *app, int32_t cmd)
{
    switch (cmd) {
    case APP_CMD_START:
        break;
    case APP_CMD_RESUME:
        platform.resumed = true;
        break;
    case APP_CMD_PAUSE:
        platform.resumed = false;
        break;
    case APP_CMD_STOP:
        break;
    case APP_CMD_DESTROY:
        platform.window = NULL;
        break;
    case APP_CMD_INIT_WINDOW:
        platform.window = app->window;
        break;
    case APP_CMD_TERM_WINDOW:
        platform.window = NULL;
        break;
    default:
        break;
    }
}

int32_t app_handle_input(struct android_app *app, AInputEvent *event)
{
    (void)app;
    int32_t event_src = AInputEvent_getSource(event);
    switch (event_src) {
        case AINPUT_SOURCE_MOUSE:
            break;
        case AINPUT_SOURCE_KEYBOARD:
            break;
        default:
            break;
    }
    return 0;
}

const char **get_platform_exts(uint32_t *platform_ext_count)
{
    *platform_ext_count = NOB_ARRAY_LEN(platform_exts);
    return platform_exts;
}

double get_time()
{
    return 0;
}

bool platform_surface_init()
{
    return false;
}

void platform_frame_buff_size(int *width, int *height)
{
    // TODO: not sure where this is coming from yet, maybe OpenXR?
    *width = 0;
    *height = 0;
}

void platform_wait_resize_frame_buffer()
{
    // TODO: I'm not really sure it makes sense for a framebuffer resize to happen since we
    // TODO: are not dealing with a typical window
}
