#ifndef RGB_H
#define RGB_H

#define GPIO_RGB_RED        4
#define GPIO_RGB_GREEN      3
#define GPIO_RGB_BLUE       5

enum {
    RGB_COLOR_RED,
    RGB_COLOR_GREEN,
    RGB_COLOR_BLUE,
    RGB_COLOR_YELLOW,
    RGB_COLOR_MAGENTA,
    RGB_COLOR_CYAN,
    RGB_COLOR_WHITE,
    RGB_COLOR_OFF,
    RGB_COLOR_MAX
};

#define RGB_BLINK_OPT        0x80 // Bit 7 - Blink option: 0 = no blink, 1 = blink

void rgb_init(void);
void rgb_set_color(bool r, bool g, bool b);
void rgb_set_led_color(int index);

#endif // RGB_H
