#include <string.h>
#define NOB_IMPLEMENTATION
#include "src/ext/nob.h"

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
    TARGET_QUEST,
    TARGET_COUNT,
} Target;

const char *target_names[TARGET_COUNT] = {
    "linux",
    "windows",
    "quest-devel", // in development but not currently supported
};

typedef enum {
    HOST_LINUX,
    HOST_WINDOWS,
} Host;

/* Data necessary for compiling an example */
typedef struct {
    const char *name;
    const Shaders shaders;
    const CFiles c_files;
    bool supported_targets[TARGET_COUNT];
} Example;

static const char *default_shader_names[] = {"default.vert", "default.frag"};
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
        .supported_targets[TARGET_QUEST] = true,
        .supported_targets[TARGET_LINUX] = true,
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
        .supported_targets[TARGET_LINUX] = true,
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
        .supported_targets[TARGET_LINUX] = true,
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
        .supported_targets[TARGET_LINUX] = true,
    },
    {
        .name = "texture",
        .shaders = {
            .names = (const char *[]) {
                "default.vert",
                "default.frag",
                "texture.vert",
                "texture.frag"
            },
            .count = 4
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
        .supported_targets[TARGET_LINUX] = true,
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
        .supported_targets[TARGET_LINUX] = true,
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
        .supported_targets[TARGET_LINUX] = true,
    },
    {
        .name = "point-cloud",
        .shaders = {
            .names = (const char *[]) {
                "point-cloud.vert",
                "point-cloud.frag",
            },
            .count = 2
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
        .supported_targets[TARGET_LINUX] = true,
    },
    {
        .name = "adv-point-cloud",
        .shaders = {
            .names = (const char *[]) {
                "default.vert",
                "default.frag",
                "point-cloud.vert",
                "point-cloud.frag",
                "texture.vert",
                "texture.frag"
            },
            .count = 6
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
        .supported_targets[TARGET_LINUX] = true,
    },
    {
        .name = "compute",
        .shaders = {
            .names = (const char *[]) {
                "default.vert",
                "default.frag",
                "default.comp",
                "particle.vert",
                "particle.frag",
            },
            .count = 5,
        },
        .c_files = {
            .names = default_c_file_names,
            .count = NOB_ARRAY_LEN(default_c_file_names)
        },
        .supported_targets[TARGET_LINUX] = true,
    },
};

typedef struct {
    int argc;
    char **argv;
    char *program;
    char *supplied_name;
    Example *example;
    Target target;
    char *target_name;
    bool using_clang;
    bool gen_comp_db;
    bool release;
    Host host;
    bool debug;
    bool renderdoc;
} Config;

void log_usage(const char *program)
{
    nob_log(NOB_INFO, "usage: %s <flags> <optional_input>", program);
    nob_log(NOB_INFO, "    -h help (log usage)");
    nob_log(NOB_INFO, "    -c clean build");
    nob_log(NOB_INFO, "    -e <example_name> optional example");
    nob_log(NOB_INFO, "    -d generate compilation database (requires clang)");
    nob_log(NOB_INFO, "    -l list available examples");
    nob_log(NOB_INFO, "    -t specify build target (linux, windows, quest)");
    nob_log(NOB_INFO, "    -g debug launch (gf2)"); // https://github.com/nakst/gf
    nob_log(NOB_INFO, "    -r renderdoc launch");
}

void print_examples();

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
            case 'd':
                nob_log(NOB_INFO, "compilation database requested (requires clang)");
                config->using_clang = config->gen_comp_db = true;
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


static const char *cvr[] = {
    "cvr",
};

bool build_cvr(Config config, const char *platform_path)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths obj_files = {0};

    nob_log(NOB_INFO, "checking cvr library");

    const char *build_path = nob_temp_sprintf("%s/cvr", platform_path);
    if (!nob_mkdir_if_not_exists(build_path)) nob_return_defer(false);

    for (size_t i = 0; i < NOB_ARRAY_LEN(cvr); i++) {
        const char *output_path = nob_temp_sprintf("%s/%s.o", build_path, cvr[i]);
        const char *input_path = nob_temp_sprintf("./src/%s.c", cvr[i]);
        const char *header_path = nob_temp_sprintf("./src/vk_ctx.h", cvr[i]);
        const char *comp_db = nob_temp_sprintf("build/compilation_database/%s.o.json", cvr[i]);
        nob_da_append(&obj_files, output_path);
        if (nob_needs_rebuild(output_path, &input_path, 1) ||
            nob_needs_rebuild(output_path, &header_path, 1)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, (config.using_clang) ? "clang" : "cc");
            if (config.gen_comp_db && config.using_clang) nob_cmd_append(&cmd, "-MJ", comp_db);
            nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
            nob_cmd_append(&cmd, "-DENABLE_VALIDATION");
            nob_cmd_append(&cmd, "-c", input_path);
            nob_cmd_append(&cmd, "-o", output_path);
            Nob_Proc proc = nob_cmd_run_async(cmd);
            nob_da_append(&procs, proc);
        }
    }

    if (!nob_procs_wait(procs)) nob_return_defer(false);

    const char *libcvr_path = nob_temp_sprintf("%s/libcvr.a", build_path);
    if (nob_needs_rebuild(libcvr_path, obj_files.items, obj_files.count)) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "ar", "-crs", libcvr_path);
        for (size_t i = 0; i < obj_files.count; i++) {
            const char *input_path = nob_temp_sprintf("%s/%s.o", build_path, cvr[i]);
            nob_cmd_append(&cmd, input_path);
        }
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }

