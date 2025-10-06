#include <string.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

#define FILE_DOES_NOT_EXIST 0
#define FILE_EXISTS 1
#define FILE_CHK_ERR -1 

typedef struct {
    const char **names;
    size_t count;
} CFiles;

typedef struct {
    const char **names;
    size_t count;
} Shaders;

typedef enum {
    TARGET_LINUX,
    TARGET_WINDOWS,
    TARGET_COUNT,
} Target;

const char *target_names[TARGET_COUNT] = {
    "linux",
    "windows",
};

typedef enum {
    HOST_LINUX,
    HOST_WINDOWS,
} Host;

/* Data necessary for compiling an example */
typedef struct {
    const char *name;
    const Shaders shaders;

    /* WARNING: Please note that if there are any other c files, other than main.c, in the example directory,
     * they will not compiled as separate translation units, but instead require files to be included
     * directly into main.c (i.e. a "unity build").
     *
     * For example:
     *
     *  .c_files = {
     *      .names = (const char *[]) {
     *          "main",
     *          "input",
     *          "cmd_and_log",
     *      },
     *      .count = 3
     *  },
     *
     *  implies that main.c has done the following:
     *
     *  #include "input.c"
     *  #include "cmd_and_log.c"
     *
     *  If you make changes to any of these, there will be an automatic recompile,
     *  however neither input.o, nor cmd_and_log.o will ever be generated.
     */ 
    const CFiles c_files;
} Example;

static const char *default_shader_names[] = {"default.vert.glsl", "default.frag.glsl"};
static const char *default_c_file_names[] = {"main"};

