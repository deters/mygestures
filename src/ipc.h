#ifndef MYGESTURES_IPC_H_
#define MYGESTURES_IPC_H_

void on_interrupt(int a);
void on_kill(int a);

void alloc_shared_memory(char *device_name, int button);
void release_shared_memory();
void send_kill_message(char *device_name);

#endif