defer:
    nob_cmd_free(cmd);
    nob_cmd_free(procs);
    nob_cmd_free(obj_files);
    return result;
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
    const char *output_folder = nob_temp_sprintf("./build/%s/%s/res", target_names[config.target], example_path);
    if (!nob_mkdir_if_not_exists(output_folder)) nob_return_defer(false);
    const char *input_folder = nob_temp_sprintf("%s/res", example_path);

    for (size_t i = 0; i < shaders.count; i++) {
        const char *output_path = nob_temp_sprintf("%s/%s.spv", output_folder, shaders.names[i]);
        const char *input_path = nob_temp_sprintf("%s/%s", input_folder, shaders.names[i]);

        if (nob_needs_rebuild(output_path, &input_path, 1)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "glslc", input_path, "-o", output_path);
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

bool build_example(const char *build_path, Config config)
{
    const char **c_files = config.example->c_files.names;
    const Example *example = config.example;
    const char *example_path = nob_temp_sprintf("examples/%s", example->name);
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths obj_files = {0};

    nob_log(NOB_INFO, "checking %s", example_path);

    for (size_t i = 0; i < example->c_files.count; i++) {
        const char *output_path = nob_temp_sprintf("%s/%s.o", build_path, c_files[i]);
        const char *input_path = nob_temp_sprintf("%s/%s.c", example_path, c_files[i]);
        const char *comp_db = nob_temp_sprintf("build/compilation_database/%s.o.json", example->name);
        nob_da_append(&obj_files, output_path);
        if (nob_needs_rebuild(output_path, &input_path, example->c_files.count)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, (config.using_clang) ? "clang" : "cc");
            if (config.gen_comp_db && config.using_clang)
                nob_cmd_append(&cmd, "-MJ", comp_db);
            nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
            nob_cmd_append(&cmd, "-I./src");
            nob_cmd_append(&cmd, "-c", input_path);
            nob_cmd_append(&cmd, "-o", output_path);
            Nob_Proc proc = nob_cmd_run_async(cmd);
            nob_da_append(&procs, proc);
        }
    }

    if (!nob_procs_wait(procs)) nob_return_defer(false);

    const char *libcvr_path = nob_temp_sprintf("./build/%s/cvr/libcvr.a", target_names[config.target]);
    const char *exec_path = nob_temp_sprintf("%s/%s", build_path, example->name);
    bool obj_updated = nob_needs_rebuild(exec_path, obj_files.items, obj_files.count);
    bool cvrlib_updated = nob_needs_rebuild(exec_path, &libcvr_path, 1);
    if (obj_updated || cvrlib_updated) {
        cmd.count = 0;
        nob_cmd_append(&cmd, (config.using_clang) ? "clang" : "cc");
        nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
        nob_cmd_append(&cmd, "-I./src");
        nob_cmd_append(&cmd, "-o", exec_path);
        for (size_t i = 0; i < obj_files.count; i++) {
            const char *input_path = nob_temp_sprintf("%s", obj_files.items[i]);
            nob_cmd_append(&cmd, input_path);
        }
        const char *cvr_path = nob_temp_sprintf("-L./build/%s/cvr", target_names[config.target]);
        nob_cmd_append(&cmd, cvr_path, "-l:libcvr.a");
        nob_cmd_append(&cmd, "-lglfw", "-lvulkan", "-ldl", "-lpthread", "-lX11", "-lXxf86vm", "-lXrandr", "-lXi", "-lm");
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }
    
defer:
    nob_cmd_free(cmd);
    nob_cmd_free(procs);
    nob_cmd_free(obj_files);
    return result;
}

