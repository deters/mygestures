#ifndef MYGESTURES_UINPUT_DEVICE_H_
#define MYGESTURES_UINPUT_DEVICE_H_

int uinput_init(void);
void uinput_close(void);
void uinput_click(int button);
void uinput_keypress_string(const char *keys);
void uinput_forward_event(int type, int code, int value);

#endif /* MYGESTURES_UINPUT_DEVICE_H_ */