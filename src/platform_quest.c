#include <android_native_app_glue.h>

typedef struct {
    struct android_app *app;
    struct android_poll_source *source;
    ANativeWindow* window;
    bool resumed;
} Platform_Data;

static Platform_Data platform = {0};

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