void print_examples()
{
    nob_log(NOB_INFO, "run example with: ./nob -e <example name>");
    nob_log(NOB_INFO, "Listing available examples:");
    for (size_t i = 0; i < NOB_ARRAY_LEN(examples); i++) {
        char *target_list = "";
        for (size_t j = 0; j < TARGET_COUNT; j++) {
            if (examples[i].supported_targets[j]) {
                if (j == 0)
                    target_list = nob_temp_sprintf("%s", target_names[j]);
                else
                    target_list = nob_temp_sprintf("%s, %s", target_list, target_names[j]);
            }

        }
        nob_log(NOB_INFO, "    %s (%s)", examples[i].name, target_list);
    }
}

bool create_comp_db()
{
    bool result = true;
    Nob_File_Paths paths = {0};
    nob_read_entire_dir("./build/compilation_database", &paths);
    Nob_String_Builder sb = {0};
    nob_sb_append_cstr(&sb, "[\n");
    for (size_t i = 0; i < paths.count; i++) {
        if (strstr(paths.items[i], ".json") != 0) {
            const char *path = nob_temp_sprintf("./build/compilation_database/%s", paths.items[i]);
            if (!nob_read_entire_file(path, &sb)) nob_return_defer(false);
        }
    }
    sb.count -= 2; // erase last two chars from file to be json compliant for newly concatenated file
    nob_sb_append_cstr(&sb, "\n]");
    nob_sb_append_null(&sb);
    if (!nob_write_entire_file("./build/compile_commands.json", sb.items, sb.count)) nob_return_defer(false);

    nob_log(NOB_INFO, "created compilation database, you may need to restart your LSP");

defer:
    return result;
}

bool cd(const char *dir)
{
    bool result = true;

    if (chdir(dir) != 0) {
        nob_log(NOB_ERROR, "failed to change directory to %s", dir);
        nob_return_defer(false);
    }

defer:
    return result;
}

bool find_supported_example(Config *config)
{
    bool result = true;

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
        nob_return_defer(false);
    }
    if (!config->example->supported_targets[config->target]) {
        nob_log(NOB_ERROR, "target %s not supported for example %s", target_names[config->target], config->example->name);
        nob_log(NOB_INFO, "try listing examples `./nob -l`");
        nob_return_defer(false);
    }

