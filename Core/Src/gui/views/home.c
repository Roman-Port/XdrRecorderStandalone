#include "gui/views/home.h"
#include "gui/viewman.h"
#include "gui/display.h"
#include "gui/assets.h"
#include "sdman.h"

#define SD_FOOTER_HEIGHT 15

static void render_sd_footer() {
	//Determine icon and text
	const gfx_img_t* icon = &icon_sdcard;
	const char* text = "";
	switch(sdman_state) {
	case SDMAN_STATE_REMOVED:
		icon = &icon_sdcard_none;
		text = "No SD Card";
		break;
	case SDMAN_STATE_MOUNTING:
		icon = &icon_sdcard_waiting;
		text = "Mounting...";
		break;
	case SDMAN_STATE_MOUNT_ERROR:
		icon = &icon_sdcard_error;
		text = "Mount Failed";
		break;
	case SDMAN_STATE_IO_ERROR:
		icon = &icon_sdcard_error;
		text = "SD IO Error";
	}

	//Render icon and text
	int top = DISPLAY_HEIGHT - SD_FOOTER_HEIGHT - 1;
	display_fb_draw_line_h(top, 0, DISPLAY_WIDTH, 1);
	top += 2;
	display_fb_draw_image(1, top, icon);
	display_fb_draw_text(&font_system_14, 1 + 11 + 1 + 5, top, text);
}

static void render(const viewman_view_t* view, int input) {
	display_fb_draw_text(&font_system_14, 0, 0, "--:--");
    display_fb_invert_region(0, 0, DISPLAY_WIDTH, 13);
    render_sd_footer();
}

void create_view_home() {
	viewman_view_t view = {
			.user_ctx = 0,
			.process_cb = render
	};
	viewman_push_view(view);
}
