#include "cvr.h"

int main()
{
    init_window(800, 600, "mixed raster");
    
    while (!window_should_close()) {
        begin_drawing(BLUE);
        end_drawing();
    }

    close_window();

    return 0;
}
