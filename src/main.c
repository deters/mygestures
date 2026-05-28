#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "assert.h"

#include "mygestures.h"

#include <sys/mman.h>
#include <sys/shm.h>

struct shm_message
{
	int pid;
	int kill;
};

static struct shm_message *message;
static char *shm_identifier;

static void process_arguments(Mygestures *self, int argc, char *const *argv)
{

	char opt;
	static struct option opts[] = {
		{"device", required_argument, 0, 'd'},
		{"button", required_argument, 0, 'b'},
		{"color", required_argument, 0, 'c'},
		{"help", no_argument, 0, 'h'},
		{"visual", no_argument, 0, 'v'},
		{"multitouch", no_argument, 0, 'm'},
		{"evdev", no_argument, 0, 'e'},
		{"create-config", no_argument, 0, 'C'},
		{0, 0, 0, 0}};

	/* read params */

	while (1)
	{
		opt = getopt_long(argc, argv, "b:c:d:vhlmeC", opts, NULL);
		if (opt == -1)
			break;

		switch (opt)
		{

		case 'C':
			self->create_config_flag = 1;
			break;

		case 'b':
			self->trigger_button = atoi(optarg);
			break;

		case 'd':
			if (self->device_count < MAX_GRABBED_DEVICES) {
				self->device_list[self->device_count++] = strdup(optarg);
			} else {
				fprintf(stderr, "Warning: Maximum devices (%d) reached. Ignoring device '%s'.\n", MAX_GRABBED_DEVICES, optarg);
			}
			break;

		case 'm':
			self->multitouch = 1;
			break;

		case 'e':
			self->evdev = 1;
			break;

		case 'v':
			if (!(self->brush_color))
			{
				self->brush_color = "blue";
			}
			break;

		case 'c':
			self->brush_color = strdup(optarg);
			break;

		case 'l':
			self->list_devices_flag = 1;
			break;

		case 'h':
			self->help_flag = 1;
			break;
		}
	}

	if (optind < argc)
	{
		self->custom_config_file = argv[optind++];
	}

	if (optind < argc)
	{
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');
	}
}

/*
 * Ask other instances with same unique_identifier to exit.
 */
void send_kill_message(char *device_name)
{

	assert(message);

	/* if shared message contains a PID, kill that process */
	if (message->pid > 0)
	{
		printf("Asking mygestures running on pid %d to exit..\n", message->pid);

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

static void char_replace(char *str, char oldChar, char newChar)
{
	assert(str);
	char *strPtr = str;
	while ((strPtr = strchr(strPtr, oldChar)) != NULL)
		*strPtr++ = newChar;
}

void alloc_shared_memory(char *device_name, int button)
{

	char *sanitized_device_name = strdup(device_name ? device_name : "");

	if (sanitized_device_name)
	{
		char_replace(sanitized_device_name, '/', '%');
	}

	int bytes = asprintf(&shm_identifier, "/mygestures_uid_%d_dev_%s_button_%d", getuid(),
						 sanitized_device_name ? sanitized_device_name : "", button);

	if (sanitized_device_name)
	{
		free(sanitized_device_name);
	}

	int shared_seg_size = sizeof(struct shm_message);
	int shmfd = shm_open(shm_identifier, O_CREAT | O_RDWR, 0600);

	//free(shm_identifier);

	if (shmfd < 0)
	{
		perror("In shm_open()");
		exit(shmfd);
	}
	int err = ftruncate(shmfd, shared_seg_size);

	message = (struct shm_message *)mmap(NULL, shared_seg_size,
										 PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

	if (message == MAP_FAILED)
	{
		perror("In mmap()");
		close(shmfd);
		exit(1);
	}
	close(shmfd);
}

static void release_shared_memory()
{

	/*  If your head comes away from your neck, it's over! */

	if (shm_identifier)
	{

		if (shm_unlink(shm_identifier) != 0 && errno != ENOENT)
		{
			perror("In shm_unlink()");
		}

		free(shm_identifier);
		shm_identifier = NULL;
	}

	if (message && message != MAP_FAILED)
	{
		munmap(message, sizeof(struct shm_message));
		message = NULL;
	}
}

static void on_interrupt(int a)
{

	if (message && message != MAP_FAILED && message->kill)
	{
		printf("\nMygestures on PID %d asked me to exit.\n", message->pid);
		// shared memory now belongs to the other process. will not be released
	}
	else
	{
		printf("\nReceived the interrupt signal.\n");
		release_shared_memory();
	}

	exit(0);
}

static void on_kill(int a)
{
	release_shared_memory();

	exit(0);
}

int main(int argc, char *const *argv)
{

	Mygestures *self = mygestures_new();

	process_arguments(self, argc, argv);

	mygestures_run(self);

	exit(0);
}
