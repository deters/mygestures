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

#ifndef MYGESTURES_WAYLAND_H_
#define MYGESTURES_WAYLAND_H_

#include "configuration.h"

ActiveWindowInfo *get_wayland_active_window_info(void);
void free_active_window_info(ActiveWindowInfo *info);

#endif /* MYGESTURES_WAYLAND_H_ */
