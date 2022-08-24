#ifndef INC_GUI_DISPLAY_H_
#define INC_GUI_DISPLAY_H_

#include <stdint.h>
#include "defines.h"

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define TEXTBOX_ALIGN_H_LEFT   (1 << 0)
#define TEXTBOX_ALIGN_H_CENTER (1 << 1)
#define TEXTBOX_ALIGN_H_RIGHT  (1 << 2)
#define TEXTBOX_ALIGN_V_TOP    (1 << 3)
#define TEXTBOX_ALIGN_V_CENTER (1 << 4)
#define TEXTBOX_ALIGN_V_BOTTOM (1 << 5)

extern uint64_t display_framebuffer[DISPLAY_WIDTH];

// Initializes the display
void display_init();

// Pushes the frame buffer to the display
void display_update();
void display_update_async();

// Clear framebuffer
void display_fb_clear();

// Sets a pixel in the frame buffer
void display_fb_set_pixel(int x, int y, int color);

// Inverts a region specified
void display_fb_invert_region(int x, int y, int width, int height);

// Draws an image to the frame buffer
void display_fb_draw(int x, int y, int width, int height, const uint64_t* data);
void display_fb_draw_image(int xOffset, int yOffset, const gfx_img_t* img);

// Draws a simple line horizontally
void display_fb_draw_line_h(int y, int x1, int x2, int color);

// Draws a simple line vertically
void display_fb_draw_line_v(int x, int y1, int y2, int color);

// Draws text to the frame buffer
void display_fb_draw_text(const gfx_font_t* font, int x, int y, const char* text);
void display_fb_draw_textbox(const gfx_font_t* font, int x, int y, int width, int height, int flags, const char* text);

#endif /* INC_GUI_DISPLAY_H_ */