defer:
    return result;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Config config = {
        .argc = argc,
        .argv = argv,
    };
    if (!handle_usr_args(&config)) return 1;

    /* compilation database */
    Nob_Cmd cmd = {0};
    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (config.gen_comp_db) {
        if (!nob_mkdir_if_not_exists("build/compilation_database")) return 1;
    } else {
        // test if compilation database was previously used to ensure clang
        Nob_File_Paths paths = {0};
        if (!nob_read_entire_dir("build", &paths)) return 1;
        for (size_t i = 0; i < paths.count; i++) {
            if (strstr(paths.items[i], "compile_commands.json") != 0) {
                config.using_clang = true;
            }
        }
    }

    const char *platform_path = nob_temp_sprintf("./build/%s", target_names[config.target]);
    if (!nob_mkdir_if_not_exists(platform_path)) return 1;
    if (!build_cvr(config, platform_path)) return 1;
    if (!find_supported_example(&config)) return 1;
    const char *examples_build_path = nob_temp_sprintf("./build/%s/examples", target_names[config.target]);
    if (!nob_mkdir_if_not_exists(examples_build_path)) return 1;
    const char *example_build_path = nob_temp_sprintf("%s/%s", examples_build_path, config.example->name);
    if (!nob_mkdir_if_not_exists(example_build_path)) return 1;
    if (!build_example(example_build_path, config)) return 1;
    if (!compile_shaders(config)) return 1;

    /* copy resources over if the files do not exist */
    const char *example_res = nob_temp_sprintf("examples/%s/res", config.example->name);
    const char *build_res   = nob_temp_sprintf("%s/res", example_build_path);
    nob_mkdir_if_not_exists(build_res);
    Nob_File_Paths paths = {0};
    nob_read_entire_dir(example_res, &paths);
    for (size_t i = 0; i < paths.count; i++) {
        const char *file_name = paths.items[i];
        if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0 ||
            strstr(file_name, ".frag")  || strstr(file_name, ".vert") ||
            strstr(file_name, ".comp"))
            continue;

        const char *src_path = nob_temp_sprintf("%s/%s", example_res, file_name);
        const char *dst_path = nob_temp_sprintf("%s/%s", build_res, file_name);

        int ret = nob_file_exists(dst_path);
        if (ret == FILE_DOES_NOT_EXIST) {
            if (!nob_copy_file(src_path, dst_path))
                return 1;
        } else if (ret == FILE_CHK_ERR) {
            return 1;
        }
    }

    /* run example after building */
    if (config.host == HOST_LINUX && config.target == TARGET_LINUX) {
        cmd.count = 0;
        nob_log(NOB_INFO, "running example %s", config.example->name);
        if (!cd(example_build_path)) return 1;
        const char *bin = nob_temp_sprintf("./%s", config.example->name);
        if (config.debug) {
            nob_cmd_append(&cmd, "gf2", "-ex", "start", bin);
            if (!nob_cmd_run_sync(cmd)) return 1;
        } else if (config.renderdoc) {
            nob_cmd_append(&cmd, "renderdoccmd", "capture", "-c", "snapshot", "-w", bin);
            if (!nob_cmd_run_sync(cmd)) return 1;

            cmd.count = 0;
            Nob_File_Paths paths = {0};
            nob_read_entire_dir(".", &paths);
            for (size_t i = 0; i < paths.count; i++) {
                const char *file_name = paths.items[i];
                if (strstr(file_name, ".rdc")) {
                    nob_log(NOB_INFO, file_name);
                    nob_cmd_append(&cmd, "renderdoc", file_name);
                    if (!nob_cmd_run_sync(cmd)) return 1;
                    break;
                }
            }
        } else {
            nob_cmd_append(&cmd, bin);
            if (!nob_cmd_run_sync(cmd)) return 1;
        }
        if (!cd("../../../../")) return 1;
    }

    if (config.gen_comp_db) {
        if (!create_comp_db()) return 1;
    }

    nob_cmd_free(cmd);
    return 0;
}
