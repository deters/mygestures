#include "mygestures.h"

typedef struct
{

    Mygestures *mygestures;
    Display *dpy;

    char *devicename;

    int is_direct_touch;

    int verbose;

    int opcode;
    int event;
    int error;

    int delta_min;

    int shut_down;

} SynapticsGrabber;

SynapticsGrabber *grabber_synaptics_new();
void grabber_synaptics_loop(SynapticsGrabber *self, Mygestures *mygestures);
void grabber_synaptics_finalize(SynapticsGrabber *self);
