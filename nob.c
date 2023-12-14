#define NOB_IMPLEMENTATION
#include "src/nob.h"

bool build_cvr()
{
    bool result = true;
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cc");
    nob_cmd_append(&cmd, "-Werror", "-Wall", "-Wextra", "-g");
    nob_cmd_append(&cmd, "-o", "./build/cvr");
    nob_cmd_append(&cmd, "./src/main.c");
    nob_cmd_append(&cmd, "-lglfw", "-lvulkan", "-ldl", "-lpthread", "-lX11", "-lXxf86vm", "-lXrandr", "-lXi");
    if (!nob_cmd_run_sync(cmd)) nob_return_defer(false);

defer:
    nob_cmd_free(cmd);
    return result;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (!nob_mkdir_if_not_exists("build")) return 1;
    if (!build_cvr()) return 1;
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "./build/cvr");
    if (!nob_cmd_run_sync(cmd)) return 1;
    nob_cmd_free(cmd);
    return 0;
}