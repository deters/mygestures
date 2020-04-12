
#ifndef MYGESTURES_MYGESTURES_H_
#define MYGESTURES_MYGESTURES_H_

#include "configuration.h"
#include <X11/Xlib.h>
#include "drawing/drawing-backing.h"
#include "drawing/drawing-brush.h"

typedef struct mygestures_
{

	int trigger_button;
	int multitouch;
	int libinput;
	int list_devices_flag;

	char *custom_config_file;

	int device_count;
	char **device_list;
	char *brush_color;

	Configuration *gestures_configuration;

} Mygestures;

typedef struct
{

	Display *dpy;

	char *devicename;
	int deviceid;
	int is_direct_touch;

	int button;
	int any_modifier;
	int follow_pointer;
	int focus;

	int started;
	int verbose;

	int opcode;
	int event;
	int error;

	int old_x;
	int old_y;

	int delta_min;

	int synaptics;

	int rought_old_x;
	int rought_old_y;

	char *fine_direction_sequence;
	char *rought_direction_sequence;

	backing_t backing;
	brush_t brush;

	int shut_down;

	struct brush_image_t *brush_image;

} Grabber;

Mygestures *mygestures_new();
void mygestures_run(Mygestures *self);

void grabber_loop(Grabber *self, Configuration *conf);
void grabbing_start_movement(Grabber *self, int new_x, int new_y);
void grabbing_update_movement(Grabber *self, int new_x, int new_y);
void grabbing_end_movement(Grabber *self, int new_x, int new_y,
						   char *device_name, Configuration *conf);

// void on_interrupt(int a);
// void on_kill(int a);

// void release_shared_memory();
// void alloc_shared_memory(char *device_name, int button);
// void send_kill_message(char *device_name);

#endif
