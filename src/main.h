

void on_interrupt(int a);
void on_kill(int a);

static void release_shared_memory();
void alloc_shared_memory(char * device_name);
void send_kill_message(char * device_name);
