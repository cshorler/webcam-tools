/*
 * Test program for libwebcam.
 *
 *
 * Copyright (c) 2006-2007 Logitech.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <webcam.h>


void print_device_info (CHandle handle, char *device_name)
{
	assert(handle || device_name);
	unsigned int size = sizeof(CDevice) + (device_name ? strlen(device_name) : 32) + 84;
	CDevice *info = (CDevice *)malloc(size);
	assert(info);

	if(handle)
		printf("  Getting device information for handle %d ...\n", handle);
	else if(device_name)
		printf("  Getting device information for device name '%s' ...\n", device_name);
	
	int ret = c_get_device_info(handle, device_name, info, &size);
	if(ret) {
		fprintf(stderr, "  Failed to c_get_device_info (%d).\n", ret);
	}
	else {
		printf("    { shortName = '%s', name = '%s', driver = '%s', location = '%s', vid = %04x, pid = %04x, bcd = %d }\n",
				info->shortName, info->name, info->driver, info->location, info->usb.vendor, info->usb.product, info->usb.release);
	}

	free(info);
}

void print_control_info(CControl *control)
{
	printf("      { id = %d, name = '%s', type = %d, flags = %d",
			control->id, control->name, control->type, control->flags);
	if(control->type == CC_TYPE_CHOICE) {
		printf(", choice = {");
		for(int index = 0; index < control->choices.count; index++) {
			printf(" '%s'[%d]", control->choices.list[index].name, control->choices.list[index].index);
		}
		printf(" }");
	}
	else {
		printf(", min = %d, max = %d, def = %d, step = %d",
				control->min.value, control->max.value, control->def.value, control->step.value);
	}
	printf(" }\n");
}

void print_device_controls(CHandle handle)
{
	unsigned int size = 0, count = 0;

	printf("  Getting control information for handle %d ...\n", handle);
	CResult ret = c_enum_controls(handle, NULL, &size, &count);
	if(ret == C_BUFFER_TOO_SMALL) {
		CControl *controls = (CControl *)malloc(size);
		ret = c_enum_controls(handle, controls, &size, &count);
		if(ret) fprintf(stderr, "Unable to c_enum_controls (%d).\n", ret);
/*		printf("    Buffer size = %d (%d controls of %d bytes)\n", size, count, sizeof(CControl));*/

		for(int i = 0; i < count; i++) {
			CControl *control = &controls[i];
			printf("    Control found: %s\n", control->name);
			print_control_info(control);
		}

		free(controls);
	}
	else {
		printf("  No controls found (ret = %d).\n", ret);
	}
}

void print_frame_intervals(CHandle handle, CPixelFormat *pixelformat, CFrameSize *framesize)
{
	unsigned int size = 0, count = 0;

	CResult ret = c_enum_frame_intervals(handle, pixelformat, framesize, NULL, &size, &count);
	if(ret == C_BUFFER_TOO_SMALL) {
		CFrameInterval *intervals = (CFrameInterval *)malloc(size);
		ret = c_enum_frame_intervals(handle, pixelformat, framesize, intervals, &size, &count);
		if(ret) fprintf(stderr, "Unable to c_enum_frame_intervals (%d).\n", ret);
/*		printf("        Buffer size = %d (%d frame intervals of %d bytes)\n", size, count, sizeof(CFrameInterval));*/

		for(int i = 0; i < count; i++) {
			CFrameInterval *frameinterval = &intervals[i];
			if(framesize->type == CF_INTERVAL_DISCRETE) {
				printf("        { discrete: %u/%u }\n",
						frameinterval->n, frameinterval->d);
			}
			else if(framesize->type == CF_INTERVAL_CONTINUOUS) {
				printf("        { continuous: min { %u/%u } .. max { %u/%u } }\n",
						frameinterval->min_n, frameinterval->min_d,
						frameinterval->max_n, frameinterval->max_d);
			}
			else if(framesize->type == CF_INTERVAL_STEPWISE) {
				printf("        { stepwise: min { %u/%u } .. max { %u/%u } / "
						"stepsize { %u/%u } }\n",
						frameinterval->min_n, frameinterval->min_d,
						frameinterval->max_n, frameinterval->max_d,
						frameinterval->step_n, frameinterval->step_d);
			}
		}

		free(intervals);
	}
	else {
		printf("        No frame intervals found (ret = %d).\n", ret);
	}
}

