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
#include <string.h>

#include "grabbing.h"
#include "gestures.h"

struct grabbing * grabber;

int is_daemonized = 0;

void usage() {
	printf("\n");
	printf(
			"mygestures %s. Credits: Nir Tzachar (xgestures) & Lucas Augusto Deters\n",
			VERSION);
	printf("\n");
	printf("-h, --help\t: print this usage info\n");
	printf(
			"-c, --config\t: set config file. Defaults: $HOME/.config/mygestures/mygestures.conf /etc/mygestures.conf");
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

void daemonize() {
	int i;

	i = fork();
	if (i != 0)
		exit(0);

	i = chdir("/");
	return;
}

void handle_args(int argc, char * const *argv) {

	char opt;
	static struct option opts[] = { { "help", 0, 0, 'h' },
			{ "button", 1, 0, 'b' }, { "modifier", 1, 0, 'm' }, {
					"without-brush", 0, 0, 'w' }, { "config", 1, 0, 'c' }, {
					"daemonize", 0, 0, 'd' }, { "brush-color", 1, 0, 'l' }, { 0,
					0, 0, 0 } };

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
		case 'm':
			grabbing_set_button_modifier(optarg);
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

int main(int argc, char * const * argv) {

	if (is_daemonized)
		daemonize();

	handle_args(argc, argv);

	Display * dpy = NULL;

	char *s;
	s = XDisplayName(NULL);
	dpy = XOpenDisplay(s);
	if (!dpy) {
		fprintf(stderr, "Can't open display %s\n", s);
		return 1;
	}

	int err = gestures_init();

	if (!err) {

		err = grabbing_init(dpy);

		if (!err) {

			signal(SIGHUP, sighup);
			signal(SIGCHLD, sigchld);

			grabbing_event_loop(dpy);

		}
	}

	grabbing_finalize();

	XCloseDisplay(dpy);

	return err;

}
