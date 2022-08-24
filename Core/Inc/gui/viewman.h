#ifndef INC_GUI_VIEWMAN_H_
#define INC_GUI_VIEWMAN_H_

#include "defines.h"

struct viewman_view;

typedef void (*viewman_view_init_cb)(const struct viewman_view* view);
typedef void (*viewman_view_tick_cb)(const struct viewman_view* view);
typedef void (*viewman_view_process_cb)(const struct viewman_view* view, int input);
typedef void (*viewman_view_deinit_cb)(const struct viewman_view* view);

typedef struct viewman_view {

	void* user_ctx;

	viewman_view_init_cb init_cb;
	viewman_view_tick_cb tick_cb;
	viewman_view_process_cb process_cb;
	viewman_view_deinit_cb deinit_cb;

} viewman_view_t;

// Should be called as fast as possible
void viewman_tick();

// Pushes an alert that is dismissed by button press. Icon must be set and must be 20x20. Text should not be longer than 20 characters. Text is copied on call.
void viewman_push_alert(const gfx_img_t* icon, const char* text);

// Pushes a view to the top of the view stack
void viewman_push_view(viewman_view_t view);

// Pulls the topmost view off of the view stack
void viewman_pop_view();

#endif /* INC_GUI_VIEWMAN_H_ */
