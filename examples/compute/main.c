#include "cvr.h"
#include "ext/raylib-5.0/raymath.h"
#include <stdlib.h>
#include "ext/nob.h"

#define PARTICLE_COUNT 256
//#define PARTICLE_COUNT 8192

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    Vector4 color;
} Particle;

Particle particles[PARTICLE_COUNT];

typedef struct {
    Buffer buff;
    size_t id;
} Compute_Buffer;

Color colors[] = {
    LIGHTGRAY, GRAY, DARKGRAY, YELLOW, GOLD, ORANGE, PINK, RED,
    MAROON, GREEN, LIME, DARKGREEN, SKYBLUE, BLUE, DARKBLUE,
    PURPLE, VIOLET, DARKPURPLE, BEIGE, BROWN, DARKBROWN, WHITE,
    BLACK, BLANK, MAGENTA, RAYWHITE
};

Vector4 color_to_vec4(Color c)
{
    return (Vector4){c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0};
}

int main()
{
    Camera camera = {
        .position   = {0.0f, 2.0f, 5.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    float width  = 800.0f;
    float height = 600.0f;
    init_window(width, height, "Compute Shader");
    set_target_fps(60);

    /* initialize the particles */
    for (size_t i = 0; i < PARTICLE_COUNT; i++) {
        float r = (float) rand() / RAND_MAX;
        float theta = (2 * i % 360) * (3.14259f / 180.0f);
        Particle *particle = &particles[i];
        float x = r * cos(theta) * height / width;
        float y = r * sin(theta);
        particle->pos.x = x;
        particle->pos.y = y;
        particle->velocity = Vector2Normalize(particle->pos);
        particle->velocity.x *= 0.0025f;
        particle->velocity.y *= 0.0025f;
        particle->color = color_to_vec4(colors[i % NOB_ARRAY_LEN(colors)]);
    }

    Compute_Buffer compute_1 = {
        .buff = {
            .items = particles,
            .size  = PARTICLE_COUNT * sizeof(Particle),
            .count = PARTICLE_COUNT,
        },
    };
    Compute_Buffer compute_2 = compute_1;

    if (!upload_compute_points(compute_1.buff, &compute_1.id)) return 1;
    if (!upload_compute_points(compute_2.buff, &compute_2.id)) return 1;

    float time = 0.0f;
    Buffer buff = {
        .size  = sizeof(float),
        .count = 1,
        .items = &time,
    };
    if (!ubo_init(buff, EXAMPLE_COMPUTE)) return 1;

    while (!window_should_close()) {
        time = get_time();
        begin_compute();
            if (!compute_points(compute_2.id)) return 1;
        end_compute();

        begin_drawing(BLACK);
        begin_mode_3d(camera);
            if (!draw_points(compute_2.id, EXAMPLE_COMPUTE)) return 1;
        end_mode_3d();
        end_drawing();
    }

    destroy_compute_buff(compute_1.id);
    destroy_compute_buff(compute_2.id);
    close_window();
    return 0;
}
