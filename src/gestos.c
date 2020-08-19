#define _GNU_SOURCE /* needed by asprintf */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "assert.h"

#include "mousegestures.h"
#include "touchgestures.h"

int main(int argc, char *const *argv)
{

	int pid = fork();

	if (pid == 0)
	{
		touchgestures_main(argc, argv);
	}
	else
	{
		mousegestures_main(argc, argv);
	}

	exit(0);
}
