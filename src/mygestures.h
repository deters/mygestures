
#ifndef MYGESTURES_MYGESTURES_H_
#define MYGESTURES_MYGESTURES_H_

#include "configuration.h"

#define MAX_GRABBED_DEVICES 16

typedef struct mygestures_
{
	int help_flag;
	int trigger_button;
	int evdev;

	char *custom_config_file;

	int device_count;
	char **device_list;

	Configuration *gestures_configuration;

} Mygestures;

Mygestures *mygestures_new();
void mygestures_run(Mygestures *self);

#endif
