/*
 * grabbing.h
 *
 *  Created on: Aug 31, 2013
 *      Author: deters
 */

#ifndef GRABBING_H_
#define GRABBING_H_

#include <X11/Xlib.h>
#include "drawing/drawing-backing.h"
#include "drawing/drawing-brush.h"
#include "gestures.h"

/* modifier keys */
enum {
	SHIFT = 0, CTRL, ALT, WIN, SCROLL, NUM, CAPS, MOD_END
};

/* names of the modifier keys */
extern const char *modifiers_names[];

/* valid strokes */
extern const char stroke_names[];

//void mouse_click(Display *display, int button);

typedef struct {

	Display * dpy;

	char * devicename;
	int deviceid;
	int is_direct_touch;

	int button;

	int started;
	int without_brush;

	int opcode;
	int event;
	int error;

	int old_x;
	int old_y;

	int rought_old_x;
	int rought_old_y;

	char * fine_direction_sequence;
	char * rought_direction_sequence;

	backing_t backing;
	brush_t brush;

	int shut_down;

	struct brush_image_t *brush_image;

} Grabber;

Grabber * grabber_init(char * device_name, int button, int without_brush, int print_devices, char * brush_color);
void grabber_event_loop(Grabber * self, Engine * conf);
void grabber_finalize(Grabber * self);

char * grabber_get_device_name(Grabber * self);
int grabber_get_device_id(Grabber * self);

#endif /* GRABBING_H_ */

