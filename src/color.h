#ifndef COLOR_H_
#define COLOR_H_

#define LIGHTGRAY  (Color){ 200, 200, 200, 255 }
#define GRAY       (Color){ 130, 130, 130, 255 }
#define DARKGRAY   (Color){ 80, 80, 80, 255 }
#define YELLOW     (Color){ 253, 249, 0, 255 }
#define GOLD       (Color){ 255, 203, 0, 255 }
#define ORANGE     (Color){ 255, 161, 0, 255 }
#define PINK       (Color){ 255, 109, 194, 255 }
#define RED        (Color){ 230, 41, 55, 255 }
#define MAROON     (Color){ 190, 33, 55, 255 }
#define GREEN      (Color){ 0, 228, 48, 255 }
#define LIME       (Color){ 0, 158, 47, 255 }
#define DARKGREEN  (Color){ 0, 117, 44, 255 }
#define SKYBLUE    (Color){ 102, 191, 255, 255 }
#define BLUE       (Color){ 0, 121, 241, 255 }
#define DARKBLUE   (Color){ 0, 82, 172, 255 }
#define PURPLE     (Color){ 200, 122, 255, 255 }
#define VIOLET     (Color){ 135, 60, 190, 255 }
#define DARKPURPLE (Color){ 112, 31, 126, 255 }
#define BEIGE      (Color){ 211, 176, 131, 255 }
#define BROWN      (Color){ 127, 106, 79, 255 }
#define DARKBROWN  (Color){ 76, 63, 47, 255 }
#define WHITE      (Color){ 255, 255, 255, 255 }
#define BLACK      (Color){ 0, 0, 0, 255 }
#define BLANK      (Color){ 0, 0, 0, 0 }
#define MAGENTA    (Color){ 255, 0, 255, 255 }

typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

#endif // COLOR_H_
