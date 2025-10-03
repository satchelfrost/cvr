#ifndef GEOMETRY_H_
#define GEOMETRY_H_

#include <stdint.h>

typedef struct {
    Vector3 pos;
    Vector3 color;
    Vector2 tex_coord;
} Vertex; 

typedef struct {
    Vector3 pos;
    uint8_t r, g, b, a;
} Small_Vertex;

#define Red 1, 0, 0
#define DarkRed 0.25f, 0, 0
#define Green 0, 1, 0
#define DarkGreen 0, 0.25f, 0
#define Blue 0, 0, 1
#define DarkBlue 0, 0, 0.25f

#define TETRAHEDRON_VTX_COUNT 4
static Vertex tetrahedron_verts[TETRAHEDRON_VTX_COUNT] = {
    {{0.0f, -0.333f, 0.943f},     {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f}},
    {{0.816f, -0.333f, -0.471f},  {0.0f, 1.0f, 0.0f},  {0.0f, 0.0f}},
    {{-0.816f, -0.333f, -0.471f}, {0.0f, 0.0f, 0.25f}, {0.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f},          {1.0f, 0.0f, 0.0f},  {0.0f, 0.0f}}
};

/* clockwise winding order */
#define TETRAHEDRON_IDX_COUNT 12
static uint16_t tetrahedron_indices[TETRAHEDRON_IDX_COUNT] = {
    0, 3, 1,
    0, 2, 3,
    0, 1, 2,
    3, 2, 1,
};

#define  LBB -0.5f, -0.5f, -0.5f
#define  LBF -0.5f, -0.5f,  0.5f
#define  LTB -0.5f,  0.5f, -0.5f
#define  LTF -0.5f,  0.5f,  0.5f
#define  RBB  0.5f, -0.5f, -0.5f
#define  RBF  0.5f, -0.5f,  0.5f
#define  RTB  0.5f,  0.5f, -0.5f
#define  RTF  0.5f,  0.5f,  0.5f

#define CUBE_SIDE(V1, V2, V3, V4, V5, V6, COLOR) {{V1}, {COLOR}, {1.0f, 1.0f}}, {{V2}, {COLOR}, {1.0f, 0.0f}}, {{V3}, {COLOR}, {0.0f, 0.0f}}, {{V4}, {COLOR}, {1.0f, 1.0f}}, {{V5}, {COLOR}, {0.0f, 0.0f}}, {{V6}, {COLOR}, {0.0f, 1.0f}},

#define CUBE_VTX_COUNT 36
static Vertex cube_verts[CUBE_VTX_COUNT] = {
    CUBE_SIDE(LTB, LBF, LBB, LTB, LTF, LBF, DarkRed)
    CUBE_SIDE(RTB, RBB, RBF, RTB, RBF, RTF, Red)
    CUBE_SIDE(LBB, LBF, RBF, LBB, RBF, RBB, DarkGreen)
    CUBE_SIDE(LTB, RTB, RTF, LTB, RTF, LTF, Green)
    CUBE_SIDE(LBB, RBB, RTB, LBB, RTB, LTB, DarkBlue)
    CUBE_SIDE(LBF, LTF, RTF, LBF, RTF, RBF, Blue)
};

/* clockwise winding order */
#define CUBE_IDX_COUNT 36
static uint16_t cube_indices[CUBE_IDX_COUNT] = {
    0,  1,  2,  3,  4,  5,
    6,  7,  8,  9,  10, 11,
    12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35,
};

#define QUAD_VTX_COUNT 4
static Vertex quad_verts[QUAD_VTX_COUNT] = {
    {{-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

/* clockwise winding order */
#define QUAD_IDX_COUNT 6
static uint16_t quad_indices[QUAD_IDX_COUNT] = {
    0, 1, 2, 2, 3, 0
};

#define FRUSTUM_VTX_COUNT 8
static Vertex frustum_verts[FRUSTUM_VTX_COUNT] = {
    // {{-0.5f,  0.375f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    // {{ 0.5f,  0.375f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    // {{-0.5f, -0.375f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    // {{ 0.5f, -0.375f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    // {{-1.0f,   0.75f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    // {{ 1.0f,   0.75f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    // {{-1.0f,  -0.75f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    // {{ 1.0f,  -0.75f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}

    {{-0.2f,  0.2f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.2f,  0.2f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-0.2f, -0.2f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{ 0.2f, -0.2f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{-1.0f,  1.0f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 1.0f,  1.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.0f, -1.0f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{ 1.0f, -1.0f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}
};

/* clockwise winding order */
#define FRUSTUM_IDX_COUNT 24
static uint16_t frustum_indices[FRUSTUM_IDX_COUNT] = {
    0, 4, 5,
    0, 5, 1,
    1, 5, 7,
    1, 7, 3,
    3, 7, 6,
    3, 6, 2,
    4, 2, 6,
    4, 0, 2,
};

typedef enum {
    PRIMITIVE_QUAD,
    PRIMITIVE_CUBE,
    PRIMITIVE_TETRAHEDRON,
    PRIMITIVE_FRUSTUM,
    PRIMITIVE_COUNT,
} Primitive_Type; 

typedef struct {
    void *items;
    size_t count;
    size_t size;
} Static_Buffer;

typedef struct {
    Static_Buffer vtx_buff;
    Static_Buffer idx_buff;
} Primitive;

static Primitive primitives[PRIMITIVE_COUNT] = {
    [PRIMITIVE_QUAD] = {
        .idx_buff = {
            .items = quad_indices,
            .count = QUAD_IDX_COUNT,
            .size  = sizeof(quad_indices) * QUAD_IDX_COUNT,
        },
        .vtx_buff = {
            .items = quad_verts,
            .count = QUAD_VTX_COUNT,
            .size  = sizeof(quad_verts) * QUAD_VTX_COUNT,
        },
    },
    [PRIMITIVE_CUBE] = {
        .idx_buff = {
            .items = cube_indices,
            .count = CUBE_IDX_COUNT,
            .size  = sizeof(cube_indices) * CUBE_IDX_COUNT,
        },
        .vtx_buff = {
            .items = cube_verts,
            .count = CUBE_VTX_COUNT,
            .size  = sizeof(cube_verts) * CUBE_VTX_COUNT,
        },
    },
    [PRIMITIVE_TETRAHEDRON] = {
        .idx_buff = {
            .items = tetrahedron_indices,
            .count = TETRAHEDRON_IDX_COUNT,
            .size  = sizeof(tetrahedron_indices) * TETRAHEDRON_IDX_COUNT,
        },
        .vtx_buff = {
            .items = tetrahedron_verts,
            .count = TETRAHEDRON_VTX_COUNT,
            .size  = sizeof(tetrahedron_verts) * TETRAHEDRON_VTX_COUNT,
        },
    },
    [PRIMITIVE_FRUSTUM] = {
        .idx_buff = {
            .items = frustum_indices,
            .count = FRUSTUM_IDX_COUNT,
            .size  = sizeof(frustum_indices) * FRUSTUM_IDX_COUNT,
        },
        .vtx_buff = {
            .items = frustum_verts,
            .count = FRUSTUM_VTX_COUNT,
            .size  = sizeof(frustum_verts) * FRUSTUM_VTX_COUNT,
        },
    },
};

#endif
