/*
 Copyright 2026 Lucas Augusto Deters
 Copyright 2005 Nir Tzachar

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 */

#ifndef MYGESTURES_LOGGING_H_
#define MYGESTURES_LOGGING_H_

#include <stdio.h>

#define LOG_INFO(verbose, fmt, ...) \
    do { \
        if (verbose) { \
            printf(fmt, ##__VA_ARGS__); \
            fflush(stdout); \
        } \
    } while (0)

#define LOG_ERROR(fmt, ...) \
    do { \
        fprintf(stderr, "[ERROR] " fmt, ##__VA_ARGS__); \
        fflush(stderr); \
    } while (0)

#define LOG_WARN(fmt, ...) \
    do { \
        fprintf(stderr, "[WARNING] " fmt, ##__VA_ARGS__); \
        fflush(stderr); \
    } while (0)

#endif /* MYGESTURES_LOGGING_H_ */
