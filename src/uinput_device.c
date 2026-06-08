#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>

#include "uinput_device.h"

static int uinput_fd = -1;

typedef struct {
    const char *name;
    unsigned int code;
} KeyMap;

static KeyMap key_map[] = {
    {"a", KEY_A}, {"b", KEY_B}, {"c", KEY_C}, {"d", KEY_D}, {"e", KEY_E},
    {"f", KEY_F}, {"g", KEY_G}, {"h", KEY_H}, {"i", KEY_I}, {"j", KEY_J},
    {"k", KEY_K}, {"l", KEY_L}, {"m", KEY_M}, {"n", KEY_N}, {"o", KEY_O},
    {"p", KEY_P}, {"q", KEY_Q}, {"r", KEY_R}, {"s", KEY_S}, {"t", KEY_T},
    {"u", KEY_U}, {"v", KEY_V}, {"w", KEY_W}, {"x", KEY_X}, {"y", KEY_Y},
    {"z", KEY_Z},
    {"0", KEY_0}, {"1", KEY_1}, {"2", KEY_2}, {"3", KEY_3}, {"4", KEY_4},
    {"5", KEY_5}, {"6", KEY_6}, {"7", KEY_7}, {"8", KEY_8}, {"9", KEY_9},
    {"Return", KEY_ENTER}, {"Enter", KEY_ENTER}, {"Escape", KEY_ESC}, {"Esc", KEY_ESC},
    {"BackSpace", KEY_BACKSPACE}, {"Tab", KEY_TAB}, {"space", KEY_SPACE}, {"Space", KEY_SPACE},
    {"Delete", KEY_DELETE}, {"Home", KEY_HOME}, {"End", KEY_END},
    {"Left", KEY_LEFT}, {"Up", KEY_UP}, {"Right", KEY_RIGHT}, {"Down", KEY_DOWN},
    {"Page_Up", KEY_PAGEUP}, {"Page_Down", KEY_PAGEDOWN},
    {"Control_L", KEY_LEFTCTRL}, {"Control_R", KEY_RIGHTCTRL}, {"Ctrl", KEY_LEFTCTRL},
    {"Shift_L", KEY_LEFTSHIFT}, {"Shift_R", KEY_RIGHTSHIFT}, {"Shift", KEY_LEFTSHIFT},
    {"Alt_L", KEY_LEFTALT}, {"Alt_R", KEY_RIGHTALT}, {"Alt", KEY_LEFTALT},
    {"Super_L", KEY_LEFTMETA}, {"Super_R", KEY_RIGHTMETA}, {"Super", KEY_LEFTMETA}, {"Win", KEY_LEFTMETA},
    {"F1", KEY_F1}, {"F2", KEY_F2}, {"F3", KEY_F3}, {"F4", KEY_F4}, {"F5", KEY_F5},
    {"F6", KEY_F6}, {"F7", KEY_F7}, {"F8", KEY_F8}, {"F9", KEY_F9}, {"F10", KEY_F10},
    {"F11", KEY_F11}, {"F12", KEY_F12},
    {NULL, 0}
};

static unsigned int name_to_keycode(const char *name) {
    if (!name) return 0;
    for (int i = 0; key_map[i].name != NULL; i++) {
        if (strcasecmp(name, key_map[i].name) == 0) {
            return key_map[i].code;
        }
    }
    return 0;
}

static void emit(int fd, int type, int code, int val) {
	struct input_event ie;
	memset(&ie, 0, sizeof(ie));
	ie.type = type;
	ie.code = code;
	ie.value = val;
	if (write(fd, &ie, sizeof(ie)) < 0) {
		perror("mygestures: Error writing event to uinput");
	}
}

int uinput_init(void) {
	if (uinput_fd >= 0) return 0;

	uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (uinput_fd < 0) {
		uinput_fd = open("/dev/misc/uinput", O_WRONLY | O_NONBLOCK);
	}

	if (uinput_fd < 0) {
		return -1;
	}

	ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY);
	ioctl(uinput_fd, UI_SET_EVBIT, EV_REL);
	ioctl(uinput_fd, UI_SET_RELBIT, REL_X);
	ioctl(uinput_fd, UI_SET_RELBIT, REL_Y);
	ioctl(uinput_fd, UI_SET_RELBIT, REL_WHEEL);
	ioctl(uinput_fd, UI_SET_RELBIT, REL_HWHEEL);
    
    ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_MIDDLE);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_SIDE);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_EXTRA);

	for (int i = 1; i < KEY_MAX; i++) {
		ioctl(uinput_fd, UI_SET_KEYBIT, i);
	}

	struct uinput_user_dev uidev;
	memset(&uidev, 0, sizeof(uidev));
	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "mygestures Virtual Device");
	uidev.id.bustype = BUS_USB;
	uidev.id.vendor  = 0x1234;
	uidev.id.product = 0x5678;

	if (write(uinput_fd, &uidev, sizeof(uidev)) < 0) {
		close(uinput_fd);
		uinput_fd = -1;
		return -1;
	}

	if (ioctl(uinput_fd, UI_DEV_CREATE) < 0) {
		close(uinput_fd);
		uinput_fd = -1;
		return -1;
	}

	return 0;
}

void uinput_close(void) {
	if (uinput_fd >= 0) {
		ioctl(uinput_fd, UI_DEV_DESTROY);
		close(uinput_fd);
		uinput_fd = -1;
	}
}

void uinput_click(int button) {
	if (uinput_fd < 0 && uinput_init() < 0) return;

	int ev_button = BTN_LEFT;
	switch (button) {
		case 1: ev_button = BTN_LEFT; break;
		case 2: ev_button = BTN_MIDDLE; break;
		case 3: ev_button = BTN_RIGHT; break;
		case 8: ev_button = BTN_SIDE; break;
		case 9: ev_button = BTN_EXTRA; break;
		default: ev_button = button; break;
	}

	emit(uinput_fd, EV_KEY, ev_button, 1);
	emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
	usleep(50000);
	emit(uinput_fd, EV_KEY, ev_button, 0);
	emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
}

void uinput_keypress_string(const char *keys) {
    if (uinput_fd < 0 && uinput_init() < 0) return;
    if (!keys) return;

    char *copy = strdup(keys);
    char *token;
    char *iter = copy;
    unsigned int codes[16];
    int count = 0;

    while ((token = strsep(&iter, "+ ")) != NULL && count < 16) {
        if (*token == '\0') continue;
        unsigned int code = name_to_keycode(token);
        if (code > 0) {
            codes[count++] = code;
        }
    }

    for (int i = 0; i < count; i++) {
        emit(uinput_fd, EV_KEY, codes[i], 1);
        emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
        usleep(10000);
    }

    usleep(50000);

    for (int i = count - 1; i >= 0; i--) {
        emit(uinput_fd, EV_KEY, codes[i], 0);
        emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
        usleep(10000);
    }

    free(copy);
}

void uinput_forward_event(int type, int code, int value) {
	if (uinput_fd < 0 && uinput_init() < 0) return;
	emit(uinput_fd, type, code, value);
}