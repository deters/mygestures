
#ifndef MYGESTURES_MYGESTURES_H_
#define MYGESTURES_MYGESTURES_H_

#include "configuration.h"

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

Mygestures *mygestures_new();
void mygestures_run(Mygestures *self);

// void on_interrupt(int a);
// void on_kill(int a);

// void release_shared_memory();
// void alloc_shared_memory(char *device_name, int button);
// void send_kill_message(char *device_name);

#endif
