#ifndef WAYLAND_CONTEXT_H_
#define WAYLAND_CONTEXT_H_

#include <sys/types.h>

typedef struct WaylandContext {
    uid_t uid;
    char *username;
    char sway_sock[1024];
    char hypr_sig[256];
    int is_sway;
    int is_hypr;
    int is_gnome;
    int is_kde;
} WaylandContext;

void discover_wayland_context(WaylandContext *ctx);
const char *get_user_command_prefix(WaylandContext *ctx, char *out_buf, size_t len);

#endif /* WAYLAND_CONTEXT_H_ */