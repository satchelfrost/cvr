#ifndef CVR_H_
#define CVR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 
 * The following header contains modifications from the original source "raylib.h",
 * whose license can be found in "src/ext/raylib-5.0/LICENSE". This mention is important for two 
 * reasons:
 *
 *     1) Attribution - giving credit where credit is due.
 *     2) Avoiding misrepresentation - which is to say, this is not raylib, but heavily borrowed from raylib.
 *
 *  In some cases (especially constants, enumerations, and structs), there is little difference aside from 
 *  naming conventions. On the other hand, while the API names may be the same or similar, the implementation
 *  of cvr is completely different from the raylib implementation. This is mainly due
 *  to Vulkan being the primary backend, but also the scope and aims of this project being much smaller.
 */

#define LIGHTGRAY  (Color){ 200, 200, 200, 255 }
#define GRAY       (Color){ 130, 130, 130, 255 }
#define DARKGRAY   (Color){ 80, 80, 80, 255 }
#define YELLOW     (Color){ 253, 249, 0, 255 }
#define GOLD       (Color){ 255, 203, 0, 255 }
#define ORANGE     (Color){ 255, 161, 0, 255 }
#define PINK       (Color){ 255, 109, 194, 255 }
#define RED        (Color){ 230, 41, 55, 255 }
#define MAROON     (Color){ 190, 33, 55, 255 }
#define GREEN      (Color){ 0, 228, 48, 255 }
#define LIME       (Color){ 0, 158, 47, 255 }
#define DARKGREEN  (Color){ 0, 117, 44, 255 }
#define SKYBLUE    (Color){ 102, 191, 255, 255 }
#define BLUE       (Color){ 0, 121, 241, 255 }
#define DARKBLUE   (Color){ 0, 82, 172, 255 }
#define PURPLE     (Color){ 200, 122, 255, 255 }
#define VIOLET     (Color){ 135, 60, 190, 255 }
#define DARKPURPLE (Color){ 112, 31, 126, 255 }
#define BEIGE      (Color){ 211, 176, 131, 255 }
#define BROWN      (Color){ 127, 106, 79, 255 }
#define DARKBROWN  (Color){ 76, 63, 47, 255 }
#define WHITE      (Color){ 255, 255, 255, 255 }
#define BLACK      (Color){ 0, 0, 0, 255 }
#define BLANK      (Color){ 0, 0, 0, 0 }
#define MAGENTA    (Color){ 255, 0, 255, 255 }
#define RAYWHITE   (Color){ 245, 245, 245, 255 }

#define CVR_COLOR
typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

