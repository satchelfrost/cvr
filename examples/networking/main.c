#include "cvr.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "../../nob.h"

#define CUBE_SPEED 3.0f
#define ROTATION_SPEED 2.0f
#define SCALE_SPEED 2.0f
#define MIN_SCALE 0.4f
#define MAX_SCALE 2.0f
#define DEAD_ZONE 0.25f

/* networking */
const char *ip = "127.0.0.1";
int port = 3000;
int sockfd = 0;
struct sockaddr_in dest_addr = {0};
bool debug = false;

typedef struct {
    Vector3 position;
    Vector3 heading;
    float y_rotation;
    float scale;
} Cube;

typedef enum {
    CUBE_TRANSFORM_TRANSLATE_RIGHT,
    CUBE_TRANSFORM_TRANSLATE_LEFT,
    CUBE_TRANSFORM_TRANSLATE_FORWARD,
    CUBE_TRANSFORM_TRANSLATE_BACKWARD,
    CUBE_TRANSFORM_ROTATE_CW,
    CUBE_TRANSFORM_ROTATE_CCW,
    CUBE_TRANSFORM_SCALE_UP,
    CUBE_TRANSFORM_SCALE_DOWN,
    CUBE_TRANFORM_COUNT,
} Cube_Transform;

// #define CHEAT_MODE
bool should_transform(Cube_Transform tform, float *joy_scale)
{
    float l_joy_x = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_X);
    float l_joy_y = get_gamepad_axis_movement(GAMEPAD_AXIS_LEFT_Y);
    float r_joy_x = get_gamepad_axis_movement(GAMEPAD_AXIS_RIGHT_X);
    bool l_trigger = is_gamepad_button_down(GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    bool r_trigger = is_gamepad_button_down(GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    (void)l_joy_x;
    (void)l_trigger;
    (void)r_trigger;
    *joy_scale = 1.0f;

    /* keyboard only */
    switch (tform) {
    case CUBE_TRANSFORM_TRANSLATE_RIGHT:    if (is_key_down(KEY_D)    && debug) return true; break; 
    case CUBE_TRANSFORM_TRANSLATE_LEFT:     if (is_key_down(KEY_A)    && debug) return true; break; 
    case CUBE_TRANSFORM_SCALE_UP:           if (is_key_down(KEY_UP)   && debug) return true; break;
    case CUBE_TRANSFORM_SCALE_DOWN:         if (is_key_down(KEY_DOWN) && debug) return true; break;
    case CUBE_TRANSFORM_TRANSLATE_FORWARD:  if (is_key_down(KEY_W)) return true; break; 
    case CUBE_TRANSFORM_TRANSLATE_BACKWARD: if (is_key_down(KEY_S)) return true; break; 
    case CUBE_TRANSFORM_ROTATE_CW:          if (is_key_down(KEY_J)) return true; break;
    case CUBE_TRANSFORM_ROTATE_CCW:         if (is_key_down(KEY_L)) return true; break;
    default: break;
    }

    /* controller only */
    switch (tform) {
    case CUBE_TRANSFORM_TRANSLATE_RIGHT:    if (l_joy_x >  DEAD_ZONE && debug) {*joy_scale =  l_joy_x; return true; } break; 
    case CUBE_TRANSFORM_TRANSLATE_LEFT:     if (l_joy_x < -DEAD_ZONE && debug) {*joy_scale = -l_joy_x; return true; } break; 
    case CUBE_TRANSFORM_SCALE_UP:           if (r_trigger            && debug)  return true; break;
    case CUBE_TRANSFORM_SCALE_DOWN:         if (l_trigger            && debug)  return true; break;
    case CUBE_TRANSFORM_TRANSLATE_FORWARD:  if (l_joy_y < -DEAD_ZONE) {*joy_scale = -l_joy_y; return true; } break; 
    case CUBE_TRANSFORM_TRANSLATE_BACKWARD: if (l_joy_y >  DEAD_ZONE) {*joy_scale =  l_joy_y; return true; } break; 
    case CUBE_TRANSFORM_ROTATE_CW:          if (r_joy_x < -DEAD_ZONE) {*joy_scale = -r_joy_x; return true; } break;
    case CUBE_TRANSFORM_ROTATE_CCW:         if (r_joy_x >  DEAD_ZONE) {*joy_scale =  r_joy_x; return true; } break;
    default: break;
    }

    return false;
}

bool cube_transformation(float dt, Cube *c)
{
    Cube cube = *c;
    float joy_scale = 1.0;
    bool transformed = false;

    if (should_transform(CUBE_TRANSFORM_ROTATE_CCW, &joy_scale)) {
        float rotation_amt = -dt*ROTATION_SPEED*joy_scale;
        cube.y_rotation += rotation_amt;
        cube.heading = Vector3RotateByAxisAngle(cube.heading, (Vector3){0.0f, 1.0f, 0.0}, rotation_amt);
        cube.heading = Vector3Normalize(cube.heading);
        transformed = true;
    }
    if (should_transform(CUBE_TRANSFORM_ROTATE_CW, &joy_scale)) {
        float rotation_amt = dt*ROTATION_SPEED*joy_scale;
        cube.y_rotation += rotation_amt;
        cube.heading = Vector3RotateByAxisAngle(cube.heading, (Vector3){0.0f, 1.0f, 0.0}, rotation_amt);
        cube.heading = Vector3Normalize(cube.heading);
        transformed = true;
    }
    Vector3 right   = {-cube.heading.z, 0.0f, cube.heading.x};
    if (should_transform(CUBE_TRANSFORM_TRANSLATE_FORWARD, &joy_scale)) {
        Vector3 scaled = Vector3Scale(cube.heading, dt*CUBE_SPEED*joy_scale);
        cube.position = Vector3Add(cube.position, scaled);
        transformed = true;
    }
    if (should_transform(CUBE_TRANSFORM_TRANSLATE_BACKWARD, &joy_scale)) {
        Vector3 scaled = Vector3Scale(cube.heading, -dt*CUBE_SPEED*joy_scale);
        cube.position = Vector3Add(cube.position, scaled);
        transformed = true;
    }
    if (should_transform(CUBE_TRANSFORM_TRANSLATE_RIGHT, &joy_scale)) {
        Vector3 scaled = Vector3Scale(right, dt*CUBE_SPEED*joy_scale);
        cube.position = Vector3Add(cube.position, scaled);
        transformed = true;
    }
    if (should_transform(CUBE_TRANSFORM_TRANSLATE_LEFT, &joy_scale)) {
        Vector3 scaled = Vector3Scale(right, -dt*CUBE_SPEED*joy_scale);
        cube.position = Vector3Add(cube.position, scaled);
        transformed = true;
    }
    if (should_transform(CUBE_TRANSFORM_SCALE_UP, &joy_scale)) {
        float ns = cube.scale + dt*SCALE_SPEED*joy_scale;
        cube.scale = (ns < MAX_SCALE) ? ns : MAX_SCALE;
        transformed = true;
    }
    if (should_transform(CUBE_TRANSFORM_SCALE_DOWN, &joy_scale)) {
        float ns = cube.scale - dt*SCALE_SPEED*joy_scale;
        cube.scale = (ns > MIN_SCALE) ? ns : MIN_SCALE;
        transformed = true;
    }

    *c = cube;
    return transformed;
}

bool send_matrix(float16 f16)
{
    if (sendto(sockfd, &f16, sizeof(float16), 0, (const struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        printf("failed to send matrix message\n");
        return false;
    }
    return true;
}

bool send_transformation(Cube cube)
{
    Matrix m = MatrixIdentity();
    m = MatrixMultiply(MatrixTranslate(cube.position.x, cube.position.y, cube.position.z), m);
    m = MatrixMultiply(MatrixRotateY(cube.y_rotation), m);
    m = MatrixMultiply(MatrixScale(cube.scale, cube.scale, cube.scale), m);
    return send_matrix(MatrixToFloatV(m));
}

bool init_networking()
{
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_port        = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(ip);


    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("failed to create socket\n");
        return false;
    }

    return true;
}

void log_usage(const char *program)
{
    printf("usage: %s [Options]\n", program);
    printf("    --help, log usage\n");
    printf("    --ip <IP>, e.g. 127.0.0.1\n");
    printf("    --port <PORT>, e.g. 3000\n");
}

int main(int argc, char **argv)
{
    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    const char *program = shift(argv, argc);
    while (argc > 0) {
        char *flag = shift(argv, argc);
        if (strcmp("--ip", flag) == 0) {
            ip = shift(argv, argc);
        }
        if (strcmp("--port", flag) == 0) {
            port = atoi(shift(argv, argc));
        }
        if (strcmp("--debug", flag) == 0) {
            debug = true;
        }
        if (strcmp("--help", flag) == 0) {
            log_usage(program);
            return 0;
        }
    }

    if (!init_networking()) return 1;

    init_window(500, 500, "cube");

    Cube cube = {.heading = {0.0f, 0.0f, -1.0f}, .scale = 1.0f};
    set_target_fps(60);
    while(!window_should_close()) {
        /* input */
        float dt = get_frame_time();
        if (cube_transformation(dt, &cube)) {
            if (!send_transformation(cube)) return 1;
        }

        /* drawing */
        begin_drawing(BEIGE);
        begin_mode_3d(camera);
            translate(cube.position.x, cube.position.y, cube.position.z);
            rotate_y(cube.y_rotation);
            scale(cube.scale, cube.scale, cube.scale);
            if (!draw_shape(SHAPE_CUBE)) return 1;
        end_mode_3d();
        end_drawing();
    }

    close(sockfd);
    close_window();
    return 0;
}
