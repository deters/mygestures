#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "gestures.h"

char * xml_get_default_filename();
char * xml_get_template_filename();

Engine * xml_load_engine(char * filename);

#endif
