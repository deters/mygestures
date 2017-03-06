
#ifndef MYGESTURES_MYGESTURES_H_
#define MYGESTURES_MYGESTURES_H_

#include "configuration.h"



typedef struct mygestures_ {
	int help;
	int button;
	int without_brush;
	int run_as_daemon;
	int list_devices;
	int verbose;
	char * custom_config_file;
	int device_count;
	char ** device_list;
	char * brush_color;

	Configuration * gestures_configuration;

} Mygestures;



#endif
