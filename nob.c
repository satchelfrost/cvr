#include <string.h>
#define NOB_IMPLEMENTATION
#include "src/ext/nob.h"

/* Whether or not to use the compilation database available with clang */
bool gen_comp_db = false;
bool using_clang = false;

typedef struct {
    const char **names;
    size_t count;
} CFiles;

typedef struct {
    const char **names;
    size_t count;
} Shaders;

/* Data necessary for compiling an example */
typedef struct {
    const char *name;
    const Shaders shaders;
    const CFiles c_files;
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
        }
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
        }
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
        }
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
        }
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
        }
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
        }
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
        }
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
        }
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
        }
    },
};

static const char *cvr[] = {
    "cvr",
};

bool build_cvr()
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths obj_files = {0};

    nob_log(NOB_INFO, "checking cvr library");

    const char *build_path = nob_temp_sprintf("./build/cvr");
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
            nob_cmd_append(&cmd, (using_clang) ? "clang" : "cc");
            if (gen_comp_db && using_clang) nob_cmd_append(&cmd, "-MJ", comp_db);
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

bool compile_shaders(const char *example_path, Shaders shaders)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

    nob_log(NOB_INFO, "checking shaders for %s", example_path);

    const char *output_folder = nob_temp_sprintf("./build/%s/res", example_path);
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

bool build_example(Example *example)
{
    const char *example_path = nob_temp_sprintf("examples/%s", example->name);
    const char **c_files = example->c_files.names;
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths obj_files = {0};

    nob_log(NOB_INFO, "checking %s", example_path);

    const char *build_path = nob_temp_sprintf("build/%s", example_path);
    if (!nob_mkdir_if_not_exists(build_path)) return 1;

    for (size_t i = 0; i < example->c_files.count; i++) {
        const char *output_path = nob_temp_sprintf("build/%s/%s.o", example_path, c_files[i]);
        const char *input_path = nob_temp_sprintf("%s/%s.c", example_path, c_files[i]);
        const char *comp_db = nob_temp_sprintf("build/compilation_database/%s.o.json", example->name);
        nob_da_append(&obj_files, output_path);
        if (nob_needs_rebuild(output_path, &input_path, example->c_files.count)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, (using_clang) ? "clang" : "cc");
            if (gen_comp_db && using_clang) nob_cmd_append(&cmd, "-MJ", comp_db);
            nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
            nob_cmd_append(&cmd, "-I./src");
            nob_cmd_append(&cmd, "-c", input_path);
            nob_cmd_append(&cmd, "-o", output_path);
            Nob_Proc proc = nob_cmd_run_async(cmd);
            nob_da_append(&procs, proc);
        }
    }

    if (!nob_procs_wait(procs)) nob_return_defer(false);

    const char *libcvr_path = nob_temp_sprintf("./build/cvr/libcvr.a");
    const char *exec_path = nob_temp_sprintf("%s/%s", build_path, example->name);
    bool obj_updated = nob_needs_rebuild(exec_path, obj_files.items, obj_files.count);
    bool cvrlib_updated = nob_needs_rebuild(exec_path, &libcvr_path, 1);
    if (obj_updated || cvrlib_updated) {
        cmd.count = 0;
        nob_cmd_append(&cmd, (using_clang) ? "clang" : "cc");
        nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
        nob_cmd_append(&cmd, "-I./src");
        nob_cmd_append(&cmd, "-o", exec_path);
        for (size_t i = 0; i < obj_files.count; i++) {
            const char *input_path = nob_temp_sprintf("%s", obj_files.items[i]);
            nob_cmd_append(&cmd, input_path);
        }
        nob_cmd_append(&cmd, "-L./build/cvr", "-l:libcvr.a");
        nob_cmd_append(&cmd, "-lglfw", "-lvulkan", "-ldl", "-lpthread", "-lX11", "-lXxf86vm", "-lXrandr", "-lXi", "-lm");
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }
    
defer:
    nob_cmd_free(cmd);
    nob_cmd_free(procs);
    nob_cmd_free(obj_files);
    return result;
}

void log_usage(const char *program)
{
    nob_log(NOB_INFO, "usage: %s <flags> <optional_input>", program);
    nob_log(NOB_INFO, "    -h help (log usage)");
    nob_log(NOB_INFO, "    -c clean build");
    nob_log(NOB_INFO, "    -e <example_name> optional example");
    nob_log(NOB_INFO, "    -d generate compilation database (requires clang)");
    nob_log(NOB_INFO, "    -l list available examples");
    nob_log(NOB_INFO, "    -k disable copying res folder for speed");
}

void print_examples()
{
    nob_log(NOB_INFO, "run example with: ./nob -e <example name>");
    nob_log(NOB_INFO, "Listing available examples:");
    for (size_t i = 0; i < NOB_ARRAY_LEN(examples); i++)
        nob_log(NOB_INFO, "    %s", examples[i].name);
}

