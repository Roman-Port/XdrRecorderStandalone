#include "gui/views/captureview.h"
#include "gui/viewman.h"
#include "gui/display.h"
#include "gui/assets.h"
#include "recorder/recorder.h"
#include "sdman.h"
#include <stdio.h>

#define TIME_HEADER_HEIGHT 13
#define SD_FOOTER_HEIGHT 16
#define CAPACITY_CHECK_LIFETIME 30000
#define RECORDER_HEIGHT ((DISPLAY_HEIGHT - SD_FOOTER_HEIGHT) / 2)
#define RECORDER_PADDING 4

#define SECS_PER_MIN 60
#define SECS_PER_HOUR (SECS_PER_MIN * 60)

#define MAX_RECORDING_TIME ((99 * SECS_PER_HOUR) - 1)
#define MAX_DROPPED_SAMPLES 99999

#define RECORDER_VIEW_TIME 0
#define RECORDER_VIEW_SIZE 1
#define RECORDER_VIEW_LOST 2

static const char* capacity_suffixes[] = {
		"kB",
		"MB",
		"GB",
		"TB"
};

static uint32_t last_capacity_check = 0;  // Milliseconds since last capacity check. Set to 0 to force a new check
static uint64_t cached_card_free = 0;     // Stores the number of free clusters on the card
static int current_recorder_view = 0;     // The currently selected view

static void render_recorder_buffers(int x1, int y1, int x2, int y2, recorder_instance_t* data) {
	//Render rectangle around
	display_fb_draw_line_v(x1, y1+1, y2-1, 1);
	display_fb_draw_line_v(x2, y1+1, y2-1, 1);
	display_fb_draw_line_h(y1+1, x1, x2, 1);
	display_fb_draw_line_h(y2-1, x1, x2, 1);

	//Calculate
	x1++;
	x2--;
	int width = x2 - x1;
	float scaler = (float)data->setup.buffer_count / (width + 1);

	//Fill in regions
	int fillY1 = y1 + 2;
	int fillY2 = y2 - 2;
	for (int i = 0; i <= width; i++)
		display_fb_draw_line_v(x1 + i, fillY1, fillY2, data->setup.buffers[(int)(i * scaler)].state == 0xFF);

	//Fill in the current saving sample
	/*if (data->state == RECORDER_STATE_RECORDING) {
		display_fb_draw_line_v(
				x1 + (data->output_buffer_index / scaler),
				y1,
				y2,
				1
		);
	}*/

	//Fill in the current writing sample
	display_fb_draw_line_v(
			x1 + ((*data->info->current_capturing_buffer) / scaler),
			y1,
			y2,
			1
	);
}

static void create_recorder_time(char* text, recorder_instance_t* recorder) {
	//Calculate time from recorded sample rates
	uint64_t time = recorder->received_samples / recorder->info->output_sample_rate;

	//Cap so the max we can show is 9:59:59
	if (time > MAX_RECORDING_TIME)
		time = MAX_RECORDING_TIME;

	//Format
	sprintf(text, "%02i:%02i:%02i",
			(int)(time / SECS_PER_HOUR),
			(int)((time / SECS_PER_MIN) % 60),
			(int)(time % SECS_PER_MIN)
	);
}

static void create_recorder_size(char* text, recorder_instance_t* recorder) {
	//Calculate output size in bytes
	uint64_t size = recorder->received_samples * recorder->info->output_channels * (recorder->info->output_bits_per_sample / 8);

	//Find the best fitting unit
	int unit = 0;
	uint64_t divisor = 1000;
	while ((size / divisor) >= 1000) {
		unit++;
		divisor *= 1000;
	}

	//Get the integer (to 3 digits) and decimal (to 1 digits) parts
	int intPart = (int)(size / divisor);
	int decPart = (int)((size / (divisor / 10)) % 10);

	//Format
	sprintf(text, "%i.%01i %s", intPart, decPart, capacity_suffixes[unit]);
}

