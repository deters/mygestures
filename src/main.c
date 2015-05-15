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
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>

#include "grabbing.h"
#include "assert.h"
#include "gestures.h"
#include "config.h"

#ifdef DEBUG
#include <mcheck.h>
#endif

int is_daemonized = 0;

char * unique_identifier = NULL;

struct shm_info {
	int pid;
};

void usage() {
	printf("%s\n\n", PACKAGE_STRING);
	printf("Usage:\n");
	printf("-h, --help\t: print this usage info\n");
	printf(
			"-c, --config\t: use config file.\n\t\tDefaults: $HOME/.config/mygestures/mygestures.xml /etc/mygestures.xml\n");
	printf("-b, --button\t: which button to use. default is 3\n");
	printf("-d, --daemonize\t: laymans daemonize\n");
	printf(
			"-l, --brush-color\t: choose a brush color. available colors are:\n");
	printf("\t\t\t  yellow, white, red, green, purple, blue (default)\n");
	printf("-w, --without-brush\t: don't paint the gesture on screen.\n");
	exit(0);
}

void highlander() {

	// "there can be only one"

	// Current process will use a shared memory to share current pid.
	// if there is already a pid in this region of memory, kill it.

	// the pid will be stored on a struct
	struct shm_info * shared_message = NULL;
	int shared_seg_size = sizeof(struct shm_info);

	// create a unique identifier using the UID
	asprintf(&unique_identifier, "/mygestures_uid%d", getuid());

	// request the shared memory identified by unique_identifier
	int shmfd = shm_open(unique_identifier, O_CREAT | O_RDWR, 0600);


	if (shmfd < 0) {
		perror("In shm_open()");
		exit(1);
	}
	ftruncate(shmfd, shared_seg_size);

	// map the memory region to be used in current process
	shared_message = (struct shm_info *) mmap(NULL, shared_seg_size,
	PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if (shared_message == NULL) {
		perror("In mmap()");
		exit(1);
	}

	// check if another process wrote his PID in the shared memory. Kill the process who has this PID
	if (shared_message->pid > 0) {
		printf("Killing mygestures running on pid %d\n", shared_message->pid);

		kill(shared_message->pid, SIGTERM);
		usleep(100 * 1000); // 100ms

	}

	// write the PID of the current process in the shared memory
	shared_message->pid = getpid();

	// the shared memory need to be released when mygestures is closed to avoid killing some other process who had been assigned to the same pid.
	// This is being done in the sigint() method.

}

void handle_args(int argc, char * const *argv) {

	char opt;
	static struct option opts[] = { { "help", 0, 0, 'h' },
			{ "button", 1, 0, 'b' }, { "without-brush", 0, 0, 'w' }, { "config",
					1, 0, 'c' }, { "daemonize", 0, 0, 'd' }, { "brush-color", 1,
					0, 'l' }, { 0, 0, 0, 0 } };

	while (1) {
		opt = getopt_long(argc, argv, "h::b:m:c:l:wdr", opts, NULL);
		if (opt == -1)
			break;

		switch (opt) {
		case 'h':
			usage();
			break;
		case 'b':
			grabbing_set_button(atoi(optarg));
			break;
		case 'c':
			gestures_set_config_file(optarg);
			break;
		case 'w':
			grabbing_set_without_brush(1);
			break;
		case 'd':
			is_daemonized = 1;
			break;
		case 'l':
			grabbing_set_brush_color(optarg);
			break;
		}

	}

	return;
}

int daemonize() {
	int i;

	i = fork();
	if (i != 0)
		exit(0);

	i = chdir("/");

	return i;
}

void sigint(int a) {

	if (unique_identifier) {
		if (shm_unlink(unique_identifier) != 0) {
			perror("In shm_unlink()");
			exit(1);
		}
	}

#ifdef DEBUG
	muntrace();
#endif

	exit(0);
}

void forkexec(char * data) {

	pid_t pid;

	pid = fork();
	if (pid == -1) {
		perror("fork error");
		exit(1);
	} else if (pid == 0) {
		/* code for child */

		system(data);

		_exit(1);
	} else { /* code for parent */
	}

}

/**
 * Execute an action
 */
void execute_action(struct action *action) {

	assert(action);

	int pid = -1;

	switch (action->type) {
	case ACTION_EXECUTE:

		forkexec(action->data);

		break;
	case ACTION_ICONIFY:
		grabbing_iconify();
		break;
	case ACTION_KILL:
		grabbing_kill();
		break;
	case ACTION_RAISE:
		grabbing_raise();
		break;
	case ACTION_LOWER:
		grabbing_lower();
		break;
	case ACTION_MAXIMIZE:
		grabbing_maximize();
		break;
	case ACTION_ROOT_SEND:
		grabbing_root_send(action->data);
		break;
	default:
		fprintf(stderr, "found an unknown gesture\n");
	}

	return;
}

int main(int argc, char * const * argv) {

#ifdef DEBUG
	printf("Debug mode enabled. Memory trace will be writed to MALLOC_TRACE.\n");
	mtrace();
#endif

	handle_args(argc, argv);

	struct grabbing * grabber;

	struct context * root_context = gestures_init();

	if (!root_context) {
		fprintf(stderr, "Error loading gestures.\n");
		return -1;
	}

	int err = grabbing_init();

	highlander();

	if (is_daemonized)
		daemonize();

	if (err) {
		fprintf(stderr, "Error grabbing button. Already running?\n");
		exit(err);
	}

	fprintf(stdout,
			"Draw some movement on the screen with the configured button pressed.\n");

	if (!err) {

		signal(SIGINT, sigint);

		while (1) {

			struct grabbed_information *grabbed = NULL;

			grabbed = grabbing_capture_movements();

			if (grabbed) {

				char * sequence = grabbed->advanced_movement;
				struct gesture * gesture = gesture_match(root_context, sequence,
						grabbed->window_class, grabbed->window_title);

				if (!gesture) {
					char * sequence = grabbed->basic_movement;
					gesture = gesture_match(root_context, sequence,
							grabbed->window_class, grabbed->window_title);
				}

				printf("\n");
				printf("Window Title = \"%s\"\n", grabbed->window_title);
				printf("Window Class = \"%s\"\n", grabbed->window_class);

				if (!gesture) {
					printf("Captured sequences %s or %s --> not found\n",
							grabbed->advanced_movement,
							grabbed->basic_movement);
				} else {
					printf(
							"Captured sequence '%s' --> Movement '%s' --> Gesture '%s'\n",
							sequence, gesture->movement->name, gesture->name);

					int j = 0;

					for (j = 0; j < gesture->actions_count; ++j) {
						struct action * a = gesture->actions[j];
						printf(" (%s)\n", a->data);
						execute_action(a);
					}

				}

				free_captured_movements(grabbed);

			}

		}

	}

	grabbing_finalize();

	gestures_finalize(root_context);

	exit(0);

}
