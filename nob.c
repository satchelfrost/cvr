#define NOB_IMPLEMENTATION
#include "src/ext/nob.h"

const char *cvr[] = {
    "main",
    "app",
    "app_utils",
    "ext_man",
    "vertex",
    "cvr_cmd",
    "cvr_buffer",
};

bool build_cvr()
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};
    Nob_File_Paths obj_files = {0};

    for (size_t i = 0; i < NOB_ARRAY_LEN(cvr); i++) {
        const char *output_path = nob_temp_sprintf("./build/%s.o", cvr[i]);
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

    const char *exec_path = nob_temp_sprintf("./build/cvr");
    if (nob_needs_rebuild(exec_path, obj_files.items, obj_files.count)) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "cc");
        nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
        nob_cmd_append(&cmd, "-DENABLE_VALIDATION");
        for (size_t i = 0; i < obj_files.count; i++) {
            const char *input_path = nob_temp_sprintf("./build/%s.o", cvr[i]);
            nob_cmd_append(&cmd, input_path);
        }
        nob_cmd_append(&cmd, "-o", exec_path);
        nob_cmd_append(&cmd, "-lglfw", "-lvulkan", "-ldl", "-lpthread", "-lX11", "-lXxf86vm", "-lXrandr", "-lXi", "-lm");
        if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);
    }

defer:
    nob_cmd_free(cmd);
    nob_cmd_free(procs);
    nob_cmd_free(obj_files);
    return result;
}

const char *shaders[] = {
    "shader.frag",
    "shader.vert"
};

bool compile_shaders()
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

    if (!nob_mkdir_if_not_exists("build/shaders")) nob_return_defer(false);

    for (size_t i = 0; i < NOB_ARRAY_LEN(shaders); i++) {
        const char *output_path = nob_temp_sprintf("./build/shaders/%s.spv", shaders[i]);
        const char *input_path = nob_temp_sprintf("./src/shaders/%s", shaders[i]);

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

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (!compile_shaders()) return 1;
    if (!build_cvr()) return 1;
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "./build/cvr");
    if (!nob_cmd_run_sync(cmd)) return 1;
    nob_cmd_free(cmd);
    return 0;
}
