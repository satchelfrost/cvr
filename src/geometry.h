#ifndef GEOMETRY_H_
#define GEOMETRY_H_

#include "ext/raylib-5.0/raymath.h"
#include <stdint.h>

typedef struct {
    Vector3 pos;
    Vector3 color;
    Vector2 tex_coord;
} Vertex; 

typedef struct {
    Vector3 pos;
    uint8_t r, g, b;
} Small_Vertex;

#define Red 1, 0, 0
#define DarkRed 0.25f, 0, 0
#define Green 0, 1, 0
#define DarkGreen 0, 0.25f, 0
#define Blue 0, 0, 1
#define DarkBlue 0, 0, 0.25f

#define TETRAHEDRON_VERTS 4
static const Vertex tetrahedron_verts[TETRAHEDRON_VERTS] = {
    {{0.0f, -0.333f, 0.943f},     {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f}},
    {{0.816f, -0.333f, -0.471f},  {0.0f, 1.0f, 0.0f},  {0.0f, 0.0f}},
    {{-0.816f, -0.333f, -0.471f}, {0.0f, 0.0f, 0.25f}, {0.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f},          {1.0f, 0.0f, 0.0f},  {0.0f, 0.0f}}
};

/* clockwise winding order */
#define TETRAHEDRON_IDXS 12
static const uint16_t tetrahedron_indices[TETRAHEDRON_IDXS] = {
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

#define CUBE_VERTS 36
static const Vertex cube_verts[CUBE_VERTS] = {
    CUBE_SIDE(LTB, LBF, LBB, LTB, LTF, LBF, DarkRed)
    CUBE_SIDE(RTB, RBB, RBF, RTB, RBF, RTF, Red)
    CUBE_SIDE(LBB, LBF, RBF, LBB, RBF, RBB, DarkGreen)
    CUBE_SIDE(LTB, RTB, RTF, LTB, RTF, LTF, Green)
    CUBE_SIDE(LBB, RBB, RTB, LBB, RTB, LTB, DarkBlue)
    CUBE_SIDE(LBF, LTF, RTF, LBF, RTF, RBF, Blue)
};

/* clockwise winding order */
#define CUBE_IDXS 36
static const uint16_t cube_indices[CUBE_IDXS] = {
    0,  1,  2,  3,  4,  5,
    6,  7,  8,  9,  10, 11,
    12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35,
};

#define QUAD_VERTS 4
static const Vertex quad_verts[QUAD_VERTS] = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

/* clockwise winding order */
#define QUAD_IDXS 6
static const uint16_t quad_indices[QUAD_IDXS] = {
    0, 1, 2, 2, 3, 0
};

#define CAM_VERTS 8
static const Vertex cam_verts[CAM_VERTS] = {
    {{-0.5f,  0.375f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.375f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-0.5f, -0.375f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{ 0.5f, -0.375f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{-1.0f,   0.75f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 1.0f,   0.75f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.0f,  -0.75f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  -0.75f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}
};

/* clockwise winding order */
#define CAM_IDXS 24
static const uint16_t cam_indices[CAM_IDXS] = {
    0, 4, 5,
    0, 5, 1,
    1, 5, 7,
    1, 7, 3,
    3, 7, 6,
    3, 6, 2,
    4, 2, 6,
    4, 0, 2,
};


#define SHAPE_LIST \
    X(QUAD, quad)  \
    X(CUBE, cube)  \
    X(TETRAHEDRON, tetrahedron) \
    X(CAM, cam)

#endif
