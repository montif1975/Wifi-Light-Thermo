#include "stdio.h"
#include "pico/stdlib.h"
#include "include/rgb.h"

// Each color is {R, G, B} with 1 = ON, 0 = OFF
const bool rgb_colors[][3] = {
    {1, 0, 0}, // Red
    {0, 1, 0}, // Green
    {0, 0, 1}, // Blue
    {1, 1, 0}, // Yellow
    {1, 0, 1}, // Magenta
    {0, 1, 1}, // Cyan
    {1, 1, 1}, // White
    {0, 0, 0}  // Off
};

const int num_colors = sizeof(rgb_colors) / sizeof(rgb_colors[0]);

/*
* Function: rgb_init()
 * Description: This function initializes the RGB LED GPIO pins.
 */
void rgb_init(void) 
{
    // Initialize GPIO pins for RGB
    gpio_init(GPIO_RGB_RED);
    gpio_set_dir(GPIO_RGB_RED, GPIO_OUT);

    gpio_init(GPIO_RGB_GREEN);
    gpio_set_dir(GPIO_RGB_GREEN, GPIO_OUT);

    gpio_init(GPIO_RGB_BLUE);
    gpio_set_dir(GPIO_RGB_BLUE, GPIO_OUT);

    return;
}

/*
* Function: rgb_set_color()
 * Description: This function sets the RGB LED color based on the provided R, G, B values.
 * Parameters:
 *   - r: Red component (1 = ON, 0 = OFF)
 *   - g: Green component (1 = ON, 0 = OFF)
 *   - b: Blue component (1 = ON, 0 = OFF)
 */
void rgb_set_color(bool r, bool g, bool b)
{
    gpio_put(GPIO_RGB_RED, r);
    gpio_put(GPIO_RGB_GREEN, g);
    gpio_put(GPIO_RGB_BLUE, b);
    
    return;
}

/*
* Function: rgb_set_led_color()
 * Description: This function sets the RGB LED color based on the index provided.
 * Parameters:
 *   - index: Index of the color to set (0 = Red, 1 = Green, ..., 7 = Off)
 * It does not return any value.
*/
void rgb_set_led_color(int index)
{
    if (index < 0 || index >= num_colors) {
        printf("Invalid RGB color index: %d\n", index);
        return;
    }

    // Set the RGB pins according to the selected color
    rgb_set_color(rgb_colors[index][0], rgb_colors[index][1], rgb_colors[index][2]);

    return;
}


