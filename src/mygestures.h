
#ifndef MYGESTURES_MYGESTURES_H_
#define MYGESTURES_MYGESTURES_H_

#include "configuration.h"



typedef struct mygestures_ {
	int help_flag;
	int button_option;
	int without_brush_option;
	int damonize_option;
	int list_devices_flag;
	int verbose_option;

	char * custom_config_file;

	int device_count;
	char ** device_list;
	char * brush_color;

	Configuration * gestures_configuration;

} Mygestures;


Mygestures * mygestures_new();
void mygestures_run(Mygestures * self);


#endif
