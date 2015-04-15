/*
 * uvcdynctrl - Manage dynamic controls in uvcvideo
 *
 *
 * Copyright (c) 2006-2007 Logitech.
 *
 * This file is part of uvcdynctrl.
 * 
 * uvcdynctrl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * uvcdynctrl is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with uvcdynctrl.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LV_CONTROLS_H
#define LV_CONTROLS_H

#include <webcam.h>


extern CControlId get_control_id (CHandle handle, const char *name);

extern CResult get_control_list (CHandle handle, CControl **controls, unsigned int *count);

extern int parse_control_value (const char *string, CControlValue *value);


#endif /* LV_CONTROLS_H */
