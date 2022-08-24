#include "gui/display.h"
#include "main.h"
#include <string.h>

uint64_t display_framebuffer[DISPLAY_WIDTH];

void display_fb_clear() {
	for (int i = 0; i < DISPLAY_WIDTH; i++)
		display_framebuffer[i] = 0;
}

void display_fb_set_pixel(int x, int y, int color) {
	if (x >= 0 && x < DISPLAY_WIDTH) {
		if (color)
			display_framebuffer[x] |= 1ULL << y;
		else
			display_framebuffer[x] &= ~(1ULL << y);
	}
}

void display_fb_invert_region(int xOffset, int yOffset, int width, int height) {
	if (xOffset < 0)
		return;
	uint64_t mask = ((1ULL << (uint64_t)height) - 1ULL) << (uint64_t)yOffset;
	for (int x = 0; x < width && x + xOffset < DISPLAY_WIDTH; x++) {
		display_framebuffer[x + xOffset] =
				(display_framebuffer[x + xOffset] & (~mask)) | /* keep other unaffected bits */
				((~display_framebuffer[x + xOffset]) & mask);  /* flip bits in mask */
	}
}

void display_fb_draw_line_h(int y, int x1, int x2, int color) {
	int start = MIN(x1, x2);
	int end = MAX(x1, x2);
	uint64_t mask = 1;
	mask <<= y;
	for (int i = start; i < end; i++) {
		if (i >= 0 && i < DISPLAY_WIDTH) {
			if (color) {
				display_framebuffer[i] |= mask;
			} else {
				display_framebuffer[i] &= ~mask;
			}
		}
	}
}

void display_fb_draw_line_v(int x, int y1, int y2, int color) {
	int start = MIN(y1, y2);
	int end = MAX(y1, y2);
	uint64_t mask = (1ULL << (end - start + 1)) - 1;
	mask <<= start;
	if (x >= 0 && x < DISPLAY_WIDTH) {
		if (color) {
			display_framebuffer[x] |= mask;
		} else {
			display_framebuffer[x] &= ~mask;
		}
	}
}

void display_fb_draw(int xOffset, int yOffset, int width, int height, const uint64_t* data) {
	if (xOffset < 0)
		return;
	uint64_t mask = (1ULL << height) - 1;
	mask <<= yOffset;
	mask = ~mask;
	for (int x = 0; x < width && x + xOffset < DISPLAY_WIDTH; x++) {
		display_framebuffer[x + xOffset] &= mask;
		display_framebuffer[x + xOffset] |= data[x] << yOffset;
	}
}

void display_fb_draw_image(int xOffset, int yOffset, const gfx_img_t* img) {
	display_fb_draw(xOffset, yOffset, img->width, img->height, img->data);
}

void display_fb_draw_text(const gfx_font_t* font, int x, int y, const char* text) {
	for (int i = 0; text[i] != 0; i++) {
		display_fb_draw_image(x, y, font->glyph_data[(int)text[i]]);
		x += font->glyph_width;
	}
}

void display_fb_draw_textbox(const gfx_font_t* font, int xOffset, int yOffset, int width, int height, int flags, const char* text) {
	//Get text size
	int textSizeX = strlen(text) * font->glyph_width;
	int textSizeY = font->glyph_height;

	//Determine X position
	int x = 0;
	if (flags & TEXTBOX_ALIGN_H_CENTER)
		x = (width - textSizeX) / 2;
	if (flags & TEXTBOX_ALIGN_H_RIGHT)
		x = width - textSizeX;

	//Determine Y position
	int y = 0;
	if (flags & TEXTBOX_ALIGN_V_CENTER)
		y = (height - textSizeY) / 2;
	if (flags & TEXTBOX_ALIGN_V_BOTTOM)
		y = height - textSizeY;

	//Draw
	display_fb_draw_text(font, xOffset + x, yOffset + y, text);
}
