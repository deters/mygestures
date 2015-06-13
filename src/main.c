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
		fprintf(stdout, "Killing mygestures running on pid %d\n",
				shared_message->pid);

		int err = kill(shared_message->pid, SIGTERM);
		if (err) {
			fprintf(stdout, "PID not killed. Ignoring\n");
		}

		/* give some time */
		usleep(100 * 1000); // 100ms

	}

	/* write own PID in shared memory */
	shared_message->pid = getpid();

	// This is being done in the sigint() method.

}

void its_over() {

	/*  If your head comes away from your neck, it's over! */

	if (unique_identifier) {

		if (shm_unlink(unique_identifier) != 0) {
			perror("In shm_unlink()");
			exit(1);
		}

		free(unique_identifier);

	}
}

void sigint(int a) {

	its_over();

#ifdef DEBUG
	muntrace();
#endif

	exit(0);
}

int handle_args(int argc, char * const *argv, grabbing * grabbing,
		config * conf) {

	char opt;
	static struct option opts[] = { { "help", 0, 0, 'h' },
			{ "button", 1, 0, 'b' }, { "without-brush", 0, 0, 'w' }, { "config",
					1, 0, 'c' }, { "daemonize", 0, 0, 'd' }, { "brush-color", 1,
					0, 'l' }, { 0, 0, 0, 0 } };

	char * custom_filename = NULL;

	while (1) {
		opt = getopt_long(argc, argv, "h::b:m:c:l:wdr", opts, NULL);
		if (opt == -1)
			break;

		switch (opt) {
		case 'h':
			usage();
			break;
		case 'b':
			grabbing_set_button(grabbing, atoi(optarg));
			break;
		case 'c':
			custom_filename = optarg;
			break;
		case 'w':
			grabbing_set_without_brush(grabbing, 1);
			break;
		case 'd':
			daemonize();
			break;
		case 'l':
			grabbing_set_brush_color(grabbing, optarg);
			break;
		}

	}

	int status = CONFIG_OK;

	if (custom_filename) {

		status = config_load_from_file(conf, custom_filename);

	} else {

		status = config_load_from_default(conf);

		if (status == CONFIG_FILE_NOT_FOUND) {

			status = config_create_from_default(conf);

			if (status == CONFIG_OK) {
				status = config_load_from_default(conf);
			}

		}

	}

	return status;
}

int daemonize() {
	int i;

	i = fork();
	if (i != 0)
		exit(0);

	i = chdir("/");

	return i;
}

static void forkexec(char * data) {

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
void execute_action(grabbing * grabbing, struct action *action) {

	assert(action);

	int pid = -1;

	switch (action->type) {
	case ACTION_EXECUTE:

		forkexec(action->data);

		break;
	case ACTION_ICONIFY:
		grabbing_iconify(grabbing);
		break;
	case ACTION_KILL:
		grabbing_kill(grabbing);
		break;
	case ACTION_RAISE:
		grabbing_raise(grabbing);
		break;
	case ACTION_LOWER:
		grabbing_lower(grabbing);
		break;
	case ACTION_MAXIMIZE:
		grabbing_maximize(grabbing);
		break;
	case ACTION_ROOT_SEND:
		grabbing_root_send(grabbing, action->data);
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

	signal(SIGINT, sigint);
	signal(SIGKILL, sigint);

	highlander();

	int status = 0;

	grabbing * grab = grabbing_new();

	config * conf = config_new();

	status = handle_args(argc, argv, grab, conf);

	if (status != CONFIG_OK) {

		char * filename = config_get_filename(conf);

		if (status == CONFIG_CREATE_ERROR) {
			fprintf(stdout, "Unable to autocreate config file '%s'.\n",filename);
		} else {
			fprintf(stdout, "Unable to load config file '%s'.\n",filename);
		}
		its_over();
		return status;
	}

	status = grabbing_prepare(grab);

	if (status) {
		fprintf(stderr, "Error grabbing button. Already running?\n");
		exit(status);
	}

	fprintf(stdout,
			"Draw some movement on the screen with the configured button pressed.\n");

	if (!status) {

		while (1) {

			capture * captured = grabbing_capture(grab);

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
						execute_action(grab, a);
					}

					printf("\n");

				}

				grabbing_free_capture(captured);

			}

		}

	}

	grabbing_finalize(grab);

	config_free(conf);

	exit(0);

}
