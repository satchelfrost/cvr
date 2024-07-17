#include "cvr.h"

#define PARTICLE_COUNT 400

typedef struct {
    Vector2 pos;
    Vector2 velocity;
    Vector4 color;
} Particle;

Particle paricles[PARTICLE_COUNT] = {0};
Color colors = {
    LIGHTGRAY, GRAY, DARKGRAY, YELLOW, GOLD, ORANGE, PINK, RED,
    MAROON, GREEN, LIME, DARKGREEN, SKYBLUE, BLUE, DARKBLUE,
    PURPLE, VIOLET, DARKPURPLE, BEIGE, BROWN, DARKBROWN, WHITE,
    BLACK, BLANK, MAGENTA, RAYWHITE
};

Vector4 color_to_vec4(Color *c)
{
    return (Vector4){c->r / 255.0f, c->g / 255.0f, c->b / 255.0f, 1.0};
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

    /* initialize the particles */
    for (size_t i = 0; i < PARTICLE_COUNT; i++) {
        float r = rand() / RAND_MAX;
        float theta = (i % 360) * (3.14259f / 180.0f);
        Particle *particle = &particles[i];
        particle->velocity.x = particle->pos.x = r * cos(theta) * height / width;
        particle->velocity.y = particle->pos.y = r * sin(theta);
        particle->color = color_to_vec4(colors[i % NOB_ARRAY_LEN(colors)]);
    }

    Buffer buff = {
        .items = particles,
        .size  = PARTICLE_COUNT * sizeof(Particle),
        .count = PARTICLE_COUNT,
    };

    if (!upload_compute_points(buff)) return 1;

    while (!window_should_close()) {
        begin_drawing(RED);
        begin_mode_3d(camera);
        end_mode_3d();
        end_drawing();
    }

    close_window();
    return 0;
}
