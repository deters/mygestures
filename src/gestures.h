
#ifndef MYGESTURES_MYGESTURES_H_
#define MYGESTURES_MYGESTURES_H_

#include "configuration.h"
#include <X11/Xlib.h>

typedef struct mygestures_
{
	int list_devices_flag;
	int delta_updates;

	int old_x;
	int old_y;

	char *sequence;

	int started;

	Context *root_context;

} Gestures;

Gestures *gestures_new();

void mygestures_set_delta_updates(Gestures *self, int delta_updates);
void mygestures_set_brush_color(Gestures *self, char *brush_color);
void mygestures_start_movement(Gestures *self);
void mygestures_update_movement(Gestures *self, int new_x, int new_y, int delta_min);
int mygestures_end_movement(Gestures *self, int cancel,
							char *device_name, Display *dpy);

void gestures_load_from_file(Gestures *self, char *filename);

// void on_interrupt(int a);
// void on_kill(int a);

// void release_shared_memory();
// void alloc_shared_memory(char *device_name, int button);
// void send_kill_message(char *device_name);

#endif
