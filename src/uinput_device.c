#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/input-event-codes.h>

#include "uinput_device.h"

static struct libevdev_uinput *uinput_dev = NULL;

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
    {"Mute", KEY_MUTE}, {"AudioMute", KEY_MUTE}, {"XF86AudioMute", KEY_MUTE},
    {"VolumeDown", KEY_VOLUMEDOWN}, {"AudioLowerVolume", KEY_VOLUMEDOWN}, {"XF86AudioLowerVolume", KEY_VOLUMEDOWN},
    {"VolumeUp", KEY_VOLUMEUP}, {"AudioRaiseVolume", KEY_VOLUMEUP}, {"XF86AudioRaiseVolume", KEY_VOLUMEUP},
    {"PlayPause", KEY_PLAYPAUSE}, {"AudioPlayPause", KEY_PLAYPAUSE}, {"XF86AudioPlay", KEY_PLAYPAUSE},
    {"Play", KEY_PLAY}, {"AudioPlay", KEY_PLAY},
    {"Pause", KEY_PAUSE}, {"AudioPause", KEY_PAUSE},
    {"Stop", KEY_STOPCD}, {"AudioStop", KEY_STOPCD}, {"XF86AudioStop", KEY_STOPCD},
    {"Next", KEY_NEXTSONG}, {"AudioNext", KEY_NEXTSONG}, {"XF86AudioNext", KEY_NEXTSONG},
    {"Prev", KEY_PREVIOUSSONG}, {"AudioPrev", KEY_PREVIOUSSONG}, {"XF86AudioPrev", KEY_PREVIOUSSONG},
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

int uinput_init_from_device(struct libevdev *source_dev) {
    if (uinput_dev) return 0;

    /* Enable all keyboard keys on the virtual clone so we can send shortcuts,
     * even if the physical device is just a mouse/touchpad. */
    for (int i = 1; i < KEY_MAX; i++) {
        if (i >= BTN_MISC && i <= BTN_GEAR_UP) continue; /* Skip mouse button range to keep source bits */
        libevdev_enable_event_code(source_dev, EV_KEY, i, NULL);
    }

    int rc = libevdev_uinput_create_from_device(source_dev, 
                                               LIBEVDEV_UINPUT_OPEN_MANAGED,
                                               &uinput_dev);
    if (rc < 0) {
        fprintf(stderr, "mygestures: Failed to create uinput device: %s\n", strerror(-rc));
        return -1;
    }

    printf("mygestures: Created virtual clone of device: %s\n", libevdev_get_name(source_dev));
    return 0;
}

void uinput_close(void) {
    if (uinput_dev) {
        libevdev_uinput_destroy(uinput_dev);
        uinput_dev = NULL;
    }
}

void uinput_click(int button) {
    if (!uinput_dev) return;

    int ev_button = BTN_LEFT;
    switch (button) {
        case 1: ev_button = BTN_LEFT; break;
        case 2: ev_button = BTN_MIDDLE; break;
        case 3: ev_button = BTN_RIGHT; break;
        case 8: ev_button = BTN_SIDE; break;
        case 9: ev_button = BTN_EXTRA; break;
        default: ev_button = button; break;
    }

    libevdev_uinput_write_event(uinput_dev, EV_KEY, ev_button, 1);
    libevdev_uinput_write_event(uinput_dev, EV_SYN, SYN_REPORT, 0);
    usleep(50000);
    libevdev_uinput_write_event(uinput_dev, EV_KEY, ev_button, 0);
    libevdev_uinput_write_event(uinput_dev, EV_SYN, SYN_REPORT, 0);
}

void uinput_keypress_string(const char *keys) {
    if (!uinput_dev) return;
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
        libevdev_uinput_write_event(uinput_dev, EV_KEY, codes[i], 1);
        libevdev_uinput_write_event(uinput_dev, EV_SYN, SYN_REPORT, 0);
        usleep(10000);
    }

    usleep(50000);

    for (int i = count - 1; i >= 0; i--) {
        libevdev_uinput_write_event(uinput_dev, EV_KEY, codes[i], 0);
        libevdev_uinput_write_event(uinput_dev, EV_SYN, SYN_REPORT, 0);
        usleep(10000);
    }

    free(copy);
}

void uinput_forward_event(int type, int code, int value) {
    if (!uinput_dev) return;
    libevdev_uinput_write_event(uinput_dev, type, code, value);
}