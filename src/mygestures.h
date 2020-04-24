
#ifndef MYGESTURES_MYGESTURES_H_
#define MYGESTURES_MYGESTURES_H_

#include "configuration.h"
#include <X11/Xlib.h>
#include "drawing/drawing-backing.h"
#include "drawing/drawing-brush.h"

typedef struct mygestures_
{
	Display *dpy;
	int trigger_button;
	int multitouch;
	int libinput;
	int list_devices_flag;
	int delta_updates;

	char *device_name;
	char *brush_color;

	backing_t backing;
	brush_t brush;
	struct brush_image_t *brush_image;

	int old_x;
	int old_y;

	int rought_old_x;
	int rought_old_y;

	char *rought_direction_sequence;

	int started;

	Context *root_context;

} Mygestures;

Mygestures *mygestures_new();

void mygestures_set_delta_updates(Mygestures *self, int delta_updates);
void mygestures_set_brush_color(Mygestures *self, char *brush_color);
void mygestures_start_movement(Mygestures *self, int new_x, int new_y, int delta_min);
void mygestures_update_movement(Mygestures *self, int new_x, int new_y, int delta_min);
int mygestures_end_movement(Mygestures *self, int cancel,
							char *device_name);

void mygestures_load_configuration(Mygestures *self, char *filename);

// void on_interrupt(int a);
// void on_kill(int a);

// void release_shared_memory();
// void alloc_shared_memory(char *device_name, int button);
// void send_kill_message(char *device_name);

#endif
