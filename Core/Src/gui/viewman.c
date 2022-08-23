#include "gui/viewman.h"
#include "gui/display.h"
#include <stdlib.h>

#define VIEW_STACK_SIZE 8

static viewman_view_t view_stack[VIEW_STACK_SIZE];
static int view_stack_count = 0;
static int view_stack_changed = 0;

// Renders a frame
void viewman_process_frame() {
	//Get input (eventually)
	int input = 0;

	//Keep drawing the topmost view until either there is none or it doesn't change
	view_stack_changed = 0;
	do {
		//Clear frame buffer
		display_fb_clear();

		//Process
		if (view_stack_count != 0) {
			view_stack[view_stack_count - 1].process_cb(&view_stack[view_stack_count], input);
		}

		//Clear input
		input = 0;
	} while (view_stack_changed);

	//Push updated frame buffer
	display_update();
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
}
