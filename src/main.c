#include "cvr.h"

int main()
{
    init_window(800, 600, "Spinning Shapes");
    while (!window_should_close() && draw())
        ;
    close_window();
    return 0;
}