void print_frame_sizes(CHandle handle, CPixelFormat *pixelformat)
{
	unsigned int size = 0, count = 0;

	CResult ret = c_enum_frame_sizes(handle, pixelformat, NULL, &size, &count);
	if(ret == C_BUFFER_TOO_SMALL) {
		CFrameSize *sizes = (CFrameSize *)malloc(size);
		ret = c_enum_frame_sizes(handle, pixelformat, sizes, &size, &count);
		if(ret) fprintf(stderr, "Unable to c_enum_frame_sizes (%d).\n", ret);
/*		printf("      Buffer size = %d (%d frame sizes of %d bytes)\n", size, count, sizeof(CFrameSize));*/

		for(int i = 0; i < count; i++) {
			CFrameSize *framesize = &sizes[i];
			if(framesize->type == CF_SIZE_DISCRETE) {
				printf("      { discrete: width = %u, height = %u }\n",
						framesize->width, framesize->height);
				print_frame_intervals(handle, pixelformat, framesize);
			}
			else if(framesize->type == CF_SIZE_CONTINUOUS) {
				printf("      { continuous: min { width = %u, height = %u } .. "
						"max { width = %u, height = %u } }\n",
						framesize->min_width, framesize->min_height,
						framesize->max_width, framesize->max_height);
				printf("      Refusing to enumerate frame intervals.\n");
				break;
			}
			else if(framesize->type == CF_SIZE_STEPWISE) {
				printf("      { stepwise: min { width = %u, height = %u } .. "
						"max { width = %u, height = %u } / "
						"stepsize { width = %u, height = %u } }\n",
						framesize->min_width, framesize->min_height,
						framesize->max_width, framesize->max_height,
						framesize->step_width, framesize->step_height);
				printf("      Refusing to enumerate frame intervals.\n");
				break;
			}
		}

		free(sizes);
	}
	else {
		printf("      No frame sizes found (ret = %d).\n", ret);
	}
}

void print_frame_formats(CHandle handle)
{
	unsigned int size = 0, count = 0;

	printf("  Getting frame format information for handle %d ...\n", handle);
	CResult ret = c_enum_pixel_formats(handle, NULL, &size, &count);
	if(ret == C_BUFFER_TOO_SMALL) {
		CPixelFormat *formats = (CPixelFormat *)malloc(size);
		ret = c_enum_pixel_formats(handle, formats, &size, &count);
		if(ret) fprintf(stderr, "Unable to c_enum_frame_formats (%d).\n", ret);
/*		printf("    Buffer size = %d (%d formats of %d bytes)\n", size, count, sizeof(CPixelFormat));*/

		for(int i = 0; i < count; i++) {
			CPixelFormat *format = &formats[i];
			printf("    { fourcc = '%s', name = '%s', mimeType = '%s' }\n",
					format->fourcc, format->name, (format->mimeType ? format->mimeType : "<unknown>"));
			print_frame_sizes(handle, format);
		}

		free(formats);
	}
	else {
		printf("  No frame formats found (ret = %d).\n", ret);
	}
}

void print_error_text(CResult error)
{
	char *text = c_get_error_text(error);
	if(text) {
		printf("Error text for error %d: '%s'\n", error, text);
		free(text);
	}
	else {
		printf("ERROR: Unable to get the text for error %d\n", error);
	}
}

