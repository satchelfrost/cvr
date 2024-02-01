# C Vulkan Renderer
> **__NOTE:__** Currently only supports Linux

C Vulkan Renderer (or simply cvr) is a framework very similar to [raylib](https://github.com/raysan5/raylib), but using Vulkan instead of OpenGL. The API tries to emulate raylib in its simplicity, while making any modifications that might be necessary for Vulkan; or things I deem to be convenient at any given time.

**What this project is not**:
* This is not a Vulkan backend for raylib.
    - cvr and raylib are separate projects with different goals. Creating a Vulkan backend for raylib would likely be a pain, and I'm not even sure the original creator would want to go in that direction in the first place. This project exists soley to answer questions like: "What if raylib supported Vulkan?", "What would that look like?", and "Would it make Vulkan easier to use?".
* This project is not even close to a drop-in replacement for raylib.
    - Over time this will become less true, but it's probably best to keep your expectations low. This is really just an on-going experiment for fun.

**Goals of the project**:
* The ability to iterate fast, and test out different ideas in Vulkan.
* Basic examples like Phong lighting, and skinned animation (currently not implemented).
* Simplicity.
    - I don't want too many layers of indirection from the API to the actual implementation. You should still be able to look at the Vulkan implementation without going blind (at least thats the hope).


## Required libraries for building

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
