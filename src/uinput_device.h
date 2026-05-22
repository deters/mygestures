#ifndef MYGESTURES_UINPUT_DEVICE_H_
#define MYGESTURES_UINPUT_DEVICE_H_

#include <X11/Xlib.h>

int uinput_init(void);
void uinput_close(void);
void uinput_click(int button);
void uinput_keypress(Display *dpy, KeySym keysym, int is_press);

#endif /* MYGESTURES_UINPUT_DEVICE_H_ */
