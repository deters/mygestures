/*
 * mygestures.c
 *
 *  Created on: Aug 31, 2013
 *      Author: deters
 */

#define _GNU_SOURCE /* needed by asprintf */
#include <stdio.h>  /* needed by asprintf */

#include <signal.h>
#include <wait.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/types.h>

#include "grabbing.h"
#include "gestures.h"
#include "configuration.h"
#include "config.h"
#include "assert.h"

struct shm_message {
	int pid;
	int kill;
};

char * unique_identifier;
struct shm_message * message;

typedef struct mygestures_ {

	int help;
	int button;
	int without_brush;
	int run_as_daemon;
	int list_devices;

	char * device;
	char * brush_color;
	char * config;

	Engine * engine;

} Mygestures;

static void mygestures_usage() {
	printf("%s\n\n", PACKAGE_STRING);
	printf("Usage: mygestures [OPTIONS] [CONFIG_FILE]\n");
	printf("\n");
	printf("CONFIG_FILE:\n");

	char * default_file = xml_get_default_filename();
	printf(" Default: %s\n", default_file);
	free(default_file);

	char * template_file = xml_get_template_filename();
	printf(" Template: %s\n", template_file);
	free(template_file);

	printf("\n");
	printf("OPTIONS:\n");
	printf(" -b, --button <BUTTON>      : Button used to draw the gesture\n");
	printf("                              Default: '1' on touchscreen devices,\n");
	printf("                                       '3' on other pointer devices\n");
	printf(" -d, --device <DEVICENAME>  : Device to grab.\n");
	printf("                              Default: 'Virtual core pointer'\n");
	printf(" -l, --device-list          : Print all available devices an exit.\n");
	printf(" -z, --daemonize            : Fork the process and return.\n");
	printf(" -x, --brush-color          : Brush color.\n");
	printf("                              Default: blue\n");
	printf("                              Options: yellow, white, red, green, purple, blue\n");
	printf(" -w, --without-brush        : Don't paint the gesture on screen.\n");
	printf(" -h, --help                 : Help\n");
}

char *replace(char *str, char oldChar, char newChar) {

	assert(str);

	char *strPtr = str;
	while ((strPtr = strchr(strPtr, oldChar)) != NULL)
		*strPtr++ = newChar;
	return str;
}

/*
 * Ask other instances with same unique_identifier to exit.
 */
static void be_unique(char * device_name) {

	// the unique_identifier = mygestures + uid + device being grabbed

	if (device_name) {
		asprintf(&unique_identifier, "/mygestures_uid_%d_dev_%s", getuid(),
				replace(device_name, '/', '%'));
	} else {
		asprintf(&unique_identifier, "/mygestures_uid_%d_dev_%s", getuid(),
						"DEFAULT_DEVICE");
	}

	int shared_seg_size = sizeof(struct shm_message);
	int shmfd = shm_open(unique_identifier, O_CREAT | O_RDWR, 0600);
	if (shmfd < 0) {
		perror("In shm_open()");
		exit(shmfd);
	}
	int err = ftruncate(shmfd, shared_seg_size);

	message = (struct shm_message *) mmap(NULL, shared_seg_size,

	PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if (message == NULL) {
		perror("In mmap()");
		exit(1);
	}

	/* if shared message contains a PID, kill that process */
	if (message->pid > 0) {
		fprintf(stdout, "Asking mygestures running on pid %d to exit.\n", message->pid);

		int running = message->pid;

		message->pid = getpid();
		message->kill = 1;

		int err = kill(running, SIGINT);

		/* give some time. ignore failing */
		usleep(100 * 1000); // 100ms

	}

	/* write own PID in shared memory */
	message->pid = getpid();
	message->kill = 0;

}

static void mygestures_release_shm_file() {

	/*  If your head comes away from your neck, it's over! */

	if (unique_identifier) {

		if (shm_unlink(unique_identifier) != 0) {
			perror("In shm_unlink()");
			exit(1);
		}

		free(unique_identifier);

	}
}

static void on_kill(int a) {
	mygestures_release_shm_file();
	exit(0);
}

static void on_interrupt(int a) {

	if (message->kill) {
		printf("Mygestures on PID %d asked me to exit.\n", message->pid);
		// shared memory now belongs to the other process. will not be cleaned.
	} else {
		printf("Received the interrupt signal.\n");
		mygestures_release_shm_file();
	}

	exit(0);

}

static void daemonize() {
	int i;

	i = fork();
	if (i != 0)
		exit(0);

	i = chdir("/");
	return;
}

Mygestures * mygestures_new() {
	Mygestures *self = malloc(sizeof(Mygestures));
	bzero(self, sizeof(Mygestures));
	self->brush_color;
	self->config;
	self->device;
	return self;
}

void mygestures_parse_arguments(Mygestures * self, int argc, char * const *argv) {

	char opt;
	static struct option opts[] = { { "help", no_argument, 0, 'h' }, { "without-brush", no_argument,
			0, 'w' }, { "daemonize", no_argument, 0, 'z' }, { "button", required_argument, 0, 'b' },
			{ "config",
			required_argument, 0, 'c' }, { "brush-color", required_argument, 0, 'l' }, { "device",
			required_argument, 0, 'd' }, { 0, 0, 0, 0 } };

	while (1) {
		opt = getopt_long(argc, argv, "b:c:d:hlwx:z", opts, NULL);
		if (opt == -1)
			break;

		switch (opt) {

		case 'd':
			self->device = optarg;
			break;

		case 'b':
			self->button = atoi(optarg);
			break;

		case 'c':
			self->config = optarg;
			break;

		case 'x':
			self->brush_color = optarg;
			break;

		case 'w':
			self->without_brush = 1;
			break;

		case 'l':
			self->list_devices = 1;
			break;

		case 'z':
			self->run_as_daemon = 1;
			break;

		case 'h':
			self->help = 1;
			break;

		}

	}

	if (optind < argc) {
		self->config = argv[optind++];
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');
	}

}

void mygestures_reload(Mygestures * self) {

	if (self->config) {
		printf("Loading custom configuration from %s\n", self->config);
		self->engine = xml_load_engine_from_file(self->config);
	} else {
		printf("Loading default configuration\n");
		self->engine = xml_load_engine_from_defaults();
	}



}

void mygestures_start(Mygestures * self) {

	Grabber * grabber = grabber_init(self->device, self->button, self->without_brush,
			self->list_devices, self->brush_color);

	grabber_event_loop(grabber, self->engine);




}

int main(int argc, char * const * argv) {

	Mygestures *self = mygestures_new();

	mygestures_parse_arguments(self, argc, argv);

	signal(SIGINT, on_interrupt);
	signal(SIGKILL, on_kill);

	if (self->run_as_daemon)
		daemonize();

	if (self->help) {
		mygestures_usage();
		exit(0);
	}

	be_unique(self->device);

	mygestures_reload(self);

	mygestures_start(self);

	mygestures_release_shm_file(self);

	exit(0);

}
