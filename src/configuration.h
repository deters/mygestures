#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "gestures.h"

char * xml_get_default_filename();

Engine * xml_load_engine_from_file(char * filename);
Engine * xmlconfig_load_engine_from_defaults();

#endif
