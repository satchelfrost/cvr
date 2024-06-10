#ifndef NOB_EXT_H_
#define NOB_EXT_H_

// TODO: nob da resize shouldn't set the count, that should be done manually
/* This file contains definitions for unofficial nob, or nob extensions */

#define nob_da_resize(da, new_size)                                                    \
    do {                                                                               \
        (da)->capacity = (da)->count = new_size;                                       \
        (da)->items = NOB_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items)); \
    } while (0)

/* similar to nob_da_free but also resets the pointer to NULL avoiding errors when reusing dynamic array */
#define nob_da_reset(da) \
    do {                 \
        nob_da_free(da); \
        da.items = NULL; \
    } while (0)

#endif // NOB_EXT_H_
