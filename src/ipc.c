#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <assert.h>

struct shm_message {
    int pid;
    int kill;
};

static struct shm_message *message = NULL;
static char *shm_identifier = NULL;

static void char_replace(char *str, char oldChar, char newChar) {
    assert(str);
    char *strPtr = str;
    while ((strPtr = strchr(strPtr, oldChar)) != NULL)
        *strPtr++ = newChar;
}

void alloc_shared_memory(char *device_name, int button) {
    char *sanitized_device_name = strdup(device_name ? device_name : "");
    if (sanitized_device_name) {
        char_replace(sanitized_device_name, '/', '%');
    }

    int bytes = asprintf(&shm_identifier, "/mygestures_uid_%d_dev_%s_button_%d", getuid(),
                         sanitized_device_name ? sanitized_device_name : "", button);

    if (sanitized_device_name) {
        free(sanitized_device_name);
    }

    int shared_seg_size = sizeof(struct shm_message);
    int shmfd = shm_open(shm_identifier, O_CREAT | O_RDWR, 0600);

    if (shmfd < 0) {
        perror("In shm_open()");
        exit(shmfd);
    }
    int err = ftruncate(shmfd, shared_seg_size);

    message = (struct shm_message *)mmap(NULL, shared_seg_size,
                                         PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if (message == MAP_FAILED) {
        perror("In mmap()");
        close(shmfd);
        exit(1);
    }
    close(shmfd);
}

void release_shared_memory() {
    if (shm_identifier) {
        if (shm_unlink(shm_identifier) != 0 && errno != ENOENT) {
            perror("In shm_unlink()");
        }
        free(shm_identifier);
        shm_identifier = NULL;
    }

    if (message && message != MAP_FAILED) {
        munmap(message, sizeof(struct shm_message));
        message = NULL;
    }
}

void send_kill_message(char *device_name) {
    assert(message);
    if (message->pid > 0) {
        printf("Asking mygestures running on pid %d to exit..\n", message->pid);
        int running = message->pid;
        message->pid = getpid();
        message->kill = 1;
        kill(running, SIGINT);
        usleep(100 * 1000); // 100ms
    }
    message->pid = getpid();
    message->kill = 0;
}

void on_interrupt(int a) {
    if (message && message != MAP_FAILED && message->kill) {
        printf("\nMygestures on PID %d asked me to exit.\n", message->pid);
    } else {
        printf("\nReceived the interrupt signal.\n");
        release_shared_memory();
    }
    exit(0);
}

void on_kill(int a) {
    release_shared_memory();
    exit(0);
}
