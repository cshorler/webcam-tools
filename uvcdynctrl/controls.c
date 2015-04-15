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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <webcam.h>

#include "controls.h"


CResult
get_control_list (CHandle handle, CControl **controls, unsigned int *count)
{
	CResult ret;
	unsigned int req_size = 0, buffer_size = 0, local_count = 0;

	assert(*controls == NULL);

	do {
		// Allocate the required memory
		if(*controls) free(*controls);
		if(req_size) {		// No allocation the first time
			*controls = (CControl *)malloc(req_size);
			if(*controls == NULL) {
				ret = C_NO_MEMORY;
				goto done;
			}
			buffer_size = req_size;
		}

		// Try to enumerate. If the buffer is not large enough, the required
		// size is returned.
		ret = c_enum_controls(handle, *controls, &req_size, &local_count);
		if(ret != C_SUCCESS && ret != C_BUFFER_TOO_SMALL)
			goto done;
	}
	while(buffer_size < req_size);

	if(count)
		*count = local_count;

done:
	if(ret) {
		if(*controls) free(*controls);
		*controls = NULL;
	}
	return ret;
}


CControlId
get_control_id (CHandle handle, const char *name)
{
	CControlId id = 0;
	CResult res;
	unsigned int count = 0;
	CControl *controls = NULL;
	assert(name);

	// Retrieve the control list
	res = get_control_list(handle, &controls, &count);
	if(res) goto done;

	// Look for a control with the given name and return its ID
	for(int i = 0; i < count; i++) {
		CControl *control = &controls[i];
		if(strcasecmp(name, control->name) == 0) {
			id = control->id;
			goto done;
		}
	}

done:
	if(controls) free(controls);
	return id;
}


int
parse_control_value (const char *string, CControlValue *value)
{
	assert(string);
	assert(value);
	
	if(strcasecmp(string, "true") == 0 ||
	   strcasecmp(string, "on") == 0 ||
	   strcasecmp(string, "yes") == 0) {
		value->value = 1;
		return 0;
	}
	else if(strcasecmp(string, "false") == 0 ||
	   strcasecmp(string, "off") == 0 ||
	   strcasecmp(string, "no") == 0) {
		value->value = 0;
		return 0;
	}

	value->value = atoi(string);
	return 0;
}