static Example examples[] = {
    {
        .name = "3d-primitives",
        .shaders = {
            .names = default_shader_names,
            .count = NOB_ARRAY_LEN(default_shader_names)
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "orthographic",
        .shaders = {
            .names = default_shader_names,
            .count = NOB_ARRAY_LEN(default_shader_names)
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "psychedelic",
        .shaders = {
            .names = default_shader_names,
            .count = NOB_ARRAY_LEN(default_shader_names)
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "waves",
        .shaders = {
            .names = default_shader_names,
            .count = NOB_ARRAY_LEN(default_shader_names)
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "texture3D",
        .shaders = {
            .names = (const char *[]) {
                "default.vert.glsl",
                "default.frag.glsl",
                "texture.vert.glsl",
                "texture.frag.glsl"
            },
            .count = 4
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "depth",
        .shaders = {
            .names = default_shader_names,
            .count = NOB_ARRAY_LEN(default_shader_names)
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "movement",
        .shaders = {
            .names = default_shader_names,
            .count = NOB_ARRAY_LEN(default_shader_names)
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "point-cloud",
        .shaders = {
            .names = (const char *[]) {
                "point-cloud.vert.glsl",
                "point-cloud.frag.glsl",
            },
            .count = 2
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "compute",
        .shaders = {
            .names = (const char *[]) {
                "default.vert.glsl",
                "default.frag.glsl",
                "default.comp.glsl",
                "particle.vert.glsl",
                "particle.frag.glsl",
            },
            .count = 5,
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "point-raster",
        .shaders = {
            .names = (const char *[]) {
                "default.vert.glsl",
                "default.frag.glsl",
                "sst.vert.glsl",
                "sst.frag.glsl",
                "render.comp.glsl",
                "resolve.comp.glsl",
            },
            .count = 6,
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "video",
        .shaders = {
            .names = (const char *[]) {
                "default.vert.glsl",
                "default.frag.glsl",
                "texture.vert.glsl",
                "texture.frag.glsl",
            },
            .count = 4,
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "gltf",
        .shaders = {
            .names = (const char *[]) {
                "default.vert.glsl",
                "default.frag.glsl",
                "gltf.vert.glsl",
                "gltf.frag.glsl",
            },
            .count = 4,
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "mixed-raster",
        .shaders = {
            .names = (const char *[]) {
                "default.vert.glsl",
                "default.frag.glsl",
                "sst.vert.glsl",
                "sst.frag.glsl",
                "mix.comp.glsl",
                "render.comp.glsl",
                "resolve.comp.glsl",
            },
            .count = 7,
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
    {
        .name = "texture_distort",
        .shaders = {
            .names = (const char *[]) {
                "texture_distort.vert.glsl",
                "texture_distort.frag.glsl"
            },
            .count = 2
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
    },
};

typedef struct {
    int argc;
    char **argv;
    int forwarded_argc;
    char **forwarded_argv;
    char *program;
    char *supplied_name;
    Example *example;
    Target target;
    char *target_name;
    bool release;
    Host host;
    bool debug;
    bool renderdoc;
} Config;

void log_usage(const char *program)
{
    nob_log(NOB_INFO, "usage: %s <flags> <optional_input>", program);
    nob_log(NOB_INFO, "    -e followed by <example_name> to build");
    nob_log(NOB_INFO, "    -h help (log usage)");
    nob_log(NOB_INFO, "    -c clean build");
    nob_log(NOB_INFO, "    -l list available examples");
    nob_log(NOB_INFO, "    -t specify build target (linux, windows, quest)");
    nob_log(NOB_INFO, "    -g debug launch (gf2)"); // https://github.com/nakst/gf
    nob_log(NOB_INFO, "    -r renderdoc launch");
    nob_log(NOB_INFO, "    -a forward command line args (e.g. -a 'arg1 arg2 ...')");
}

void print_examples();

bool parse_sub_args(const char *arg_str, char ***forwarded_argv, int *forwarded_argc)
{
    Nob_String_View sv = nob_sv_from_cstr(arg_str);
    int argc = 0;

    do {
        nob_sv_chop_by_delim(&sv, ' ');
        argc++;
    } while (sv.count);

    *forwarded_argc = argc;

    char **argv = (*forwarded_argv) = malloc(argc * sizeof(char *));
    sv = nob_sv_from_cstr(arg_str);
    for (int i = 0; i < argc; i++) {
        Nob_String_View tmp_sv = nob_sv_chop_by_delim(&sv, ' ');
        const char *cstring = nob_temp_sv_to_cstr(tmp_sv);
        argv[i] = nob_temp_strdup(cstring);
    }

    return true;
}

bool handle_usr_args(Config *config)
{
    bool result = true;
    Nob_Cmd cmd = {0};

    config->program = nob_shift_args(&config->argc, &config->argv);
    bool target_exists = false;

    while (config->argc > 0) {
        char flag;
        char *flags = nob_shift_args(&config->argc, &config->argv);
        while ((flag = (++flags)[0])) { // ignores '-'
            switch (flag) {
            case 'c':
                nob_log(NOB_INFO, "clean build requested, removing build folder");
                // TODO: I want this to remove only the build/target not the main build
                nob_cmd_append(&cmd, "rm", "build", "res", "-rf");
                if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
                break;
            case 'e':
                if (config->argc == 0) {
                    log_usage(config->program);
                    nob_return_defer(false);
                }
                config->supplied_name = nob_shift_args(&config->argc, &config->argv);
                break;
            case 't':
                if (config->argc == 0) {
                    log_usage(config->program);
                    nob_return_defer(false);
                }
                config->target_name = nob_shift_args(&config->argc, &config->argv);
                for (size_t i = 0; i < TARGET_COUNT; i++) {
                    if (!target_exists &&
                        strcmp(target_names[i], config->target_name) == 0) {
                        config->target = i;
                        target_exists = true;
                    }
                }
                if (!target_exists) {
                    nob_log(NOB_ERROR, "No such target %s exists", config->target_name);
                    nob_return_defer(false);
                }
                break;
            case 'h':
                log_usage(config->program);
                nob_return_defer(false);
                break;
            case 'l':
                print_examples();
                nob_return_defer(false);
                break;
            case 'g':
                config->debug = true;
                break;
            case 'r':
                config->renderdoc = true;
                break;
            case 'a':
                if (!parse_sub_args(config->argv[0], &config->forwarded_argv, &config->forwarded_argc))
                    nob_return_defer(false);
                nob_shift_args(&config->argc, &config->argv);
                break;
            default:
                nob_log(NOB_ERROR, "unrecognized flag %c", flag);
                log_usage(config->program);
                nob_return_defer(false);
            }
        }
    }

    if (!config->supplied_name) {
        nob_log(NOB_ERROR, "no example supplied");
        log_usage(config->program);
        nob_return_defer(false);
    }

defer:
    nob_cmd_free(cmd);
    return result;
}

bool build_glfw_linux(const char *platform_path)
{
    bool result = true;
    Nob_Cmd cmd = {0};

    const char *build_path = nob_temp_sprintf("%s/glfw", platform_path);
    if (!nob_mkdir_if_not_exists(build_path)) nob_return_defer(false);
    const char *output_path = nob_temp_sprintf("%s/glfw.o", build_path);
    const char *input_path = nob_temp_sprintf("./external/raylib-5.0/rglfw.c");
    if (nob_needs_rebuild(output_path, &input_path, 1)) {
        nob_cmd_append(&cmd, "cc");
        nob_cmd_append(&cmd, "-I./external/raylib-5.0/glfw/include");
        nob_cmd_append(&cmd, "-I./external/raylib-5.0/glfw");
        nob_cmd_append(&cmd, "-c", input_path);
        nob_cmd_append(&cmd, "-o", output_path);
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }

defer:
    nob_cmd_free(cmd);
    return result;
}

bool build_glfw_win(const char *platform_path)
{
    bool result = true;
    Nob_Cmd cmd = {0};

    const char *build_path = nob_temp_sprintf("%s/glfw", platform_path);
    if (!nob_mkdir_if_not_exists(build_path)) nob_return_defer(false);
    const char *output_path = nob_temp_sprintf("%s/glfw.o", build_path);
    const char *input_path = nob_temp_sprintf("./external/raylib-5.0/rglfw.c");
    if (nob_needs_rebuild(output_path, &input_path, 1)) {
        nob_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
        nob_cmd_append(&cmd, "-I./external/raylib-5.0/glfw/include");
        nob_cmd_append(&cmd, "-I./external/raylib-5.0/glfw");
        nob_cmd_append(&cmd, "-c", input_path);
        nob_cmd_append(&cmd, "-o", output_path);
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }

defer:
    nob_cmd_free(cmd);
    return result;
}

bool build_glfw(Config config, const char *platform_path)
{
    if (config.target == TARGET_LINUX) {
        return build_glfw_linux(platform_path);
    } else if (config.target == TARGET_WINDOWS && config.host == HOST_LINUX) {
        return build_glfw_win(platform_path);
    } else {
        nob_log(NOB_ERROR, "glfw target %d not yet supported", config.target);
        return false;
    }
}

static const char *cvr[] = {
    "core",
};

bool build_cvr_linux(const char *platform_path)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths obj_files = {0};

    /* build modules */
    const char *build_path = nob_temp_sprintf("%s/cvr", platform_path);
    if (!nob_mkdir_if_not_exists(build_path)) nob_return_defer(false);
    for (size_t i = 0; i < NOB_ARRAY_LEN(cvr); i++) {
        const char *output_path = nob_temp_sprintf("%s/%s.o", build_path, cvr[i]);
        const char *input_path = nob_temp_sprintf("./src/%s.c", cvr[i]);
        const char *header_path = nob_temp_sprintf("./src/rag_vk.h", cvr[i]);
        nob_da_append(&obj_files, output_path);
        if (nob_needs_rebuild(output_path, &input_path, 1) ||
            nob_needs_rebuild(output_path, &header_path, 1)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "cc");
            nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP_GLFW");
            nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
            nob_cmd_append(&cmd, "-I./external");
            nob_cmd_append(&cmd, "-I./external/raylib-5.0/glfw/include");
            nob_cmd_append(&cmd, "-DVK_VALIDATION");
            nob_cmd_append(&cmd, "-c", input_path);
            nob_cmd_append(&cmd, "-o", output_path);
            Nob_Proc proc = nob_cmd_run_async(cmd);
            nob_da_append(&procs, proc);
        }
    }

    if (!nob_procs_wait(procs)) nob_return_defer(false);

    /* create library */
    const char *libcvr_path = nob_temp_sprintf("%s/libcvr.a", build_path);
    if (nob_needs_rebuild(libcvr_path, obj_files.items, obj_files.count)) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "ar", "-crs", libcvr_path);
        for (size_t i = 0; i < obj_files.count; i++) {
            const char *input_path = nob_temp_sprintf("%s/%s.o", build_path, cvr[i]);
            nob_cmd_append(&cmd, input_path);
        }
        nob_cmd_append(&cmd, nob_temp_sprintf("%s/glfw/glfw.o", platform_path));
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }

defer:
    nob_cmd_free(cmd);
    nob_cmd_free(procs);
    nob_cmd_free(obj_files);
    return result;
}

bool build_cvr_win(const char *platform_path)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths obj_files = {0};

    /* build modules */
    const char *build_path = nob_temp_sprintf("%s/cvr", platform_path);
    if (!nob_mkdir_if_not_exists(build_path)) nob_return_defer(false);
    for (size_t i = 0; i < NOB_ARRAY_LEN(cvr); i++) {
        const char *output_path = nob_temp_sprintf("%s/%s.o", build_path, cvr[i]);
        const char *input_path = nob_temp_sprintf("./src/%s.c", cvr[i]);
        const char *header_path = nob_temp_sprintf("./src/rag_vk.h", cvr[i]);
        nob_da_append(&obj_files, output_path);
        if (nob_needs_rebuild(output_path, &input_path, 1) ||
            nob_needs_rebuild(output_path, &header_path, 1)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
            nob_cmd_append(&cmd, "-DPLATFORM_DESKTOP_GLFW");
            nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
            nob_cmd_append(&cmd, "-I./external");
            nob_cmd_append(&cmd, "-I./external/raylib-5.0/glfw/include");
            nob_cmd_append(&cmd, "-c", input_path);
            nob_cmd_append(&cmd, "-o", output_path);
            Nob_Proc proc = nob_cmd_run_async(cmd);
            nob_da_append(&procs, proc);
        }
    }

    if (!nob_procs_wait(procs)) nob_return_defer(false);

    /* create library */
    const char *libcvr_path = nob_temp_sprintf("%s/libcvr.dll", build_path);
    if (nob_needs_rebuild(libcvr_path, obj_files.items, obj_files.count)) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "x86_64-w64-mingw32-gcc", "-static-libgcc", "-shared", "-o", libcvr_path);
        nob_cmd_append(&cmd, "-L./lib");
        for (size_t i = 0; i < obj_files.count; i++) {
            const char *input_path = nob_temp_sprintf("%s/%s.o", build_path, cvr[i]);
            nob_cmd_append(&cmd, input_path);
        }
        nob_cmd_append(&cmd, nob_temp_sprintf("%s/glfw/glfw.o", platform_path));
        nob_cmd_append(&cmd, "-lwinmm", "-lgdi32", "-lvulkan-1");
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }

defer:
    nob_cmd_free(cmd);
    nob_cmd_free(procs);
    nob_cmd_free(obj_files);
    return result;
}

bool build_cvr(Config *config, const char *platform_path)
{
    if (config->target == TARGET_LINUX) {
        return build_cvr_linux(platform_path);
    } else if (config->target == TARGET_WINDOWS && config->host == HOST_LINUX) {
        return build_cvr_win(platform_path);
    } else {
        nob_log(NOB_ERROR, "cvr target %d not yet supported", config->target);
        return false;
    }
}

const char *src_file_to_shader_stage_flag(const char *src)
{
    if (strstr(src, "vert.glsl")) return "-fshader-stage=vert";
    if (strstr(src, "frag.glsl")) return "-fshader-stage=frag";
    if (strstr(src, "comp.glsl")) return "-fshader-stage=comp";
    assert(0 && "shader extension unrecognized");
}

bool compile_shaders(Config config)
{
    bool result = true;

    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

    const Example *example = config.example;
    Shaders shaders = example->shaders;
    const char *example_path = nob_temp_sprintf("examples/%s", example->name);
    nob_log(NOB_INFO, "checking shaders for %s", example_path);

    char *dst = NULL;
    dst = nob_temp_sprintf("./build/%s/%s/res", target_names[config.target], example_path);
    if (!dst) {
        nob_log(NOB_ERROR, "could create destination folder for shaders");
        nob_return_defer(false);
    }
    if (!nob_mkdir_if_not_exists(dst)) nob_return_defer(false);
    const char *src = nob_temp_sprintf("%s/res", example_path);

    for (size_t i = 0; i < shaders.count; i++) {
        const char *shader_dst = nob_temp_sprintf("%s/%s.spv", dst, shaders.names[i]);
        const char *shader_src = nob_temp_sprintf("%s/%s", src, shaders.names[i]);

        if (nob_needs_rebuild(shader_dst, &shader_src, 1)) {
            cmd.count = 0;
            const char *flag = src_file_to_shader_stage_flag(shader_src);
            nob_cmd_append(&cmd, "glslc", flag, shader_src, "-o", shader_dst);
            Nob_Proc proc = nob_cmd_run_async(cmd);
            nob_da_append(&procs, proc);
        }
    }
    if (!nob_procs_wait(procs)) nob_return_defer(false);

defer:
    nob_cmd_free(cmd);
    nob_da_free(procs);
    return result;
}

bool build_example_linux(Config config, const char *build_path)
{
    const Example *example = config.example;
    const char *example_path = nob_temp_sprintf("examples/%s", example->name);
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_File_Paths c_files = {0};

    for (size_t i = 0; i < example->c_files.count; i++)
        nob_da_append(&c_files, nob_temp_sprintf("%s/%s.c", example_path, example->c_files.names[i]));

    /* build and link */
    const char *libcvr_path = nob_temp_sprintf("./build/%s/cvr/libcvr.a", target_names[config.target]);
    const char *exec_path = nob_temp_sprintf("%s/%s", build_path, example->name);
    bool c_files_updated = nob_needs_rebuild(exec_path, c_files.items, example->c_files.count);
    bool libcvr_updated = nob_needs_rebuild(exec_path, &libcvr_path, 1);
    if (c_files_updated || libcvr_updated) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "cc");
        nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
        nob_cmd_append(&cmd, "-I./src");
        nob_cmd_append(&cmd, "-I./external");
        nob_cmd_append(&cmd, "-o", exec_path);
        nob_cmd_append(&cmd, nob_temp_sprintf("%s/main.c", example_path));
        const char *cvr_path = nob_temp_sprintf("-L./build/%s/cvr", target_names[config.target]);
        nob_cmd_append(&cmd, cvr_path, "-l:libcvr.a");
        nob_cmd_append(&cmd, "-lvulkan", "-ldl", "-lpthread", "-lX11", "-lXxf86vm", "-lXrandr", "-lXi", "-lm");
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }

defer:
    nob_cmd_free(c_files);
    nob_cmd_free(cmd);
    return result;
}

/* specifically if the destination file does not exists */
bool copy_file_if_not_exists(const char *src, const char *dst)
{
    switch (nob_file_exists(dst)) {
    case FILE_DOES_NOT_EXIST:
        if (!nob_copy_file(src, dst)) return false;
        else return true;
    case FILE_CHK_ERR:
        return false;
    case FILE_EXISTS:
    default:
        return true;
    }
}

bool build_example_win(Config config, const char *build_path)
{
    const Example *example = config.example;
    const char *example_path = nob_temp_sprintf("examples/%s", example->name);
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths c_files = {0};

    for (size_t i = 0; i < example->c_files.count; i++)
        nob_da_append(&c_files, nob_temp_sprintf("%s/%s.c", example_path, example->c_files.names[i]));

    /* build and link */
    const char *libcvr_path = nob_temp_sprintf("./build/%s/cvr/libcvr.dll", target_names[config.target]);
    const char *exec_path = nob_temp_sprintf("%s/%s", build_path, example->name);
    bool c_files_updated = nob_needs_rebuild(exec_path, example->c_files.names, example->c_files.count);
    bool libcvr_updated = nob_needs_rebuild(exec_path, &libcvr_path, 1);
    if (c_files_updated || libcvr_updated) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
        nob_cmd_append(&cmd, "-I./src");
        nob_cmd_append(&cmd, "-I./external");
        nob_cmd_append(&cmd, "-o", exec_path);
        nob_cmd_append(&cmd, nob_temp_sprintf("%s/main.c", example_path));
        const char *cvr_path = nob_temp_sprintf("-L./build/%s/cvr", target_names[config.target]);
        nob_cmd_append(&cmd, cvr_path, "-l:libcvr.dll");
        nob_cmd_append(&cmd, "-L./lib", "-l:vulkan-1.lib", "-lwinmm", "-lgdi32", "-lpthread");
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }

    /* copy cvr dll to examples folder */
    const char *dll_dst = nob_temp_sprintf("%s/libcvr.dll", build_path);
    if (!copy_file_if_not_exists(libcvr_path, dll_dst)) return false;
    
defer:
    nob_cmd_free(cmd);
    nob_cmd_free(c_files);
    return result;
}

bool build_example(const char *example_build_path, Config config)
{
    if (config.target == TARGET_LINUX) {
        return build_example_linux(config, example_build_path);
    } else if (config.target == TARGET_WINDOWS && config.host == HOST_LINUX) {
        return build_example_win(config, example_build_path);
    } else {
        if (0 <= config.target && config.target < NOB_ARRAY_LEN(target_names))
            nob_log(NOB_ERROR, "failed to build example for target %s", target_names[config.target]);
        else
            nob_log(NOB_ERROR, "failed to build example for target %d", config.target);
        return false;
    }
}

void print_examples()
{
    nob_log(NOB_INFO, "run example with: ./nob -e <example name>");
    nob_log(NOB_INFO, "Listing available examples:");
    for (size_t i = 0; i < NOB_ARRAY_LEN(examples); i++)
        nob_log(NOB_INFO, "    %s", examples[i].name);
}

bool cd(const char *dir)
{
    if (chdir(dir) != 0) {
        nob_log(NOB_ERROR, "failed to change directory to %s", dir);
        return false;
    }

    return true;
}

bool find_supported_example(Config *config)
{
    bool name_found = false;
    for (size_t i = 0; i < NOB_ARRAY_LEN(examples); i++) {
        if (strcmp(config->supplied_name, examples[i].name) == 0) {
            name_found = true;
            config->example = &examples[i];
            break;
        }
    }
    if (!name_found) {
        nob_log(NOB_ERROR, "no such example found: %s", config->supplied_name);
        return false;
    }

    return true;
}

bool copy_resources(const char *example_build_path, Config config)
{
    /* get the source and destination resource folder names */
    const char *src_res = nob_temp_sprintf("examples/%s/res", config.example->name);
    char *dst_res = NULL;
    dst_res = nob_temp_sprintf("%s/res", example_build_path);
    if (!dst_res) {
        nob_log(NOB_ERROR, "could create destination folder for resources");
        return false;
    }
    if (!nob_mkdir_if_not_exists(dst_res)) return false;

    /* copy everything over into the resources folder */
    Nob_File_Paths paths = {0};
    if (!nob_read_entire_dir(src_res, &paths)) return false;
    for (size_t i = 0; i < paths.count; i++) {
        const char *file_name = paths.items[i];
        /* note: we ignore shaders since they are copied in shader build step
         * this is because they may need to be copied again if the shader needs to be recompiled */
        if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0 ||
            strstr(file_name, ".frag")  || strstr(file_name, ".vert") ||
            strstr(file_name, ".comp"))
            continue;

        const char *src_path = nob_temp_sprintf("%s/%s", src_res, file_name);
        const char *dst_path = nob_temp_sprintf("%s/%s", dst_res, file_name);
        if (!copy_file_if_not_exists(src_path, dst_path)) return false;
    }

    return true;
}

bool run_example_linux(Config config, const char *example_build_path)
{
    bool result = true;

    Nob_Cmd cmd = {0};
    nob_log(NOB_INFO, "running example %s", config.example->name);
    if (!cd(example_build_path)) nob_return_defer(false);
    const char *bin = nob_temp_sprintf("./%s", config.example->name);
    if (config.debug) {
        // nob_cmd_append(&cmd, "gf2", "-ex", "start", bin);
        nob_cmd_append(&cmd, "gdb", "-ex", "start", bin);
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    } else if (config.renderdoc) {
        /* open renderdoc to take a capture */
        nob_cmd_append(&cmd, "renderdoccmd", "capture", "-c", "snapshot", "-w", bin);
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);

        cmd.count = 0;
        Nob_File_Paths paths = {0};
        nob_read_entire_dir(".", &paths);
        for (size_t i = 0; i < paths.count; i++) {
            const char *file_name = paths.items[i];
            if (strstr(file_name, ".rdc")) {
                /* once capture is take, automatically open it for analysis */
                nob_log(NOB_INFO, file_name);
                nob_cmd_append(&cmd, "renderdoc", file_name);
                if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);

                /* cleanup capture file */
                cmd.count = 0;
                nob_cmd_append(&cmd, "rm", file_name);
                if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
                break;
            }
        }
    } else {
        nob_cmd_append(&cmd, bin);
        for (int i = 0; i < config.forwarded_argc; i++) {
            nob_cmd_append(&cmd, config.forwarded_argv[i]);
        }
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }
    if (!cd("../../../../")) nob_return_defer(false);

defer:
    nob_cmd_free(cmd);
    return result;
}

bool run_example_win(Config config, const char *example_build_path)
{
    bool result = true;

    Nob_Cmd cmd = {0};
    nob_log(NOB_INFO, "running windows example %s", config.example->name);
    if (!cd(example_build_path)) nob_return_defer(false);
    const char *bin = nob_temp_sprintf("./%s.exe", config.example->name);
    nob_cmd_append(&cmd, "wine", bin);
    if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    if (!cd("../../../../")) nob_return_defer(false);

defer:
    nob_cmd_free(cmd);
    return result;
}

bool deploy_example_quest(Config config, const char *example_build_path)
{
    bool result = true;
    Nob_Cmd cmd = {0};

    const char *apk_path = nob_temp_sprintf("%s/%s.apk", example_build_path, config.example->name);
    nob_cmd_append(&cmd, "adb", "install", "-r", apk_path);
    if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);

    // adb shell am start -n com.wamwadstudios.blarg/android.app.NativeActivity && adb logcat

defer:
    nob_cmd_free(cmd);
    return result;
}

bool run(Config config, const char *example_build_path)
{
    if (config.host != HOST_LINUX) {
        nob_log(NOB_ERROR, "currently examples only run on linux");
        return false;
    }

    switch (config.target) {
    case TARGET_LINUX:
        if (!run_example_linux(config, example_build_path)) return false;
        break;
    case TARGET_WINDOWS:
        if (!run_example_win(config, example_build_path)) return false;
        break;
    default:
        nob_log(NOB_ERROR, "Target %s not yet supported for running or deploying", target_names[config.target]);
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Config config = {
        .argc = argc,
        .argv = argv,
    };
    if (!handle_usr_args(&config)) return 0;
    assert(config.host == HOST_LINUX && "for now the only supported host is linux");

    Nob_Cmd cmd = {0};
    if (!nob_mkdir_if_not_exists("build")) return 1;
    const char *platform_path = nob_temp_sprintf("./build/%s", target_names[config.target]);
    if (!nob_mkdir_if_not_exists(platform_path)) return 1;
    if (!build_glfw(config, platform_path)) return 1;
    if (!build_cvr(&config, platform_path)) return 1;
    if (!find_supported_example(&config)) return 1;
    const char *examples_build_path = nob_temp_sprintf("./build/%s/examples", target_names[config.target]);
    if (!nob_mkdir_if_not_exists(examples_build_path)) return 1;
    const char *example_build_path = nob_temp_sprintf("%s/%s", examples_build_path, config.example->name);
    if (!nob_mkdir_if_not_exists(example_build_path)) return 1;
    if (!build_example(example_build_path, config)) return 1;
    if (!compile_shaders(config)) return 1;
    if (!copy_resources(example_build_path, config)) return 1;
    if (!run(config, example_build_path)) return 1;

    nob_cmd_free(cmd);
    return 0;
}
