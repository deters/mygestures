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

static char * unique_identifier = NULL;

struct shared_structure {
	int pid;
	int kill;
};

static struct shared_structure * message;

static void usage() {
	printf("%s\n\n", PACKAGE_STRING);
	printf("Usage: mygestures [OPTIONS] [CONFIG_FILE]\n");
	printf("\n");
	printf("CONFIG_FILE:\n");

	char * default_file = xml_get_default_filename();
	printf(" Default: %s\n", default_file);
	free(default_file);

	char * template_file = xml_get_template_filename();
	printf(" Default: %s\n", template_file);
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

/*
 * Ask other instances with same unique_identifier to exit.
 */
static void be_unique(int device_id) {

	// the unique_identifier = mygestures + uid + device being grabbed
	int bytes = asprintf(&unique_identifier, "/mygestures_uid_%d_dev_%d", getuid(), device_id);

	int shared_seg_size = sizeof(struct shared_structure);
	int shmfd = shm_open(unique_identifier, O_CREAT | O_RDWR, 0600);
	if (shmfd < 0) {
		perror("In shm_open()");
		exit(shmfd);
	}
	int err = ftruncate(shmfd, shared_seg_size);
	message = (struct shared_structure *) mmap(NULL, shared_seg_size,
	PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if (message == NULL) {
		perror("In mmap()");
		exit(1);
	}

	/* if shared message contains a PID, kill that process */
	if (message->pid > 0) {
		fprintf(stdout, "Asking mygestures running on pid %d to kill himself.\n", message->pid);

		int running = message->pid;

		message->kill = 1;
		message->pid = getpid();

		int err = kill(running, SIGINT);

		/* give some time. ignore failing */
		usleep(100 * 1000); // 100ms

	}

	/* write own PID in shared memory */
	message->pid = getpid();
	message->kill = 0;

}

static void clean_shared_memory() {

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
	clean_shared_memory();
	exit(0);
}

static void on_interrupt(int a) {

	if (message->kill) {
		printf("PID %d asked me to exit.\n", message->pid);
		on_kill(a);
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

struct args_t {

	int help;
	int button;
	int without_brush;
	int is_daemonized;
	int list_devices;

	char * device;
	char * brush_color;
	char * config;
};

static struct args_t * handle_args(int argc, char * const *argv) {

	char opt;
	static struct option opts[] = { { "help", no_argument, 0, 'h' }, { "without-brush", no_argument,
			0, 'w' }, { "daemonize", no_argument, 0, 'z' }, { "button", required_argument, 0, 'b' },
			{ "config",
			required_argument, 0, 'c' }, { "brush-color", required_argument, 0, 'l' }, { "device",
			required_argument, 0, 'd' }, { 0, 0, 0, 0 } };

	struct args_t * result = malloc(sizeof(struct args_t));
	bzero(result, sizeof(struct args_t));

	while (1) {
		opt = getopt_long(argc, argv, "b:c:d:hlwx:z", opts, NULL);
		if (opt == -1)
			break;

		switch (opt) {

		case 'd':
			result->device = optarg;
			break;

		case 'b':
			result->button = atoi(optarg);
			break;

		case 'c':
			result->config = optarg;
			break;

		case 'x':
			result->brush_color = optarg;
			break;

		case 'w':
			result->without_brush = 1;
			break;

		case 'l':
			result->list_devices = 1;
			break;

		case 'z':
			result->is_daemonized = 1;
			break;

		case 'h':
			result->help = 1;
			break;

		}

	}

	if (optind < argc) {
		result->config = argv[optind++];
	}

	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');
	}
	return result;
}

int main(int argc, char * const * argv) {

	signal(SIGINT, on_interrupt);
	signal(SIGKILL, on_kill);

	struct args_t * args = handle_args(argc, argv);

	if (args->help) {
		usage();
		exit(0);
	}

	if (args->is_daemonized)
		daemonize();

	Engine * engine = xml_load_engine(args->config);

	Grabber * grabber = grabber_init(args->device, args->button, args->without_brush,
			args->list_devices, args->brush_color);

	be_unique(grabber_get_device_id(grabber));

	grabber_event_loop(grabber, engine);
	grabber_finalize(grabber);

	exit(0);

}
