#ifndef GESTURES_H_
#define GESTURES_H_

#include "gestures.h"

typedef struct gestos_
{
    int list_devices_flag;
    int fingers;
    int button;
    int grabbers;
    char *config_file;
    char *device_name;
    Gestures *gestures;
    Display *dpy;
} Gestos;

#endif