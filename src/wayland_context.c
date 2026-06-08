#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include "wayland_context.h"

void discover_wayland_context(WaylandContext *ctx) {
    memset(ctx, 0, sizeof(WaylandContext));

    const char *env_sway = getenv("SWAYSOCK");
    const char *env_hypr = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    const char *desktop = getenv("XDG_CURRENT_DESKTOP");

    if (desktop) {
        if (strstr(desktop, "GNOME") != NULL || strstr(desktop, "gnome") != NULL || strstr(desktop, "Ubuntu") != NULL) {
            ctx->is_gnome = 1;
        }
        if (strstr(desktop, "KDE") != NULL || strstr(desktop, "kde") != NULL) {
            ctx->is_kde = 1;
        }
    }

    if (env_sway) {
        snprintf(ctx->sway_sock, sizeof(ctx->sway_sock), "%s", env_sway);
        ctx->is_sway = 1;
        return;
    }
    if (env_hypr) {
        snprintf(ctx->hypr_sig, sizeof(ctx->hypr_sig), "%s", env_hypr);
        ctx->is_hypr = 1;
        return;
    }

    char *sudo_uid_env = getenv("SUDO_UID");
    char *sudo_user_env = getenv("SUDO_USER");

    if (sudo_uid_env && sudo_user_env) {
        ctx->uid = atoi(sudo_uid_env);
        ctx->username = strdup(sudo_user_env);
    } else {
        ctx->uid = getuid();
        if (ctx->uid > 0) {
            struct passwd *pw = getpwuid(ctx->uid);
            if (pw) ctx->username = strdup(pw->pw_name);
        }
    }

    if (ctx->uid > 0) {
        char path[512];
        snprintf(path, sizeof(path), "/run/user/%d", ctx->uid);
        DIR *dir = opendir(path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir))) {
                if (strstr(entry->d_name, "sway-ipc") && strstr(entry->d_name, ".sock")) {
                    snprintf(ctx->sway_sock, sizeof(ctx->sway_sock), "/run/user/%d/%s", ctx->uid, entry->d_name);
                    ctx->is_sway = 1;
                    break;
                }
            }
            closedir(dir);
        }

        if (!ctx->is_sway) {
            snprintf(path, sizeof(path), "/run/user/%d/hypr", ctx->uid);
            DIR *dir2 = opendir(path);
            if (dir2) {
                struct dirent *entry;
                while ((entry = readdir(dir2))) {
                    if (strlen(entry->d_name) >= 40) {
                        snprintf(ctx->hypr_sig, sizeof(ctx->hypr_sig), "%s", entry->d_name);
                        ctx->is_hypr = 1;
                        break;
                    }
                }
                closedir(dir2);
            }
        }
    }

    if (ctx->uid == 0 || (!ctx->is_sway && !ctx->is_hypr)) {
        DIR *dir = opendir("/run/user");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir))) {
                uid_t d_uid = atoi(entry->d_name);
                if (d_uid >= 1000) {
                    char path[512];
                    snprintf(path, sizeof(path), "/run/user/%d", d_uid);
                    DIR *sub_dir = opendir(path);
                    if (sub_dir) {
                        struct dirent *sub_entry;
                        while ((sub_entry = readdir(sub_dir))) {
                            if (strstr(sub_entry->d_name, "sway-ipc") && strstr(sub_entry->d_name, ".sock")) {
                                snprintf(ctx->sway_sock, sizeof(ctx->sway_sock), "/run/user/%d/%s", d_uid, sub_entry->d_name);
                                ctx->is_sway = 1;
                                break;
                            }
                        }
                        closedir(sub_dir);
                    }

                    if (!ctx->is_sway) {
                        snprintf(path, sizeof(path), "/run/user/%d/hypr", d_uid);
                        DIR *sub_dir2 = opendir(path);
                        if (sub_dir2) {
                            struct dirent *sub_entry;
                            while ((sub_entry = readdir(sub_dir2))) {
                                if (strlen(sub_entry->d_name) >= 40) {
                                    snprintf(ctx->hypr_sig, sizeof(ctx->hypr_sig), "%s", sub_entry->d_name);
                                    ctx->is_hypr = 1;
                                    break;
                                }
                            }
                            closedir(sub_dir2);
                        }
                    }

                    if (ctx->is_sway || ctx->is_hypr) {
                        ctx->uid = d_uid;
                        struct passwd *pw = getpwuid(ctx->uid);
                        if (ctx->username) free(ctx->username);
                        ctx->username = pw ? strdup(pw->pw_name) : NULL;
                        break;
                    }
                }
            }
            closedir(dir);
        }
    }
}

const char *get_user_command_prefix(WaylandContext *ctx, char *out_buf, size_t len) {
    if (getuid() == 0 && ctx->username) {
        if (ctx->is_sway) {
            snprintf(out_buf, len, "sudo -u %s env SWAYSOCK=%s XDG_RUNTIME_DIR=/run/user/%d ", 
                     ctx->username, ctx->sway_sock, ctx->uid);
        } else if (ctx->is_hypr) {
            snprintf(out_buf, len, "sudo -u %s env HYPRLAND_INSTANCE_SIGNATURE=%s XDG_RUNTIME_DIR=/run/user/%d ", 
                     ctx->username, ctx->hypr_sig, ctx->uid);
        } else {
            snprintf(out_buf, len, "sudo -u %s env XDG_RUNTIME_DIR=/run/user/%d ", 
                     ctx->username, ctx->uid);
        }
        return out_buf;
    }
    return "";
}