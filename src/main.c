/*
 * mygestures.c
 *
 *  Created on: Aug 31, 2013
 *      Author: deters
 */

#include <signal.h>
#include <wait.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "grabbing.h"
#include "assert.h"
#include "gestures.h"
#include "config.h"

struct grabbing * grabber;

int is_daemonized = 0;

int shut_down = 0;

void usage() {
	printf("%s\n\n", PACKAGE_STRING);
	printf("Usage:\n");
	printf("-h, --help\t: print this usage info\n");
	printf(
			"-c, --config\t: use config file.\n\t\tDefaults: $HOME/.config/mygestures/mygestures.xml /etc/mygestures.xml\n");
	printf("-b, --button\t: which button to use. default is 3\n");
	printf("-d, --daemonize\t: laymans daemonize\n");
	printf("-m, --modifier\t: which modifier to use. valid values are: \n");
	printf("\t\t  CTRL, SHIFT, ALT, WIN, CAPS, NUM, AnyModifier \n");
	printf("\t\t  default is SHIFT\n");
	printf(
			"-l, --brush-color\t: choose a brush color. available colors are:\n");
	printf("\t\t\t  yellow, white, red, green, purple, blue (default)\n");
	printf("-w, --without-brush\t: don't paint the gesture on screen.\n");
	exit(0);
}

void sighup(int a) {
	int err = gestures_init();
	if (err != 0) {
		fprintf(stderr, "Error reloading gestures.");
	}
	return;
}

void sigchld(int a) {
	int err;
	waitpid(-1, &err, WNOHANG);
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

/**
 * Execute an action
 */
void execute_action(struct action *action) {

	assert(action);

	int pid = -1;

	switch (action->type) {
	case ACTION_EXECUTE:

		system(action->data);

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
		fprintf(stderr, "found an unknown gesture \n");
	}

	return;
}

int main(int argc, char * const * argv) {

	handle_args(argc, argv);

	if (is_daemonized)
		daemonize();

	int err = 0;

	err = gestures_init();

	if (err) {
		fprintf(stderr, "Error loading gestures.\n");
		return err;
	}

	err = grabbing_init();

	if (err) {
		fprintf(stderr, "Error grabbing button. Already running?\n");
		return err;
	}

	if (!err) {

		signal(SIGHUP, sighup);
		signal(SIGCHLD, sigchld);

		while (!shut_down) {

			struct grabbed_information *grabbed = NULL;

			grabbed = grabbing_capture_movements();

			if (grabbed) {

				char * sequence = grabbed->advanced_movement;
				struct gesture * gesture = gesture_match(sequence,
						grabbed->window_class, grabbed->window_title);

				if (!gesture) {
					char * sequence = grabbed->basic_movement;
					gesture = gesture_match(sequence, grabbed->window_class,
							grabbed->window_title);
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

	return err;

}