void get_brightness(CHandle handle)
{
	CControlValue value;
	CResult ret = c_get_control(handle, CC_BRIGHTNESS, &value);
	if(ret) {
		printf("Failed to get brightness. (ret = %d)\n", ret);
	}
	else {
		printf("Current brightness = %d\n", value.value);
	}
}

void set_brightness(CHandle handle, int val)
{
	CControlValue value;
	value.value = val;
	CResult ret = c_set_control(handle, CC_BRIGHTNESS, &value);
	if(ret) {
		printf("Failed to set brightness. (ret = %d)\n", ret);
	}
	else {
		printf("Successfully set brightness to = %d\n", value.value);
	}
}


void enum_devices()
{
	CResult ret;

	unsigned int size = 0, count = 0;
	printf("Enumerating devices ...\n");
	ret = c_enum_devices(NULL, &size, &count);
	if(ret == C_BUFFER_TOO_SMALL) {
		CDevice *devices = (CDevice *)malloc(size);
		ret = c_enum_devices(devices, &size, &count);
		if(ret) fprintf(stderr, "Unable to c_enum_devices (%d).\n", ret);
/*		printf("  Buffer size = %d (%d devices of %d bytes)\n\n", size, count, sizeof(CDevice));*/

		for(int i = 0; i < count; i++) {
			CDevice *device = &devices[i];
			printf("  Device found: %s\n", device->shortName);

			CHandle handle = c_open_device(device->shortName);
			if(handle == 0) {
				printf("  Failed to open device '%s'.\n", device->shortName);
				continue;
			}
			printf("  Opened device '%s' successfully (handle = %d)\n", device->shortName, handle);
			print_device_info(handle, NULL);
			print_device_controls(handle);
/*			print_frame_formats(handle);*/

			c_close_device(handle);
			printf("  Closed device '%s' (handle = %d)\n\n", device->shortName, handle);
			handle = 0;
		}

		free(devices);
		printf("Done.\n");
	}
	else {
		printf("No devices found.\n");
	}
}


void add_control_mappings()
{
	CResult ret;

	CDynctrlInfo info;
	memset(&info, 0, sizeof(info));

	printf("Adding control mappings ...\n");
	ret = c_add_control_mappings_from_file("dynctrl/logitech.xml", &info);
	if(ret) {
		printf("An error occurred trying to add the control mappings in dynctrl/logitech.xml:\n");
		print_error_text(ret);
	}
	else {
		printf("  Control mappings successfully added (%d controls, %d messages).\n", info.stats.controls.successful + info.stats.controls.failed, info.message_count);
		if(info.message_count) {
			for(int i = 0; i < info.message_count; i++) {
				CDynctrlMessage *msg = &info.messages[i];
				printf("    Message %d: { line = %d, col = %d, text = '%s' }\n", i, msg->line, msg->col, msg->text);
			}
		}
	}

	if(info.messages)
		free(info.messages);
}


int main ()
{
	CResult ret;

	printf("\nTesting libwebcam ...\n");

	// Initialize the library
	ret = c_init();
	if(ret) fprintf(stderr, "Unable to c_init (%d).\n", ret);

	// Get device information
	print_device_info(0, "video0");
	print_device_info(0, "video1");
	print_device_info(0, "video2");

	// Enumerate the devices
	enum_devices();

	CHandle hDevice = c_open_device("video0");
	get_brightness(hDevice);
	sleep(2);
	set_brightness(hDevice, 0);
	sleep(2);
	set_brightness(hDevice, 255);
	sleep(2);
	set_brightness(hDevice, 127);
	c_close_device(hDevice);

	// Enumerate the devices
	//enum_devices();

	// Test error codes
	print_error_text(C_SUCCESS);
	print_error_text(C_SYNC_ERROR);
	print_error_text(123);
	
	// Test adding of control mappings
	add_control_mappings();

	// Release the library
	c_cleanup();
	
	printf("Exiting.\n");
	return 0;
}
