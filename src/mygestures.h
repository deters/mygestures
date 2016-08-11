
#ifndef MYGESTURES_MYGESTURES_H_
#define MYGESTURES_MYGESTURES_H_

#include "configuration.h"


typedef struct parameters_ {
	int help;
	int button;
	int without_brush;
	int run_as_daemon;
	int list_devices;
	int reconfigure;
	int verbose;
	int debug;
	char * custom_config_file;
	char * device;
	char * brush_color;

	struct gestures_ * gestures;

} Parameters;


#endif
