#include "gui/viewman.h"
#include "gui/display.h"
#include "gui/assets.h"
#include "main.h"
#include <stdlib.h>
#include <string.h>

#define VIEW_STACK_SIZE 4
#define TARGET_FPS 10
#define ALERT_PADDING 4

static uint32_t next_frame_time = 0;
static viewman_view_t view_stack[VIEW_STACK_SIZE];
static int view_stack_count = 0;
static int view_stack_changed = 0;

static const gfx_img_t* alert_img = 0;
static char alert_text[21];

static void render_alert() {
	//Apply dithering pattern to display
	for (int i = 0; i < DISPLAY_WIDTH; i++)
		display_framebuffer[i] &= 0x5555555555555555 << (i % 2);

	//Determine dimensions
	int height = ALERT_PADDING + alert_img->height + ALERT_PADDING;
	int top = DISPLAY_HEIGHT - height;
	int contentLeft = ALERT_PADDING + alert_img->width + ALERT_PADDING;
	int contentTop = top + ALERT_PADDING;

	//Clear out this region
	uint64_t clearMask = (1ULL << top) - 1;
	for (int i = 0; i < DISPLAY_WIDTH; i++)
		display_framebuffer[i] &= clearMask;

	//Draw icon and text
	display_fb_draw_image(ALERT_PADDING, contentTop, alert_img);
	display_fb_draw_textbox(&font_system_14, contentLeft, contentTop, DISPLAY_WIDTH - contentLeft - ALERT_PADDING, alert_img->height, TEXTBOX_ALIGN_H_LEFT | TEXTBOX_ALIGN_V_CENTER, alert_text);

	//Finally, invert this region
	display_fb_invert_region(0, top, DISPLAY_WIDTH, height);
}

// Renders a frame
void viewman_tick() {
	//Tick topmost view
	if (view_stack_count > 0 && view_stack[view_stack_count - 1].tick_cb != 0)
		view_stack[view_stack_count - 1].tick_cb(&view_stack[view_stack_count - 1]);

	//Check if we need to process a frame
	if (view_stack_changed || HAL_GetTick() >= next_frame_time) {
		//Calculate next frame time
		next_frame_time = HAL_GetTick() + (1000 / TARGET_FPS);

		//Get input (eventually)
		int input = 0;

		//Check if we should clear alert
		if (alert_img != 0 && input != 0) {
			alert_img = 0;
			input = 0;
		}

		//Keep drawing the topmost view until either there is none or it doesn't change
		view_stack_changed = 0;
		do {
			//Clear frame buffer
			display_fb_clear();

			//Process
			if (view_stack_count != 0) {
				view_stack[view_stack_count - 1].process_cb(&view_stack[view_stack_count - 1], input);
			}

			//Clear input
			input = 0;
		} while (view_stack_changed);

		//Check if we should render an alert
		if (alert_img != 0)
			render_alert();

		//Push updated frame buffer
		display_update();
	}
}

// Pushes a view to the top of the view stack
void viewman_push_view(viewman_view_t view) {
	//Make sure it's not full
	if (view_stack_count == VIEW_STACK_SIZE)
		abort();

	//Write
	view_stack[view_stack_count] = view;
	view_stack_count++;

	//Set flag
	view_stack_changed = 1;

	//Call init function
	if (view.init_cb != 0)
		view.init_cb(&view_stack[view_stack_count - 1]);
}

// Pulls the topmost view off of the view stack
void viewman_pop_view() {
	//Make sure this isn't already empty
	if (view_stack_count == 0)
		abort();

	//Decrement
	view_stack_count--;

	//Set flag
	view_stack_changed = 1;

	//Call deinit function
	if (view_stack[view_stack_count].deinit_cb != 0)
		view_stack[view_stack_count].deinit_cb(&view_stack[view_stack_count]);
}

void viewman_push_alert(const gfx_img_t* icon, const char* text) {
	//Copy
	alert_img = icon;
	strcpy(alert_text, text);
}
