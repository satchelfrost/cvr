# C Vulkan Renderer
Currently only supports Linux and draws a primitive

## Dependencies

Vulkan & GLFW

### Using apt
```bash
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers-dev spirv-tools libglfw3-dev
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