typedef enum {
    KEY_NULL            = 0,
    KEY_APOSTROPHE      = 39,
    KEY_COMMA           = 44,
    KEY_MINUS           = 45,
    KEY_PERIOD          = 46,
    KEY_SLASH           = 47,
    KEY_ZERO            = 48,
    KEY_ONE             = 49,
    KEY_TWO             = 50,
    KEY_THREE           = 51,
    KEY_FOUR            = 52,
    KEY_FIVE            = 53,
    KEY_SIX             = 54,
    KEY_SEVEN           = 55,
    KEY_EIGHT           = 56,
    KEY_NINE            = 57,
    KEY_SEMICOLON       = 59,
    KEY_EQUAL           = 61,
    KEY_A               = 65,
    KEY_B               = 66,
    KEY_C               = 67,
    KEY_D               = 68,
    KEY_E               = 69,
    KEY_F               = 70,
    KEY_G               = 71,
    KEY_H               = 72,
    KEY_I               = 73,
    KEY_J               = 74,
    KEY_K               = 75,
    KEY_L               = 76,
    KEY_M               = 77,
    KEY_N               = 78,
    KEY_O               = 79,
    KEY_P               = 80,
    KEY_Q               = 81,
    KEY_R               = 82,
    KEY_S               = 83,
    KEY_T               = 84,
    KEY_U               = 85,
    KEY_V               = 86,
    KEY_W               = 87,
    KEY_X               = 88,
    KEY_Y               = 89,
    KEY_Z               = 90,
    KEY_LEFT_BRACKET    = 91,
    KEY_BACKSLASH       = 92,
    KEY_RIGHT_BRACKET   = 93,
    KEY_GRAVE           = 96,
    KEY_SPACE           = 32,
    KEY_ESCAPE          = 256,
    KEY_ENTER           = 257,
    KEY_TAB             = 258,
    KEY_BACKSPACE       = 259,
    KEY_INSERT          = 260,
    KEY_DELETE          = 261,
    KEY_RIGHT           = 262,
    KEY_LEFT            = 263,
    KEY_DOWN            = 264,
    KEY_UP              = 265,
    KEY_PAGE_UP         = 266,
    KEY_PAGE_DOWN       = 267,
    KEY_HOME            = 268,
    KEY_END             = 269,
    KEY_CAPS_LOCK       = 280,
    KEY_SCROLL_LOCK     = 281,
    KEY_NUM_LOCK        = 282,
    KEY_PRINT_SCREEN    = 283,
    KEY_PAUSE           = 284,
    KEY_F1              = 290,
    KEY_F2              = 291,
    KEY_F3              = 292,
    KEY_F4              = 293,
    KEY_F5              = 294,
    KEY_F6              = 295,
    KEY_F7              = 296,
    KEY_F8              = 297,
    KEY_F9              = 298,
    KEY_F10             = 299,
    KEY_F11             = 300,
    KEY_F12             = 301,
    KEY_LEFT_SHIFT      = 340,
    KEY_LEFT_CONTROL    = 341,
    KEY_LEFT_ALT        = 342,
    KEY_LEFT_SUPER      = 343,
    KEY_RIGHT_SHIFT     = 344,
    KEY_RIGHT_CONTROL   = 345,
    KEY_RIGHT_ALT       = 346,
    KEY_RIGHT_SUPER     = 347,
    KEY_KB_MENU         = 348,
    KEY_KP_0            = 320,
    KEY_KP_1            = 321,
    KEY_KP_2            = 322,
    KEY_KP_3            = 323,
    KEY_KP_4            = 324,
    KEY_KP_5            = 325,
    KEY_KP_6            = 326,
    KEY_KP_7            = 327,
    KEY_KP_8            = 328,
    KEY_KP_9            = 329,
    KEY_KP_DECIMAL      = 330,
    KEY_KP_DIVIDE       = 331,
    KEY_KP_MULTIPLY     = 332,
    KEY_KP_SUBTRACT     = 333,
    KEY_KP_ADD          = 334,
    KEY_KP_ENTER        = 335,
    KEY_KP_EQUAL        = 336,
    KEY_BACK            = 4,
    KEY_MENU            = 82,
    KEY_VOLUME_UP       = 24,
    KEY_VOLUME_DOWN     = 25 
} Keyboard_Key;

typedef enum {
    MOUSE_BUTTON_LEFT    = 0,
    MOUSE_BUTTON_RIGHT   = 1,
    MOUSE_BUTTON_MIDDLE  = 2,
    MOUSE_BUTTON_SIDE    = 3,
    MOUSE_BUTTON_EXTRA   = 4,
    MOUSE_BUTTON_FORWARD = 5,
    MOUSE_BUTTON_BACK    = 6,
} Mouse_Button;

typedef enum {
    PERSPECTIVE,
    ORTHOGRAPHIC
} Camera_Projection;

#define RL_VECTOR3_TYPE
typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;

#define CVR_CAMERA
typedef struct {
    Vector3 position;
    Vector3 target;
    Vector3 up;
    float fovy;
    int projection;
} Camera;

#define SHAPE_TYPE
typedef enum {
    SHAPE_CUBE = 0,
    SHAPE_QUAD,
    SHAPE_TETRAHEDRON,
    SHAPE_CAM,
    SHAPE_COUNT,
} Shape_Type;

#define RL_MATRIX_TYPE
typedef struct {
    float m0, m4, m8, m12;
    float m1, m5, m9, m13;
    float m2, m6, m10, m14;
    float m3, m7, m11, m15;
} Matrix;

#define RL_FLOAT_16
typedef struct float16 {
    float v[16];
} float16;

