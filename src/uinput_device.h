#ifndef MYGESTURES_UINPUT_DEVICE_H_
#define MYGESTURES_UINPUT_DEVICE_H_

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

int uinput_init_from_device(struct libevdev *source_dev);
void uinput_close(void);
void uinput_click(int button);
void uinput_keypress_string(const char *keys);
void uinput_forward_event(int type, int code, int value);

#endif /* MYGESTURES_UINPUT_DEVICE_H_ */