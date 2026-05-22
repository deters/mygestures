#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "uinput_device.h"

static int uinput_fd = -1;

// Helper to convert X11 Keysym to Linux keycode as a fallback
static unsigned int keysym_to_linux_keycode(KeySym keysym) {
	// If it's alphanumeric ASCII
	if (keysym >= 'a' && keysym <= 'z') {
		switch (keysym) {
			case 'a': return KEY_A;
			case 'b': return KEY_B;
			case 'c': return KEY_C;
			case 'd': return KEY_D;
			case 'e': return KEY_E;
			case 'f': return KEY_F;
			case 'g': return KEY_G;
			case 'h': return KEY_H;
			case 'i': return KEY_I;
			case 'j': return KEY_J;
			case 'k': return KEY_K;
			case 'l': return KEY_L;
			case 'm': return KEY_M;
			case 'n': return KEY_N;
			case 'o': return KEY_O;
			case 'p': return KEY_P;
			case 'q': return KEY_Q;
			case 'r': return KEY_R;
			case 's': return KEY_S;
			case 't': return KEY_T;
			case 'u': return KEY_U;
			case 'v': return KEY_V;
			case 'w': return KEY_W;
			case 'x': return KEY_X;
			case 'y': return KEY_Y;
			case 'z': return KEY_Z;
		}
	}
	if (keysym >= 'A' && keysym <= 'Z') {
		switch (keysym) {
			case 'A': return KEY_A;
			case 'B': return KEY_B;
			case 'C': return KEY_C;
			case 'D': return KEY_D;
			case 'E': return KEY_E;
			case 'F': return KEY_F;
			case 'G': return KEY_G;
			case 'H': return KEY_H;
			case 'I': return KEY_I;
			case 'J': return KEY_J;
			case 'K': return KEY_K;
			case 'L': return KEY_L;
			case 'M': return KEY_M;
			case 'N': return KEY_N;
			case 'O': return KEY_O;
			case 'P': return KEY_P;
			case 'Q': return KEY_Q;
			case 'R': return KEY_R;
			case 'S': return KEY_S;
			case 'T': return KEY_T;
			case 'U': return KEY_U;
			case 'V': return KEY_V;
			case 'W': return KEY_W;
			case 'X': return KEY_X;
			case 'Y': return KEY_Y;
			case 'Z': return KEY_Z;
		}
	}
	if (keysym >= '0' && keysym <= '9') {
		switch (keysym) {
			case '0': return KEY_0;
			case '1': return KEY_1;
			case '2': return KEY_2;
			case '3': return KEY_3;
			case '4': return KEY_4;
			case '5': return KEY_5;
			case '6': return KEY_6;
			case '7': return KEY_7;
			case '8': return KEY_8;
			case '9': return KEY_9;
		}
	}

	// Special keys
	switch (keysym) {
		case XK_Return: return KEY_ENTER;
		case XK_Escape: return KEY_ESC;
		case XK_BackSpace: return KEY_BACKSPACE;
		case XK_Tab: return KEY_TAB;
		case XK_space: return KEY_SPACE;
		case XK_Delete: return KEY_DELETE;
		case XK_Home: return KEY_HOME;
		case XK_End: return KEY_END;
		case XK_Left: return KEY_LEFT;
		case XK_Up: return KEY_UP;
		case XK_Right: return KEY_RIGHT;
		case XK_Down: return KEY_DOWN;
		case XK_Page_Up: return KEY_PAGEUP;
		case XK_Page_Down: return KEY_PAGEDOWN;
		
		// Modifiers
		case XK_Control_L: return KEY_LEFTCTRL;
		case XK_Control_R: return KEY_RIGHTCTRL;
		case XK_Shift_L: return KEY_LEFTSHIFT;
		case XK_Shift_R: return KEY_RIGHTSHIFT;
		case XK_Alt_L: return KEY_LEFTALT;
		case XK_Alt_R: return KEY_RIGHTALT;
		case XK_Super_L: return KEY_LEFTMETA;
		case XK_Super_R: return KEY_RIGHTMETA;

		// F-keys
		case XK_F1: return KEY_F1;
		case XK_F2: return KEY_F2;
		case XK_F3: return KEY_F3;
		case XK_F4: return KEY_F4;
		case XK_F5: return KEY_F5;
		case XK_F6: return KEY_F6;
		case XK_F7: return KEY_F7;
		case XK_F8: return KEY_F8;
		case XK_F9: return KEY_F9;
		case XK_F10: return KEY_F10;
		case XK_F11: return KEY_F11;
		case XK_F12: return KEY_F12;
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
	if (uinput_fd >= 0) return 0; // Already initialized

	uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (uinput_fd < 0) {
		uinput_fd = open("/dev/misc/uinput", O_WRONLY | O_NONBLOCK);
	}

	if (uinput_fd < 0) {
		fprintf(stderr, "mygestures: Failed to open uinput device: %s\n", strerror(errno));
		if (errno == EACCES || errno == EPERM) {
			fprintf(stderr, "\n=========================================================================\n");
			fprintf(stderr, "PERMISSIONS GUIDE FOR NON-ROOT EXECUTION (uinput):\n");
			fprintf(stderr, "To simulate keyboard shortcuts and clicks without running as root, you must\n");
			fprintf(stderr, "configure udev rules to make /dev/uinput accessible to the 'input' group.\n\n");
			fprintf(stderr, "1. Ensure your user is in the 'input' group:\n");
			fprintf(stderr, "   sudo usermod -aG input $USER\n");
			fprintf(stderr, "   (Note: You will need to log out and log back in for this to take effect)\n\n");
			fprintf(stderr, "2. Ensure the mygestures udev rules are installed to allow uinput device creation:\n");
			fprintf(stderr, "   - If you installed via package (e.g. deb), the rules are already installed.\n");
			fprintf(stderr, "   - If you built from source, copy the rules file manually:\n");
			fprintf(stderr, "     sudo cp 99-mygestures.rules /etc/udev/rules.d/\n");
			fprintf(stderr, "     sudo udevadm control --reload-rules && sudo udevadm trigger\n");
			fprintf(stderr, "=========================================================================\n\n");
		}
		return -1;
	}

	// Enable key events
	if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY) < 0) {
		perror("mygestures: ioctl UI_SET_EVBIT EV_KEY failed");
	}
	
	// Enable relative events (for mouse clicks)
	if (ioctl(uinput_fd, UI_SET_EVBIT, EV_REL) < 0) {
		perror("mygestures: ioctl UI_SET_EVBIT EV_REL failed");
	}
	
	// Enable mouse buttons
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_MIDDLE);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_SIDE);
	ioctl(uinput_fd, UI_SET_KEYBIT, BTN_EXTRA);

	// Enable keyboard keys we want to support
	for (int i = 1; i < KEY_MAX; i++) {
		ioctl(uinput_fd, UI_SET_KEYBIT, i);
	}

	// Setup virtual input device
	struct uinput_user_dev uidev;
	memset(&uidev, 0, sizeof(uidev));
	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "mygestures Virtual Device");
	uidev.id.bustype = BUS_USB;
	uidev.id.vendor  = 0x1234;
	uidev.id.product = 0x5678;

	if (write(uinput_fd, &uidev, sizeof(uidev)) < 0) {
		perror("mygestures: Failed to write uinput device description");
		close(uinput_fd);
		uinput_fd = -1;
		return -1;
	}

	if (ioctl(uinput_fd, UI_DEV_CREATE) < 0) {
		perror("mygestures: Failed to create uinput device");
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
		case 4: ev_button = BTN_SIDE; break;
		case 5: ev_button = BTN_EXTRA; break;
		default: ev_button = button; break;
	}

	emit(uinput_fd, EV_KEY, ev_button, 1);
	emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
	usleep(50000); // 50ms hold
	emit(uinput_fd, EV_KEY, ev_button, 0);
	emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
}

void uinput_keypress(Display *dpy, KeySym keysym, int is_press) {
	if (uinput_fd < 0 && uinput_init() < 0) return;

	unsigned int keycode = 0;
	if (dpy) {
		KeyCode x_keycode = XKeysymToKeycode(dpy, keysym);
		if (x_keycode >= 8) {
			keycode = x_keycode - 8;
		}
	}

	if (keycode == 0) {
		// Fallback to static mapping
		keycode = keysym_to_linux_keycode(keysym);
	}

	if (keycode > 0) {
		emit(uinput_fd, EV_KEY, keycode, is_press ? 1 : 0);
		emit(uinput_fd, EV_SYN, SYN_REPORT, 0);
	} else {
		fprintf(stderr, "mygestures: Could not map keysym %lu to uinput keycode\n", keysym);
	}
}
