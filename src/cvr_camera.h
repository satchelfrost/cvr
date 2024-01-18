#ifndef CVR_CAMERA_H_
#define CVR_CAMERA_H_

#include "ext/raylib-5.0/raymath.h"

typedef enum {
    PERSPECTIVE,
    ORTHOGRAPHIC
} Camera_Projection;

typedef struct {
    Vector3 position;
    Vector3 target;
    Vector3 up;
    float fovy;
    int projection;
} CVR_Camera;

#endif // CVR_CAMERA_H_
