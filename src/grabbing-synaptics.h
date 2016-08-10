#include "grabbing.h"

Grabber * grabber_synaptics_init(char * device_name, int button, int without_brush, int print_devices, char * brush_color, int verbose);
void grabber_synaptics_loop(Grabber * self);
void grabber_synaptics_finalize(Grabber * self);
