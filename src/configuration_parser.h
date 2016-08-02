#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "configuration.h"

char * xml_get_default_filename();

Configuration * xml_load_engine_from_file(char * filename);
Configuration * xmlconfig_load_engine_from_defaults();

#endif
