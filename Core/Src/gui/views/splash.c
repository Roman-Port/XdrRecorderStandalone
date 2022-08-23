#include "gui/views/splash.h"
#include "gui/viewman.h"
#include "gui/display.h"
#include "gui/assets.h"
#include "main.h"

static void render(const viewman_view_t* view, int input) {
	display_fb_draw_text(&font_system_14, 0, DISPLAY_HEIGHT - 14, RECORDER_FW_VER);
}

void create_view_splash() {
	viewman_view_t view = {
			.user_ctx = 0,
			.process_cb = render
	};
	viewman_push_view(view);
}
