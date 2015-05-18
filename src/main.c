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

/*
 * Try to kill other instances of the program before running.
 * Will use shared memory to store the pid of the running session.
 * Will kill the session stored previously on shared memory before writing new pid.
 */
void highlander() {

	/* "there can be only one" */

	struct shm_info * shared_message = NULL;
	int shared_seg_size = sizeof(struct shm_info);

	/* the unique identifier */
	asprintf(&unique_identifier, "/mygestures_uid%d", getuid());

	/* request the shared memory */
	int shmfd = shm_open(unique_identifier, O_CREAT | O_RDWR, 0600);

	if (shmfd < 0) {
		perror("In shm_open()");
		exit(1);
	}
	ftruncate(shmfd, shared_seg_size);

	/* read the struct from shared memory */
	shared_message = (struct shm_info *) mmap(NULL, shared_seg_size,
	PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

	if (shared_message == NULL) {
		perror("In mmap()");
		exit(1);
	}

	/* if shared message contains a PID, kill that process */
	if (shared_message->pid > 0) {
		printf("Killing mygestures running on pid %d\n", shared_message->pid);

		kill(shared_message->pid, SIGTERM);

		/* give some time */
		usleep(100 * 1000); // 100ms

	}

	/* write own PID in shared memory */
	shared_message->pid = getpid();

	// This is being done in the sigint() method.

}

void sigint(int a) {

	/* release shared memory used to share current PID */

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

void handle_args(int argc, char * const *argv, char ** config_file) {

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
			*config_file = optarg;
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

	char * config_file = NULL;

	handle_args(argc, argv, &config_file);

	config * conf = config_new();

	int err = 0;

	if (config_file) {
		err = config_load_from_file(conf, config_file);
	} else {
		err = config_load_from_default(conf);
	}

	if (err) {
		fprintf(stderr, "Error loading gestures.\n");
		return err;
	}

	signal(SIGINT, sigint);
	highlander();

	struct grabbing * grabber;
	err = grabbing_init();

	if (is_daemonized)
		daemonize();

	if (err) {
		fprintf(stderr, "Error grabbing button. Already running?\n");
		exit(err);
	}

	fprintf(stdout,
			"Draw some movement on the screen with the configured button pressed.\n");

	if (!err) {

		while (1) {

			capture * captured = grabbing_capture();

			if (captured) {

				struct gesture * detected = config_match_captured(conf,
						captured);

				printf("\n");
				printf("  title : %s\n", captured->window_title);
				printf("  class : %s\n", captured->window_class);
				printf(" movement values :");

				int i = 0;
				for (i = 0; i < captured->movement_representations_count; ++i) {

					printf(" %s", captured->movement_representations[i]);
				}

				printf("\n");

				if (!detected) {

					printf("        -- NO MATCH --\n");

				} else {

					printf("movement: %s\n", detected->movement->name);
					printf("context : %s\n", detected->context->name);
					printf(" gesture: %s\n", detected->name);

					int j = 0;

					printf("         ");

					for (j = 0; j < detected->actions_count; ++j) {
						struct action * a = detected->actions[j];
						printf(" (%s)", a->data);
						execute_action(a);
					}

					printf("\n");

				}

				grabbing_free_grabbed_information(captured);

			}

		}

	}

	grabbing_finalize();

	config_free(conf);

	exit(0);

}