static void create_recorder_lost(char* text, recorder_instance_t* recorder) {
	//Format
	uint64_t dropped = recorder->setup.dropped_samples;
	if (dropped > MAX_DROPPED_SAMPLES)
		sprintf(text, "%i+ lost", MAX_DROPPED_SAMPLES);
	else
		sprintf(text, "%i lost", (int)dropped);
}

static void render_recorder_status(int x, int y, int height, recorder_instance_t* recorder) {
	//Render icon
	if (recorder->setup.dropped_samples == 0 || ((HAL_GetTick() / 1000) % 2))
		display_fb_draw_image(x, y, recorder->info->icon);
	else
		display_fb_draw_image(x, y, &icon_recorder_warn);
	x += recorder->info->icon->width + RECORDER_PADDING;

	//Prepare text
	char text[32];
	switch (current_recorder_view) {
	case RECORDER_VIEW_TIME: create_recorder_time(text, recorder); break;
	case RECORDER_VIEW_SIZE: create_recorder_size(text, recorder); break;
	case RECORDER_VIEW_LOST: create_recorder_lost(text, recorder); break;
	}

	//Render text
	display_fb_draw_text(&font_system_14, x, y, text);

	//Render buffer status
	render_recorder_buffers(
			x,
			y + 14,
			DISPLAY_WIDTH - 1,
			y + height,
			recorder
	);
}

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

	//Render icon
	int top = DISPLAY_HEIGHT - SD_FOOTER_HEIGHT + 1;
	int left = 1;
	display_fb_draw_line_h(top, 0, DISPLAY_WIDTH, 1);
	top += 2;
	display_fb_draw_image(left, top, icon);
	left += 11 + 6;

	//Render capacity if it's ready, or write error text
	if (sdman_state == SDMAN_STATE_READY) {
		//Check if we need to query the capacity
		if (last_capacity_check == 0 || HAL_GetTick() - last_capacity_check >= CAPACITY_CHECK_LIFETIME) {
			//Query free clusters
			DWORD freeClusters = 0;
			FATFS* fs = &sdman_fs;
			f_getfree(SDPath, &freeClusters, &fs);

			//Apply
			cached_card_free = freeClusters;
			last_capacity_check = HAL_GetTick();
		}

		//Calculate the rectangle that'll display the filled capacity
		int rectLeft = left;
		int rectTop = top + 2;
		int rectRight = DISPLAY_WIDTH - 2;
		int rectBottom = DISPLAY_HEIGHT - 2 - 2;

		//Render rectangle
		display_fb_draw_line_h(rectTop, rectLeft, rectRight, 1);
		display_fb_draw_line_h(rectBottom, rectLeft, rectRight, 1);
		display_fb_draw_line_v(rectLeft, rectTop, rectBottom, 1);
		display_fb_draw_line_v(rectRight, rectTop, rectBottom, 1);

		//Calculate and update states
		float available = 1 - ((float)cached_card_free / (sdman_fs.n_fatent - 2));
		int fullness = (int)(available * (rectRight - rectLeft));

		//Fill
		for (int i = 0; i < fullness; i++)
			display_fb_draw_line_v(rectLeft++, rectTop, rectBottom, 1);
	} else {
		//Render error text
		display_fb_draw_text(&font_system_14, left, top, text);

		//Reset capacity check counter
		last_capacity_check = 0;
	}
}

static void init(const viewman_view_t* view) {
	//Initialize recorders
	recorder_init();
}

static void tick(const viewman_view_t* view) {
	//Tick recorders
	recorder_tick();
}

static void render(const viewman_view_t* view, int input) {
    //Draw content
	render_recorder_status(0, 0, RECORDER_HEIGHT - RECORDER_PADDING, &recorders[0]);
	render_recorder_status(0, RECORDER_HEIGHT, RECORDER_HEIGHT - RECORDER_PADDING, &recorders[0]);

    //Draw footer
    render_sd_footer();
}

static void deinit(const viewman_view_t* view) {
	//TODO: Uninitialize recorders
}

void create_view_capture() {
	viewman_view_t view = {
			.user_ctx = 0,
			.init_cb = init,
			.tick_cb = tick,
			.process_cb = render,
			.deinit_cb = deinit
	};
	viewman_push_view(view);
}
