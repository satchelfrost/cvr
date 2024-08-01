# C Vulkan Renderer

C Vulkan Renderer (or simply cvr) is a framework heavily inspired by [raylib](https://github.com/raysan5/raylib), but with a Vulkan backend instead of OpenGL. Like raylib, cvr is written in C, and has a very similar API. There are deviations here and there, but the general idea of simplicity is still the same. For example, drawing a rotating cube, should take less than 30 lines of code.

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

For now, cvr is more of an experiment, and one should not expect feature parity with raylib.

## Required libraries for building

> **__NOTE:__** Currently only tested on Linux. I know it's in C, but my excuse is that I haven't had the need to test on Windows or Mac.

Vulkan & GLFW

### Using apt
```bash
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers-dev spirv-tools libglfw3-dev libxxf86vm-dev libxi-dev
```

Download glslc from [here](https://github.com/google/shaderc/blob/main/downloads.md) and copy (or symlink) it to your /usr/local/bin.

For more in depth instructions visit [here](https://vulkan-tutorial.com/Development_environment#page_Linux)

## Build system: `nob`
Instead of using scripts, make, cmake, or some other alternative, this project uses nob (short for "no build"). Nob is a bootstrapping build system that only requires a C compiler. The header file (`nob.h`) was developed by Alexey Kutepov (a.k.a Tsoding), but the implementation (`nob.c`) is specific to each individual project.

Here's how it works, build `nob.c` once,

```bash
cc -o nob nob.c
```
and you never need to directly build it again. Even if you make changes to `nob.c`, that will be detected and nob will recompile itself.

For usage options of this project type the following command
```bash
./nob -h
```

## Running an example:
To list available examples run the following:
```bash
./nob -l
```
To build and run an example use the `-e` flag, e.g.:
```bash
./nob -e psychedelic
```

![](examples/psychedelic/psychedelic.gif)

For more up to date commands see `-h` flag
```bash
./nob -h
```

## Compilation Database (Optional)
If you're using the clangd language server for better autocomplete/go-to-definitions, then there's an option to generate a compilation database (compile_commands.json) in the build folder. It should be noted that this requires the clang compiler. After generating the database once, all future invokations of `nob` should use clang automatically (otherwise the default compiler is `cc`). To generate the db simply run:

```bash
./nob -d
```
To clean the build folder and generate the database (for example if `cc` was run previously and you want to be consistent with compilers) run:

```bash
./nob -d -c
```
or simply

```bash
./nob -dc
```