bool create_comp_db()
{
    bool result = true;
    Nob_File_Paths paths = {0};
    nob_read_entire_dir("build/compilation_database", &paths);
    Nob_String_Builder sb = {0};
    nob_sb_append_cstr(&sb, "[\n");
    for (size_t i = 0; i < paths.count; i++) {
        if (strstr(paths.items[i], ".json") != 0) {
            const char *path = nob_temp_sprintf("build/compilation_database/%s", paths.items[i]);
            if (!nob_read_entire_file(path, &sb)) nob_return_defer(false);
        }
    }
    sb.count -= 2; // erase last two chars from file to be json compliant for newly concatenated file
    nob_sb_append_cstr(&sb, "\n]");
    nob_sb_append_null(&sb);
    if (!nob_write_entire_file("build/compile_commands.json", sb.items, sb.count)) nob_return_defer(false);

    nob_log(NOB_INFO, "created compilation database, you may need to restart your LSP");

defer:
    return result;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};
    const char *program = nob_shift_args(&argc, &argv);
    bool copy = true; // default is to copy res directory, -k to disable
    char *example = NULL;
    while (argc > 0) {
        char flag;
        char *flags = nob_shift_args(&argc, &argv);
        while ((flag = (++flags)[0])) { // ignores '-'
            switch (flag) {
            case 'c':
                nob_log(NOB_INFO, "clean build requested, removing build folder");
                nob_cmd_append(&cmd, "rm", "build", "res", "-rf");
                if (!nob_cmd_run_sync(cmd)) return 1;
                break;
            case 'e':
                if (argc == 0) {
                    log_usage(program);
                    return 1;
                }
                example = nob_shift_args(&argc, &argv);
                break;
            case 'd':
                nob_log(NOB_INFO, "compilation database requested (requires clang)");
                using_clang = gen_comp_db = true;
                break;
            case 'h':
                log_usage(program);
                return 0;
                break;
            case 'l':
                print_examples();
                return 0;
                break;
            case 'k':
                copy = false;
                break;
            default:
                nob_log(NOB_ERROR, "unrecognized flag %c", flag);
                log_usage(program);
                return 1;
            }
        }
    }

    cmd.count = 0;
    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (gen_comp_db) {
        if (!nob_mkdir_if_not_exists("build/compilation_database")) return 1;
    } else {
        // test if compilation database was previously used to ensure clang
        Nob_File_Paths paths = {0};
        if (!nob_read_entire_dir("build", &paths)) return 1;
        for (size_t i = 0; i < paths.count; i++) {
            if (strstr(paths.items[i], "compile_commands.json") != 0) {
                using_clang = true;
            }
        }
    }
    if (!build_cvr()) return 1;

    if (!nob_mkdir_if_not_exists("build/examples")) return 1;
    if (example) {
        size_t i;
        bool found = false;
        for (i = 0; i < NOB_ARRAY_LEN(examples); i++) {
            if (strcmp(example, examples[i].name) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            nob_log(NOB_ERROR, "no such example found: %s", example);
            return 1;
        }
        const char *example_path = nob_temp_sprintf("examples/%s", examples[i].name);
        const char *example_res  = nob_temp_sprintf("%s/res", example_path);
        const char *build_res    = nob_temp_sprintf("./build/%s/res", example_path);
        if (!build_example(&examples[i])) return 1;
        if (!compile_shaders(example_path, examples[i].shaders)) return 1;

        /* copy res folder to root so example can be run from root */
        if (copy) {
            Nob_File_Paths paths = {0};
            nob_read_entire_dir(example_res, &paths);
            for (size_t i = 0; i < paths.count; i++) {
                const char *file_name = paths.items[i];
                if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0||
                        strstr(file_name, ".frag")  || strstr(file_name, ".vert"))
                    continue;

                const char *src_path = nob_temp_sprintf("%s/%s", example_res, file_name);
                const char *dst_path = nob_temp_sprintf("%s/%s", build_res, file_name);
                if (!nob_copy_file(src_path, dst_path)) return 1;
            }

            nob_mkdir_if_not_exists("res");
            if (!nob_copy_directory_recursively(build_res, "res")) return 1;
        }

        /* run example after building */
        cmd.count = 0;
        nob_log(NOB_INFO, "running example %s", example);
        const char *build_path = nob_temp_sprintf("build/%s/%s", example_path, examples[i].name);
        nob_cmd_append(&cmd, build_path);
        if (!nob_cmd_run_sync(cmd)) return 1;
    } else {
        nob_log(NOB_INFO, "building all examples");
        for (size_t i = 0; i < NOB_ARRAY_LEN(examples); i++) {
            const char *example_path = nob_temp_sprintf("examples/%s", examples[i].name);
            if (!build_example(&examples[i])) return 1;
            if (!compile_shaders(example_path, examples[i].shaders)) return 1;
        }
    }

    if (gen_comp_db) {
        if (!create_comp_db()) return 1;
    }

    nob_cmd_free(cmd);
    return 0;
}
