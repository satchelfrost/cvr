#ifndef NOB_EXT_H_
#define NOB_EXT_H_

/* This file contains definitions for unofficial nob, or nob extensions */

#define nob_da_resize(da, new_size)                                                    \
    do {                                                                               \
        (da)->capacity = (da)->count = new_size;                                       \
        (da)->items = NOB_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items)); \
    } while (0)

#endif // NOB_EXT_H_