#ifndef MYGESTURES_GRABBING_EVDEV_H_
#define MYGESTURES_GRABBING_EVDEV_H_

#include <stddef.h>
#include "grabbing.h"
#include "configuration.h"

int find_mouse_device(char *path, size_t len);
void grabber_evdev_loop(Grabber *self, Configuration *conf);

#endif /* MYGESTURES_GRABBING_EVDEV_H_ */
