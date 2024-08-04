# Cool Vulkan Renderer

Cool Vulkan Renderer (or simply cvr) is a framework heavily inspired by [raylib](https://github.com/raysan5/raylib), but with a Vulkan backend instead of OpenGL. Like raylib, cvr is written in C, and has a very similar API. There are deviations here and there, but the general idea of simplicity is still the same. For example, drawing a rotating cube can be done with the following code:

```c
#include "cvr.h"

int main()
{
    Camera camera = {
        .position   = {0.0f, 1.0f, 2.0f},
        .target     = {0.0f, 0.0f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = PERSPECTIVE,
    };

    init_window(500, 500, "cube");

    while(!window_should_close()) {
        begin_drawing(BEIGE);
        begin_mode_3d(camera);
            rotate_y(get_time());
            if (!draw_shape(SHAPE_CUBE)) return 1;
        end_mode_3d();
        end_drawing();
    }

    close_window();
    return 0;
}
```

For now, cvr is for my own learning purposes, and is not expected to have feature parity with raylib.

## Building from Source on Linux

First, install the dependencies using a package manager e.g.:

```bash
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers-dev spirv-tools libglfw3-dev libxxf86vm-dev libxi-dev
```

Next, we need a shader compiler.
Download glslc from [here](https://github.com/google/shaderc/blob/main/downloads.md) and copy (or symlink) it to your /usr/local/bin.

With the dependencies installed we can now compile the build system.

```bash
cc -o nob nob.c
```

```bash
./nob -h
```

> **__NOTE:__** nob will recompile itself if you make any changes to it. You do not need to run `cc -o nob nob.c` ever again. For more information on nob (short for "no build") see [Musializer](https://github.com/tsoding/musializer/).

## Running an example:
To list available examples run the following:
```bash
./nob -l
```
To build and run an example use the `-e` flag, e.g.:
```bash
./nob -e 3d-primitives
```

![](examples/3d-primitives/3d-primitives.png)

Chaining multiple flags can also work, for example here is how to do a clean build and run the example
```bash
./nob -ec 3d-primitives
```