typedef enum {
    SHADER_STAGE_VERT,
    SHADER_STAGE_FRAG,
    SHADER_STAGE_BOTH,
    SHADER_STAGE_COUNT,
} ShaderStage;

/* correspond to pre-defined descriptor set layouts */
typedef enum {
    EXAMPLE_TEX,
    EXAMPLE_POINT_CLOUD,
    EXAMPLE_ADV_POINT_CLOUD,
    EXAMPLE_COMPUTE,
    EXAMPLE_COMPUTE_RASTERIZER,
    EXAMPLE_CUSTOM,
    EXAMPLE_COUNT,
} Example;

/* Structure for configuring uniform buffers */
typedef struct {
    ShaderStage stage;
    Example example;
    uint32_t binding;
} Uniform_Config;

typedef struct {
    int width;
    int height;
} Window_Size;

bool init_window(int width, int height, const char *title); /* Initialize window and vulkan context */
void close_window();                                        /* Close window and vulkan context */
bool window_should_close();                                 /* Check if window should close and poll events */
Window_Size get_window_size();
bool draw_shape(Shape_Type shape_type);                     /* Draw one of the existing shapes (solid fill) */
bool draw_shape_wireframe(Shape_Type shape_type);           /* Draw one of the existing shapes (wireframe) */
void begin_drawing(Color color);                            /* Vulkan for commands, set clear color */
void begin_drawing2();                                      /* doesn't call vk_begin_drawing or vk_begin_render_pass */
void begin_mode_3d(Camera camera);                          /* Set camera and push a matrix */
void end_mode_3d();                                         /* Pops matrix, checks for errors */
void end_drawing();                                         /* Submits commands, presents, and polls for input */
void update_camera_free(Camera *camera);                    /* Updates camera based on WASD movement, and mouse */
void begin_compute();
void end_compute();
bool compute(Example example);

bool is_key_pressed(int key);
bool is_key_down(int key);
int get_mouse_x();
int get_mouse_y();

double get_frame_time();
double get_time();
int get_fps();
int get_average_fps();
void set_target_fps(int fps);

void push_matrix();
void pop_matrix();
void translate(float x, float y, float z);
void rotate(Vector3 axis, float angle);
void rotate_x(float angle);
void rotate_y(float angle);
void rotate_z(float angle);
void rotate_xyz(Vector3 angle);
void rotate_zyx(Vector3 angle);
void scale(float x, float y, float z);
void look_at(Camera camera);
void camera_move_up(Camera *camera, float distance);

typedef struct {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
} Image;

Image load_image(const char *file_name);
void unload_image(Image img);

typedef struct {
    size_t id;
    int width;
    int height;
    int mipmaps;
    int format;
} Texture;

/* Generic buffer */
typedef struct {
    void *items;  // pointer to elements
    size_t size;  // size of the entire buffer
    size_t count; // number of items
} Buffer;

Texture load_texture_from_image(Image img);
Texture load_pc_texture_from_image(Image img);
void unload_texture(Texture texture);
void unload_pc_texture(Texture texture);
bool draw_texture(Texture texture, Shape_Type shape_type);
bool draw_pc_texture(Texture texture);
bool is_mouse_button_down(int button);

bool upload_point_cloud(Buffer buff, size_t *id);
bool upload_compute_points(Buffer buff, size_t *id, Example example);
void destroy_point_cloud(size_t id);
void destroy_compute_buff(size_t id, Example example);
bool draw_points(size_t vtx_id, Example example);
bool update_cameras_ubo(Camera *four_cameras, int shader_mode, int *cam_order);
bool get_matrix_tos(Matrix *model); /* get the top of the matrix stack */
bool get_mvp(Matrix *mvp);
bool get_mvp_float16(float16 *mvp);
Matrix get_view_proj();
bool pc_sampler_init();
Matrix get_proj(Camera camera);
bool ubo_init(Buffer buff, Example example);
bool ssbo_init(Example example);
Color color_from_HSV(float hue, float saturation, float value);
void wait_idle();
void log_fps();

#endif // CVR_H_
