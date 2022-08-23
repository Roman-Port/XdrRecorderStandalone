#ifndef INC_GUI_VIEWMAN_H_
#define INC_GUI_VIEWMAN_H_

struct viewman_view;

typedef void (*viewman_view_process_cb)(const struct viewman_view* view, int input);

typedef struct viewman_view {

	void* user_ctx;

	viewman_view_process_cb process_cb;

} viewman_view_t;

// Renders a frame
void viewman_process_frame();

// Pushes a view to the top of the view stack
void viewman_push_view(viewman_view_t view);

// Pulls the topmost view off of the view stack
void viewman_pop_view();

#endif /* INC_GUI_VIEWMAN_H_ */
