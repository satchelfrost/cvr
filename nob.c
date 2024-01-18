#include <string.h>
#define NOB_IMPLEMENTATION
#include "src/ext/nob.h"

static const char *cvr[] = {
    "cvr_core",
    "cvr_context",
    "ext_man",
    "vertex",
    "cvr_cmd",
    "cvr_buffer",
    "cvr_window",
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
        nob_da_append(&obj_files, output_path);
        if (nob_needs_rebuild(output_path, &input_path, 1)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "cc");
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

typedef struct {
    const char **names;
    size_t count;
} Shaders;

bool compile_shaders(const char *example_path, Shaders shaders)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

    nob_log(NOB_INFO, "checking shaders for %s", example_path);

    const char *output_folder = nob_temp_sprintf("./build/%s/shaders", example_path);
    if (!nob_mkdir_if_not_exists(output_folder)) nob_return_defer(false);

    const char *input_folder = nob_temp_sprintf("%s/shaders", example_path);

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

typedef struct {
    const char **names;
    size_t count;
} CFiles;

bool build_example(const char *example_path, CFiles c_files)
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths obj_files = {0};

    nob_log(NOB_INFO, "checking %s", example_path);

    const char *build_path = nob_temp_sprintf("build/%s", example_path);
    if (!nob_mkdir_if_not_exists(build_path)) return 1;

    for (size_t i = 0; i < c_files.count; i++) {
        const char *output_path = nob_temp_sprintf("build/%s/%s.o", example_path, c_files.names[i]);
        const char *input_path = nob_temp_sprintf("%s/%s.c", example_path, c_files.names[i]);
        nob_da_append(&obj_files, output_path);
        if (nob_needs_rebuild(output_path, &input_path, c_files.count)) {
            cmd.count = 0;
            nob_cmd_append(&cmd, "cc");
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
    const char *exec_path = nob_temp_sprintf("./build/%s/main", example_path);
    bool obj_updated = nob_needs_rebuild(exec_path, obj_files.items, obj_files.count);
    bool cvrlib_updated = nob_needs_rebuild(exec_path, &libcvr_path, 1);
    if (obj_updated || cvrlib_updated) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "cc");
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

static const char *default_shader_names[] = {
    "shader.vert",
    "shader.frag"
};

static const char *default_c_file_name[] = {
    "main",
};

/* Data necessary for compiling an example */
typedef struct {
    const char *name;
    const Shaders shaders;
    const CFiles c_files;
} Example;



static Example examples[] = {
    {
        .name    = "3d-primitives",
        .shaders = {
            .names = default_shader_names,
            .count = NOB_ARRAY_LEN(default_shader_names)
        },
        .c_files = {
            .names = default_c_file_name,
            .count = NOB_ARRAY_LEN(default_c_file_name) 
        }
    },
};

void log_usage(const char *program)
{
    nob_log(NOB_ERROR, "usage: %s <flags> <optional_input>", program);
    nob_log(NOB_ERROR, "    -c clean build");
    nob_log(NOB_ERROR, "    -e <example_name> optional example");
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};
    const char *program = nob_shift_args(&argc, &argv);
    char *example = NULL;
    if (argc > 0) {
        char flag;
        char *flags = nob_shift_args(&argc, &argv);
        while (flag = (++flags)[0]) {
            switch (flag) {
            case 'c':
                nob_log(NOB_INFO, "clean build requested, removing build folder");
                nob_cmd_append(&cmd, "rm", "build", "-r");
                if (!nob_cmd_run_sync(cmd)) return 1;
                break;
            case 'e':
                if (argc == 0) {
                    log_usage(program);
                    return 1;
                }
                example = nob_shift_args(&argc, &argv);
                break;
            case 'h':
                log_usage(program);
                return 1;
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
        if (!build_example(example_path, examples[i].c_files)) return 1;
        if (!compile_shaders(example_path, examples[i].shaders)) return 1;

        /* run example after building */
        nob_log(NOB_INFO, "running example %s", example);
        const char *build_path = nob_temp_sprintf("build/%s/main", example_path);
        nob_cmd_append(&cmd, build_path);
        if (!nob_cmd_run_sync(cmd)) return 1;
    } else {
        nob_log(NOB_INFO, "building all examples");
        for (size_t i = 0; i < NOB_ARRAY_LEN(examples); i++) {
            const char *example_path = nob_temp_sprintf("examples/%s", examples[i].name);
            if (!build_example(example_path, examples[i].c_files)) return 1;
            if (!compile_shaders(example_path, examples[i].shaders)) return 1;
        }
    }

    nob_cmd_free(cmd);
    return 0;
}
