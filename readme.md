# C Vulkan Renderer

C Vulkan Renderer (or simply cvr) is a project initially inspired by [raylib](https://github.com/raysan5/raylib),
but with a Vulkan twist. Essentially, it's a basic renderer I use in my research.

The header [vk_ctx.h](src/vk_ctx.h) is meant to be a header-only-ish library that will work outside of cvr.
I say "ish", because it does rely on linkage with glfw, but in the future I want to make this optional.
cvr is essentially a few extra conveniences around vk_ctx.h (e.g. a matrix stack, frame timer, a 
few simple primitives etc.).

Some examples are going to look familiar to users of raylib like [3d-primitives](examples/3d-primitives/main.c),
whereas other more complicated ones like [point-raster](examples/point-raster/main.c) are going to be more familiar
to the avid Vulkan user.

## Building on Linux

### Install libraries

Install using a package manager e.g.

```bash
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers-dev spirv-tools libxxf86vm-dev libxi-dev
```

Hopefully, I didn't miss any, but if I did feel free to let me know.

### Install shader compiler

Download glslc from [here](https://github.com/google/shaderc/blob/main/downloads.md)
and copy (or symlink) it to your /usr/local/bin.

### Build nob

The build system we use is nob, hence the nob.c/nob.h.

```bash
cc -o nob nob.c
```

Interested in [nob](https://github.com/tsoding/nob.h)?

### Run an example
To list available examples run the following:
```bash
./nob -l
```
To build and run an example use the `-e` flag, e.g.:
```bash
./nob -e 3d-primitives
```

> **Note:** The example `arena-point-raster` contains private data, so it won't run, but you can still examine the source code.
Also, `video` and `gltf` rely on assets that are not found in this repo, so they won't run properly until you download the
assets (check their source code comments). The rest of the examples should work out-of-the-box.

## Building on Mac/Windows
Not yet supported.

## Additional flags

Specify build target flag, requires that the proper cross-compilers are installed (e.g. x86_64-w64-mingw32-gcc).
See [nob.c](nob.c) for details.
e.g. usage

```bash
./nob -e 3d-primitives -t windows
```

Debug launch (gf2), uses gf2 and expects it to be in your path.
e.g. usage

```bash
./nob -ed 3d-primitives
```

Renderdoc launch, expects renderdoccmd to be in your path. You
e.g. usage

```bash
./nob -er 3d-primitives
```

Then, take a screen shot while launched (usually f11/f12), then hit escape. The example window will close,
and a new one of renderdoc will launch.
