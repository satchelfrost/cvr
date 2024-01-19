# C Vulkan Renderer
Currently only supports Linux and draws a primitive

## Dependencies

Vulkan & GLFW

### Using apt
```bash
sudo apt install vulkan-tools
```
```bash
sudo apt install libvulkan-dev
```
```bash
sudo apt install vulkan-validationlayers-dev spirv-tools
```
```bash
sudo apt install libglfw3-dev
```

Download glslc from [here](https://github.com/google/shaderc/blob/main/downloads.md) and copy (or symlink) it to your /usr/local/bin.

For more in depth instructions visit [here](https://vulkan-tutorial.com/Development_environment#page_Linux)


## Compilation Database (Optional)
Note about potentially using a compilation database for clangd (i.e. autocompletion, squigglies, etc.). For now I'm not making clang, and the users shell a dependency; however I'm noting the steps so I don't forget, and in case I want to do this later.

To manually generate a compilation database I followed the steps outlined [here](https://sarcasm.github.io/notes/dev/compilation-database.html#clang), I want to mention it somewhere so I don't forget.

First use clang to compile, and use the "-MJ" flag to generate compilation database files e.g.

```bash
clang -MJ a.o.json -o a.o -c a.c
clang -MJ b.o.json -o b.o -c b.c
```
You can also modify `nob.c` just change `cc` -> `clang`, and after that add an additional nob_cmd_append(&cmd, "-MJ", example.o.json)

These json files can be concatenated using the following sed command:

```bash
sed -e '1s/^/[\n/' -e '$s/,$/\n]/' *.o.json > compile_commands.json
```

Then the original `*.o.json` can be deleted. Now, having the compile_commands.json at the root directory should enable clangd to workout things like included files.

If all you need is include files then simply making a `.clangd` file with the following will suffice:

```yaml
CompileFlags:
  Add: [-I/full/path/to/cvr/src]
```

For anything more complex, a compilation database may be required for better autocompletion etc.

