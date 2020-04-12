// #define _GNU_SOURCE /* needed by asprintf */

// #include <sys/shm.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <stddef.h>
// #include <assert.h>
// #include <sys/mman.h>
// #include "string.h"
// #include <signal.h>
// #include <fcntl.h>

// // static struct shm_message *message;

// // struct shm_message
// // {
// //     int pid;
// //     int kill;
// // };

// void on_interrupt(int a)
// {

//     // if (message->kill)
//     // {
//     //     printf("\nMygestures on PID %d asked me to exit.\n", message->pid);
//     //     // shared memory now belongs to the other process. will not be released
//     // }
//     // else
//     // {
//     // printf("\nReceived the interrupt signal.\n");
//     // release_shared_memory();
//     // // }

//     exit(0);
// }

// void on_kill(int a)
// {
//     //release_shared_memory();

//     exit(0);
// }

// static void char_replace(char *str, char oldChar, char newChar)
// {
//     assert(str);
//     char *strPtr = str;
//     while ((strPtr = strchr(strPtr, oldChar)) != NULL)
//         *strPtr++ = newChar;
// }

// void get_shm_name(char **shm_identifier, char *device_name)
// {

//     char *sanitized_device_name = device_name;

//     if (sanitized_device_name)
//     {
//         char_replace(sanitized_device_name, '/', '%');
//     }
//     else
//     {
//         sanitized_device_name = "";
//     }

//     int size = asprintf(&shm_identifier, "/mygestures_uid_%d_dev_%s", getuid(),
//                         sanitized_device_name);

//     if (size < 0)
//     {
//         printf("Error in asprintf at get_shm_name\n");
//         exit(1);
//     }
// }

// static void alloc_unique_channel(char *device_name)
// {

//     int shared_seg_size = sizeof(struct shm_message);
//     int shmfd = shm_open(shm_identifier, O_CREAT | O_RDWR, 0600);

//     //free(shm_identifier);

//     if (shmfd < 0)
//     {
//         perror("In shm_open()");
//         exit(shmfd);
//     }
//     int err = ftruncate(shmfd, shared_seg_size);

//     if (err)
//     {
//         printf("Error truncating SHM variable\n");
//     }

//     message = (struct shm_message *)mmap(NULL, shared_seg_size,
//                                          PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

//     if (message == NULL)
//     {
//         perror("In mmap()");
//         exit(1);
//     }
// }

// void release_shared_memory(char *device_name)
// {

//     /*  If your head comes away from your neck, it's over! */

//     if (shm_identifier)
//     {

//         if (shm_unlink(shm_identifier) != 0)
//         {
//             perror("In shm_unlink()");
//             exit(1);
//         }

//         free(shm_identifier);
//     }
// }

// /*
//  * Ask other instances with same unique_identifier to exit.
//  */
// void send_kill_message(char *device_name)
// {

//     assert(message);

//     send_kill_message(device_name);

//     /* if shared message contains a PID, kill that process */
//     if (message->pid > 0)
//     {
//         printf("Asking mygestures running on pid %d to exit..\n", message->pid);

//         int running = message->pid;

//         message->pid = getpid();
//         message->kill = 1;

//         int err = kill(running, SIGINT);

//         if (err)
//         {
//             printf("Error sending kill message.\n");
//         }

//         /* give some time. ignore failing */
//         usleep(100 * 1000); // 100ms
//     }

//     /* write own PID in shared memory */
//     message->pid = getpid();
//     message->kill = 0;
// }
