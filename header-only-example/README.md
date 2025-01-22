# Bare bones example that only uses vk_ctx.h
This is a separate example from the rest because it's meant to be minimalistic, separate from the core of cvr,
and only requiring a few simple commands to build. Note that you do still need to link with vulkan and glfw.

```bash
gcc -o main main.c -lvulkan -lglfw && ./main
```

Also, I've precompiled the shaders for this example. Alternatively you can do the following:


```bash
glslc -o default.vert.spv default.vert
```

and

```bash
glslc -o default.frag.spv default.frag
```
