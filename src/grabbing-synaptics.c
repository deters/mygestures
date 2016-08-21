/*
 *   Copyright 2002-2004 Peter Osterlund <petero2@telia.com>
 *   Copyright 2016      Lucas Augusto Deters
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/shm.h> /* needed for synaptics */
#include <X11/Xlib.h>
#include <sys/time.h>

#include "grabbing-synaptics.h"

#define SHM_SYNAPTICS 23947
typedef struct _SynapticsSHM {
	int version; /* Driver version */

	/* Current device state */
	int x, y; /* actual x, y coordinates */
	int z; /* pressure value */
	int numFingers; /* number of fingers */
	int fingerWidth; /* finger width value */
	int left, right, up, down; /* left/right/up/down buttons */
	Bool multi[8];
	Bool middle;
} SynapticsSHM;

static int synaptics_shm_is_equal(SynapticsSHM * s1, SynapticsSHM * s2) {
	int i;

	if ((s1->x != s2->x) || (s1->y != s2->y) || (s1->z != s2->z)
			|| (s1->numFingers != s2->numFingers)
			|| (s1->fingerWidth != s2->fingerWidth) || (s1->left != s2->left)
			|| (s1->right != s2->right) || (s1->up != s2->up)
			|| (s1->down != s2->down) || (s1->middle != s2->middle))
		return 0;

	for (i = 0; i < 8; i++)
		if (s1->multi[i] != s2->multi[i])
			return 0;

	return 1;
}

/** Init and return SHM area or NULL on error */
static SynapticsSHM *
grabber_synaptics_shm_init(int debug) {
	SynapticsSHM *synshm = NULL;
	int shmid = 0;

	if ((shmid = shmget(SHM_SYNAPTICS, sizeof(SynapticsSHM), 0)) == -1) {
		if ((shmid = shmget(SHM_SYNAPTICS, 0, 0)) == -1) {
			if (debug) {
				printf(
						"Can't access shared memory area. SHMConfig disabled?\n");
			}
		} else {
			if (debug) {
				printf(
						"Incorrect size of shared memory area. Incompatible driver version?\n");
			}
		}
	} else if ((synshm = (SynapticsSHM *) shmat(shmid, NULL, SHM_RDONLY))
			== NULL) {
		if (debug) {
			perror("shmat");
		}
	}

	return synshm;
}

void syn_print(const SynapticsSHM* cur) {
	printf("%4d %4d %3d %d %2d %2d %d %d %d %d  %d%d%d%d%d%d%d%d\n", cur->x,
			cur->y, cur->z, cur->numFingers, cur->fingerWidth, cur->left,
			cur->right, cur->up, cur->down, cur->middle, cur->multi[0],
			cur->multi[1], cur->multi[2], cur->multi[3], cur->multi[4],
			cur->multi[5], cur->multi[6], cur->multi[7]);
}

void grabber_synaptics_loop(Grabber * self, Configuration * conf) {

	SynapticsSHM *synshm = NULL;

	synshm = grabber_synaptics_shm_init(0);

	if (!synshm) {
		printf(" You will need a patched synaptics driver with SHM enabled.\n");
		printf(
		" Take a look at https://github.com/Chosko/xserver-xorg-input-synaptics\n");
		return;
	}

	int delay = 10;

	SynapticsSHM old;

	memset(&old, 0, sizeof(SynapticsSHM));
	old.x = -1; /* Force first equality test to fail */

	int max_fingers = 0;

	while (!self->shut_down) {

		SynapticsSHM cur = *synshm;

		if (!synaptics_shm_is_equal(&old, &cur)) {

			int delay = 10;

			// release
			if (cur.numFingers >= 3 && max_fingers >= 3) {

				if (self->verbose) {
					syn_print(&cur);
				}

				grabbing_update_movement(self, cur.x, cur.y);

				//// got > 3 fingers
			} else if (cur.numFingers == 0 && max_fingers >= 3) {

				if (self->verbose) {
					syn_print(&cur);
					printf("stopped	\n");
				}

				// reset max fingers
				max_fingers = 0;

				grabbing_end_movement(self, old.x, old.y, conf);

				/// energy economy
				int delay = 50;

			} else if (cur.numFingers >= 3 && max_fingers < 3) {

				if (self->verbose) {

					syn_print(&cur);

				}

				max_fingers = max_fingers + 1;

				if (max_fingers >= 3) {

					if (self->verbose) {
						printf("started\n");
					}

					grabbing_start_movement(self, cur.x, cur.y);

				}

			}

			//// movement

		}

		usleep(delay * 1000);

		old = cur;

	}

}
