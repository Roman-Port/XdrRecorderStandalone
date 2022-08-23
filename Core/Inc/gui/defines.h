#ifndef INC_GUI_DEFINES_H_
#define INC_GUI_DEFINES_H_

#include <stdint.h>

typedef struct {

	const uint64_t* data;
	int width;
	int height;

} gfx_img_t;

typedef struct {

	int glyph_width;
	int glyph_height;
	int padding_x;
	int padding_y;
	const gfx_img_t* glyph_data[256];

} gfx_font_t;

#endif /* INC_GUI_DEFINES_H_ */
