#include "grabbing.h"

Grabber * grabber_synaptics_init(Grabber * self);
void grabber_synaptics_loop(Grabber * self, Configuration * conf);
void grabber_synaptics_finalize(Grabber * self);
