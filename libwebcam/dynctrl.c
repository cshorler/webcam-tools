/**
 * \file
 * Dynamic control support for the Linux UVC driver.
 *
 * \ingroup libwebcam
 */

/*
 * Copyright (c) 2006-2011 Logitech.
 *
 * This file is part of libwebcam.
 * 
 * libwebcam is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * libwebcam is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with libwebcam.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>
#include <errno.h>
#include <iconv.h>

#include "webcam.h"
#include "libwebcam.h"

#ifdef ENABLE_UVCVIDEO_DYNCTRL

#include "compat.h"
#include <libxml/parser.h>
#include <libxml/tree.h>


/*
 * Macros
 */

/// Convert a single hex character into its numeric value
#define HEX_DECODE_CHAR(c)		((c) >= '0' && (c) <= '9' ? (c) - '0' : (tolower(c)) - 'a' + 0xA)
/// Convert two hex characters into their byte value
#define HEX_DECODE_BYTE(cc)		((HEX_DECODE_CHAR((cc)[0]) << 4) + HEX_DECODE_CHAR((cc)[1]))
/// Helper macro to convert the UTF-8 strings used by libxml2 into ASCII
#define UNICODE_TO_ASCII(s)		(unicode_to_ascii(s, ctx))
/// Helper macro to convert the UTF-8 strings used by libxml2 into whitespace normalized ASCII
#define UNICODE_TO_NORM_ASCII(s)	(unicode_to_normalized_ascii(s, ctx))



/*
 * Enumerations
 */

/**
 * Type of constants that are allowed in the XML configuration file.
 */
typedef enum _ConstantType {
	CT_INVALID		= 0,		///< Invalid type (used internally)
	CT_INTEGER,					///< Signed integer constant
	CT_GUID,					///< GUID

} ConstantType;

// Define uvc_control_data_type which existed for uvcvideo < r209.
// It has been removed because enum's are not binary compatible on certain platforms among
// different compilers. In our case we don't care and enums are handy, so we redefine it.
#ifdef UVC_CTRL_DATA_TYPE_RAW
	#undef UVC_CTRL_DATA_TYPE_RAW
	#undef UVC_CTRL_DATA_TYPE_SIGNED
	#undef UVC_CTRL_DATA_TYPE_UNSIGNED
	#undef UVC_CTRL_DATA_TYPE_BOOLEAN
	#undef UVC_CTRL_DATA_TYPE_ENUM
	#undef UVC_CTRL_DATA_TYPE_BITMASK

	/**
	 * Data type for dynamic UVC driver controls.
	 */
	enum uvc_control_data_type {
		UVC_CTRL_DATA_TYPE_RAW = 0,
		UVC_CTRL_DATA_TYPE_SIGNED,
		UVC_CTRL_DATA_TYPE_UNSIGNED,
		UVC_CTRL_DATA_TYPE_BOOLEAN,
		UVC_CTRL_DATA_TYPE_ENUM,
		UVC_CTRL_DATA_TYPE_BITMASK,
	};
#endif



/*
 * Types
 */

/**
 * Constant read from the XML configuration file.
 */
typedef struct _Constant {
	/// Data type of the constant
	ConstantType		type;
	/// Name of the constant
	char				* name;

	union {
		/// Integer value (only valid if type == CT_INTEGER)
		int					value;
		/// GUID value (only valid if type == CT_GUID)
		__u8				guid[GUID_SIZE];
	};

	/// Pointer to the next constant in the list
	struct _Constant	* next;

} Constant;

/**
 * UVC extension unit control for use with UVCIOC_CTRL_ADD.
 */
typedef struct _UVCXUControl {
	/// Unique identifier of the extension unit control definition
	xmlChar						* id;
	/// UVC data required to identify an extension unit control
	struct uvc_xu_control_info	info;
	/// Pointer to the next extension unit control definition in the list
	struct _UVCXUControl		* next;

} UVCXUControl;

/**
 * Helper structure that contains handles and information useful during the XML parsing process.
 */
typedef struct _ParseContext {
	/// Structure used to pass information between the application and libwebcam. Can be NULL.
	CDynctrlInfo	* info;
	/// Size of the info->messages buffer (which contains the CDynctrlMessage structures
	/// and the strings pointed to).
	unsigned int	messages_size;
	/// Conversion descriptor for iconv
	iconv_t			cd;
	/// List of constants parsed from the @c constants node
	Constant		* constants;
	/// Handle to the libwebcam device
	CHandle			handle;
	/// Handle to the V4L2 device that is used to add the dynamic controls
	int				v4l2_handle;
	/// List of controls parsed from the @c devices nodes
	UVCXUControl	* controls;
	/// The current parsing pass (first device is pass 1, second device pass 2, etc.)
	int				pass;

} ParseContext;



/*
 * Helper functions
 */

/**
 * Converts a GUID string into a GUID byte array.
 *
 * This function assumes that @a guid is a valid GUID string. No validation is performed.
 */
static void guid_to_byte_array (const char *guid, __u8 *array)
{
	array[ 0] = HEX_DECODE_BYTE(guid +  6);
	array[ 1] = HEX_DECODE_BYTE(guid +  4);
	array[ 2] = HEX_DECODE_BYTE(guid +  2);
	array[ 3] = HEX_DECODE_BYTE(guid     );

	array[ 4] = HEX_DECODE_BYTE(guid + 11);
	array[ 5] = HEX_DECODE_BYTE(guid +  9);

	array[ 6] = HEX_DECODE_BYTE(guid + 16);
	array[ 7] = HEX_DECODE_BYTE(guid + 14);

	array[ 8] = HEX_DECODE_BYTE(guid + 19);
	array[ 9] = HEX_DECODE_BYTE(guid + 21);

	array[10] = HEX_DECODE_BYTE(guid + 24);
	array[11] = HEX_DECODE_BYTE(guid + 26);
	array[12] = HEX_DECODE_BYTE(guid + 28);
	array[13] = HEX_DECODE_BYTE(guid + 30);
	array[14] = HEX_DECODE_BYTE(guid + 32);
	array[15] = HEX_DECODE_BYTE(guid + 34);
}


/**
 * Checks whether a given string contains a valid integer value.
 *
 * This function considers all integers recognized by strtol() as valid. This includes
 * hexadecimal numbers with a '0x' prefix and octal numbers with a leading zero.
 *
 * @param string	String containing an integer. Can be NULL.
 * @param value		A pointer in which the converted value will be stored.
 * 					Can be NULL if conversion is not required.
 *
 * @return			boolean indicating whether the string represents a valid integer
 */
static int is_valid_integer_string (const char *string, int *value)
{
	if(!string) return 0;

	char *end = NULL;
	int ret = strtol(string, &end, 0);
	if(*string != '\0' && *end == '\0') {
		if(value) *value = ret;
		return 1;
	}
	return 0;
}


/**
 * Checks whether a given value represents a valid size.
 *
 * Only positive numbers are considered valid size values. In addition, an upper
 * threshold can be specified, above which values will be considered invalid.
 *
 * @param value		value to be checked
 * @param max		upper threshold for the validity of @a value. If the threshold
 * 					is negative, no threshold check is performed, i.e. max == INT_MAX
 * 					is assumed.
 * 
 * @return			boolean indicating whether the value is a valid size
 */
static inline int is_valid_size (int value, int max)
{
	return (value >= 0 && value <= (max < 0 ? INT_MAX : max));
}


/**
 * Checks whether a given string represents a valid size.
 *
 * This function works just like is_valid_size() but accepts an input string
 * and a buffer to store the converted value.
 */
static int is_valid_size_string (const char *string, int *value, int max)
{
	int temp_value = 0;
	if(is_valid_integer_string(string, &temp_value) && is_valid_size(temp_value, max)) {
		if(value)
			*value = temp_value;
		return 1;
	}
	return 0;
}


/**
 * Checks whether a given string represents a valid GUID.
 */
static int is_valid_guid (const char *string)
{
	if(string == NULL && strlen(string) != 36)
		return 0;

	int i;
	for(i = 0; i < 36; i++) {
		switch(i) {
			case  8:
			case 13:
			case 18:
			case 23:
				if(string[i] != '-')
					return 0;
				break;
			default:
				if(!isxdigit(string[i]))
					return 0;
		}
	}
	return 1;
}


/**
 * Converts a string to a version consisting of major and minor version.
 *
 * Accepted formats are "x.y" and "x" where x is the major and y the minor version.
 *
 * @param string	version string to be converted
 * @param major		pointer to an integer where the major version number should be stored
 * @param minor		pointer to an integer where the minor version number should be stored
 *
 * @return			boolean indicating whether the conversion succeeded or not
 */
static int string_to_version (const char *string, unsigned int *major, unsigned int *minor)
{
	assert(string);

	unsigned int maj = 0, min = 0;

	// After the following conversion ...
	// if endptr == '.', we have a "x.y" format,
	// if endptr == '\0', we have a "x" format,
	// if endptr == string, there were no digits.
	char *end = NULL;
	maj = strtol(string, &end, 10);
	if(*end == '.') {			
		min = atoi(end + 1);		
	}								

	if(major)
		*major = maj;
	if(minor)
		*minor = min;

	return end == string ? 0 : 1;
}


/**
 * Converts the name of a UVC request into its corresponding constant.
 *
 * @param name		request string to be converted. Can be NULL.
 *
 * @return
 * 		- 0 if the conversion failed, i.e. the request name was not recognized
 * 		- a UVC_CONTROL_* constant if the conversion was successful
 */
static __u32 get_uvc_request_by_name (const xmlChar *name)
{
	__u32 request = 0;	// Used to denote an invalid/unsupported UVC control request
	if(!name) return request;

	if(xmlStrEqual(name, BAD_CAST("SET_CUR"))) {
		request = UVC_CTRL_FLAG_SET_CUR;
	}
	else if(xmlStrEqual(name, BAD_CAST("GET_CUR"))) {
		request = UVC_CTRL_FLAG_GET_CUR;
	}
	else if(xmlStrEqual(name, BAD_CAST("GET_MIN"))) {
		request = UVC_CTRL_FLAG_GET_MIN;
	}
	else if(xmlStrEqual(name, BAD_CAST("GET_MAX"))) {
		request = UVC_CTRL_FLAG_GET_MAX;
	}
	else if(xmlStrEqual(name, BAD_CAST("GET_RES"))) {
		request = UVC_CTRL_FLAG_GET_RES;
	}
	else if(xmlStrEqual(name, BAD_CAST("GET_DEF"))) {
		request = UVC_CTRL_FLAG_GET_DEF;
	}
	return request;
}


/**
 * Converts the name of a UVC data type constant into its corresponding numerical value.
 *
 * @param name		data type string to be converted. Can be NULL.
 *
 * @return
 * 		- -1 if the conversion failed, i.e. the data type name was not recognized
 * 		- a UVC_CTRL_DATA_TYPE_* constant if the conversion was successful
 */
static enum uvc_control_data_type get_uvc_ctrl_type_by_name (const xmlChar *name)
{
	enum uvc_control_data_type type = -1;	// Used to denote an invalid/unsupported UVC control type
	if(!name) return type;

	if(xmlStrEqual(name, BAD_CAST("UVC_CTRL_DATA_TYPE_RAW"))) {
		type = UVC_CTRL_DATA_TYPE_RAW;
	}
	else if(xmlStrEqual(name, BAD_CAST("UVC_CTRL_DATA_TYPE_SIGNED"))) {
		type = UVC_CTRL_DATA_TYPE_SIGNED;
	}
	else if(xmlStrEqual(name, BAD_CAST("UVC_CTRL_DATA_TYPE_UNSIGNED"))) {
		type = UVC_CTRL_DATA_TYPE_UNSIGNED;
	}
	else if(xmlStrEqual(name, BAD_CAST("UVC_CTRL_DATA_TYPE_BOOLEAN"))) {
		type = UVC_CTRL_DATA_TYPE_BOOLEAN;
	}
	else if(xmlStrEqual(name, BAD_CAST("UVC_CTRL_DATA_TYPE_ENUM"))) {
		type = UVC_CTRL_DATA_TYPE_ENUM;
	}
	else if(xmlStrEqual(name, BAD_CAST("UVC_CTRL_DATA_TYPE_BITMASK"))) {
		type = UVC_CTRL_DATA_TYPE_BITMASK;
	}
	return type;
}


/**
 * Converts the name of a V4L2 data type constant into its corresponding numerical value.
 *
 * Note that not all V4L2 data types are recognized. Only the ones relevant for libwebcam
 * and allowed by the schema are considered valid.
 *
 * @param name		data type string to be converted. Can be NULL.
 *
 * @return
 * 		- 0 if the conversion failed, i.e. the data type name was not recognized
 * 		- a V4L2_CTRL_TYPE_* constant if the conversion was successful
 */
static enum v4l2_ctrl_type get_v4l2_ctrl_type_by_name (const xmlChar *name)
{
	enum v4l2_ctrl_type type = 0;	// Used to denote an invalid/unsupported V4L2 control type 
	if(!name) return type;

	if(xmlStrEqual(name, BAD_CAST("V4L2_CTRL_TYPE_INTEGER"))) {
		type = V4L2_CTRL_TYPE_INTEGER;
	}
	else if(xmlStrEqual(name, BAD_CAST("V4L2_CTRL_TYPE_BOOLEAN"))) {
		type = V4L2_CTRL_TYPE_BOOLEAN;
	}
#ifdef ENABLE_RAW_CONTROLS
	else if(xmlStrEqual(name, BAD_CAST("V4L2_CTRL_TYPE_STRING"))) {
		type = V4L2_CTRL_TYPE_STRING;
	}
#endif
	/*
	else if(xmlStrEqual(name, BAD_CAST("V4L2_CTRL_TYPE_MENU"))) {
		type = V4L2_CTRL_TYPE_MENU;
	}
	else if(xmlStrEqual(name, BAD_CAST("V4L2_CTRL_TYPE_BUTTON"))) {
		type = V4L2_CTRL_TYPE_BUTTON;
	}
	else if(xmlStrEqual(name, BAD_CAST("V4L2_CTRL_TYPE_INTEGER64"))) {
		type = V4L2_CTRL_TYPE_INTEGER64;
	}
	*/
	return type;
}


/**
 * Normalizes a string in terms of whitespace.
 *
 * The function returns a copy of the input string with leading and trailing whitespace
 * removed and all internal whitespace reduced to simple spaces. Examples:
 * 		" text  "							=> "text"
 * 		" Multi\nline text"					=> "Multi line text"
 *
 * This function allocates a new buffer that needs to be freed by the caller.
 *
 * @param input		input string to be normalized. This string is not modified.
 *
 * @return
 * 		- NULL if @a input is NULL or if memory could not be allocated
 * 		- a newly allocated buffer containing the output string
 */
static char *normalize_string (const char *input)
{
	const char *whitespace = " \t\v\n\r\f";

	if(!input) return NULL;

	// Skip whitespace at the beginning
	input += strspn(input, whitespace);

	// Allocate a new buffer for the normalized string
	unsigned int input_length = strlen(input) + 1;
	char *output = (char *)malloc(input_length);
	if(!output) return NULL;
	memset(output, 0, input_length);

	// Filter extra whitespace
	char *op = output;
	while(*input) {
		if(isspace(*input)) {
			while(*++input && isspace(*input));
			if(!*input) break;
			*op++ = ' ';
		}
		*op++ = *input++;
	}

	return output;
}


/**
 * Converts a UTF-8 string to ASCII.
 *
 * The function may convert some characters in a non-reversible way.
 *
 * @param unicode	input string to be converted.
 * @param ctx		current parse context
 *
 * @return
 * 		- NULL if the input buffer is NULL or an error occurs
 * 		- a copy of the input string if there is no iconv conversion descriptor
 * 		- a newly allocated buffer containing only ASCII characters
 */
static char *unicode_to_ascii (const xmlChar *unicode, ParseContext *ctx)
{
	if(!unicode) return NULL;
	assert(ctx && ctx->cd && ctx->cd != (iconv_t)-1);

	// If there is no conversion descriptor return a copy of the input string
	if(!ctx || !ctx->cd || ctx->cd == (iconv_t)-1) return strdup((char *)unicode);

	// Allocate a new buffer as big as the input string
	char *inbuf, *outbuf, *ascii;
	size_t unicode_bytes, ascii_bytes;
	inbuf = (char *)unicode;
	ascii_bytes = unicode_bytes = strlen(inbuf) + 1;
	ascii = (char *)malloc(ascii_bytes);
	memset(ascii, 0, sizeof(*ascii));
	outbuf = ascii;

	// Do the conversion
	if(iconv(ctx->cd, &inbuf, &unicode_bytes, &outbuf, &ascii_bytes) == -1) {
		assert(0);
		free(ascii);
		return NULL;
	}
	return ascii;
}


/**
 * Converts a UTF-8 string to ASCII and then normalizes the string's whitespace.
 *
 * This function is effectively a combination of unicode_to_ascii() and normalize_string().
 * Note that the caller must free the returned string.
 */
static char *unicode_to_normalized_ascii (const xmlChar *unicode, ParseContext *ctx)
{
	char *ascii = unicode_to_ascii(unicode, ctx);
	char *normalized = normalize_string(ascii);
	free(ascii);
	return normalized;
}



/*
 * XML/libxml2 helper functions
 */

/**
 * Returns the text content of the given node.
 *
 * @param node	Pointer to a node. May be NULL.
 *
 * @return
 * 		- the content of the first text child node if it exists
 * 		- NULL if a node in the path does not exist or does not contain text
 *
 */
static const xmlChar *xml_get_node_text (const xmlNode *node)
{
	if(node && node->children && xmlNodeIsText(node->children)) {
		return node->children->content;
	}
	else {
		return NULL;
	}
}


/**
 * Returns the first child node with a given name.
 *
 * @param node	pointer to the parent node whose children are to be searched
 * @param name	the name of the child node to be searched for
 *
 * @return
 * 		- a pointer to the first child node with the given name
 * 		- NULL if no child node has the given name
 */
static xmlNode *xml_get_first_child_by_name (const xmlNode *node, const char *name)
{
	xmlNode *cnode = node->children;
	while(cnode) {
		if(cnode->type == XML_ELEMENT_NODE && xmlStrEqual(cnode->name, BAD_CAST(name)))
			return cnode;
		cnode = cnode->next;
	}
	return NULL;
}


/**
 * Returns the next sibling node with a given name.
 *
 * @param node	pointer to the node whose siblings are to be searched
 * @param name	the name of the sibling node to be searched for
 *
 * @return
 * 		- a pointer to the first sibling node with the given name
 * 		- NULL if no sibling node has the given name
 */
static xmlNode *xml_get_next_sibling_by_name (const xmlNode *node, const char *name)
{
	xmlNode *cnode = node->next;
	while(cnode) {
		if(cnode->type == XML_ELEMENT_NODE && xmlStrEqual(cnode->name, BAD_CAST(name)))
			return cnode;
		cnode = cnode->next;
	}
	return NULL;
}



/*
 * Data management and lookup functions
 */

/**
 * Look up a constant by its name.
 *
 * @param ctx		current parse context
 * @param find_name		name of the constant to look up
 * @param find_type		only search for constants with this type. Specify CT_INVALID to
 * 						disable the type filter.
 *
 * @return
 * 		- NULL if no constant with the given name (and given type) could be found
 * 		- a pointer to the constant if the search was successful
 */
static Constant *lookup_constant (const char *find_name, ConstantType find_type, ParseContext *ctx)
{
	Constant *elem = ctx->constants;
	while(elem) {
		if(strcmp(elem->name, find_name) == 0
				&& (find_type == CT_INVALID ? 1 : elem->type == find_type))
			return elem;
		elem = elem->next;
	}
	return NULL;
}


/**
 * Convert the given string to an integer or look up a constant with the given name.
 *
 * The function first tries to convert the given string to an integer. If successful,
 * the converted value is returned. If the conversion fails, the string is interpreted
 * as a name and a constant with that name is looked up. If the lookup is successful,
 * the value of the constant is returned.
 *
 * @param text		pointer to the name or integer value. Can be NULL.
 * @param value		pointer to an integer to receive the converted or constant value
 * @param ctx		current parse context
 *
 * @return
 * 		- C_SUCCESS if the string was successfully converted to an integer
 * 		- C_SUCCESS if the string was not converted but a constant was found
 * 		- C_PARSE_ERROR if @a text is NULL, or if both conversion and lookup fail
 */
static CResult lookup_or_convert_to_integer (const xmlChar *text, int *value, ParseContext *ctx)
{
	if(!text) return C_PARSE_ERROR;

	if(!is_valid_integer_string((const char *)text, value)) {
		Constant *constant = lookup_constant((const char *)text, CT_INTEGER, ctx);
		if(constant) {
			*value = constant->value;
		}
		else {
			return C_PARSE_ERROR;
		}
	}

	return C_SUCCESS;
}


/**
 * Convert the given string to a GUID or look up a constant with the given name.
 *
 * This function works like lookup_or_convert_to_integer() except that it looks for GUIDs
 * instead of integers.
 */
static CResult lookup_or_convert_to_guid (const xmlChar *text, __u8 *guid, ParseContext *ctx)
{
	if(!text) return C_PARSE_ERROR;

	if(is_valid_guid((const char *)text)) {
		guid_to_byte_array((const char *)text, guid);
	}
	else {
		Constant *constant = lookup_constant((const char *)text, CT_GUID, ctx);
		if(constant) {
			memcpy(guid, constant->guid, GUID_SIZE);
		}
		else {
			return C_PARSE_ERROR;
		}
	}

	return C_SUCCESS;
}


/**
 * Lookup a UVC extension unit control with the given name.
 */
static UVCXUControl * lookup_control (const xmlChar *name, ParseContext *ctx)
{
	UVCXUControl *elem = ctx->controls;
	while(elem) {
		if(xmlStrEqual(elem->id, name))
			return elem;
		elem = elem->next;
	}
	return NULL;
}


/**
 * Reorganize the buffer that is used to store the messages that are logged during parsing.
 *
 * The buffer that is used to store the messages is completely self-contained. It consists
 * of an array of CDynctrlMessage structures and an area for "dynamics" which stores
 * dynamic data like strings. All string pointers in the array point to strings in the
 * dynamics area, so for clean up only a single buffer needs to be freed.
 *
 * Every time a new array element is added, the buffer has to be enlarged, and a new array
 * element has to be inserted between the current last element and the dynamics area.
 * The dynamics area is moved by sizeof(CDynctrlMessage) bytes and all pointers to it
 * are updated to point to the new direction.
 *
 * @param ctx	current parse context
 * @param msg	pointer to the new message to be added. Note that this is only a temporary
 * 				buffer. Its content (including its strings) is copied into the message
 * 				buffer and the temporary buffer is no longer required after the function
 * 				returns.
 *
 * @return
 * 		- C_NO_MEMORY if the buffer could not be enlarged for lack of memory
 * 		- C_SUCCESS otherwise
 */
static CResult resize_message_buffer (ParseContext *ctx, CDynctrlMessage *msg)
{
	CDynctrlMessage *new_messages = NULL;

	// Enlarge the CDynctrlMessage buffer and zero out the newly allocated space
	unsigned int extra_required = sizeof(CDynctrlMessage) + strlen(msg->text) + 1;
	new_messages = (CDynctrlMessage *)realloc(ctx->info->messages, ctx->messages_size + extra_required);
	if(new_messages == NULL) return C_NO_MEMORY;
	memset((unsigned char *)new_messages + ctx->messages_size, 0, extra_required);
	ctx->messages_size += extra_required;

	// Move the dynamics area (e.g. all strings) down (imagine ctx->messages as the top)
	// by sizeof(CDynctrlMessage) to make room for the new array element.
	// Note that to access the current .text string we already need to refer to the new
	// buffer (to the old location in the new buffer to be more specific) because the buffer
	// has been freed above.
	int i, dynamics_length = 0;
	for(i = 0; i < ctx->info->message_count; i++) {
		char *old_text = (char *)new_messages + ctx->info->message_count * sizeof(CDynctrlMessage) + dynamics_length;
		dynamics_length += strlen(old_text) + 1;
		// Note that the .text pointers will be invalid until we actually move the data below
		new_messages[i].text = old_text + sizeof(CDynctrlMessage);
	}
	if(i > 0) {
		memmove((unsigned char *)new_messages + (ctx->info->message_count + 1) * sizeof(CDynctrlMessage),
				(unsigned char *)new_messages + ctx->info->message_count * sizeof(CDynctrlMessage),
				dynamics_length);
	}

	// Copy the string of the new message to the end of the dynamics area
	char *new_text = (char *)((unsigned char *)new_messages + (ctx->info->message_count + 1) * sizeof(CDynctrlMessage) + dynamics_length);
	strcpy(new_text, msg->text);
	msg->text = new_text;		// Update the message to point to the new text

	// Copy the new message to the end of the array and update the array size
	ctx->info->message_count++;
	memcpy(&new_messages[ctx->info->message_count - 1], msg, sizeof(CDynctrlMessage));

	// Finally, update the context to point to the resized messages buffer
	ctx->info->messages = new_messages;

	return C_SUCCESS;
}


/**
 * Adds a new message to the message list.
 *
 * This function works like add_message() but takes a @a va_list argument instead
 * of a parameter list.
 */
static CResult add_message_v (ParseContext *ctx, int line, int col, CDynctrlMessageSeverity severity, const char *format, va_list va)
{
	CResult ret = C_SUCCESS;

	if(!ctx->info)
		return C_INVALID_ARG;
	if(!(ctx->info->flags & CD_REPORT_ERRORS))
		return C_SUCCESS;

	// Allocate a temporary buffer that contains the final message string
	char *text = NULL;
	int written = vasprintf(&text, format, va);
	if(written == -1) {
		ret = C_NO_MEMORY;
		goto done;
	}

	// Allocate memory for the new message (i.e. CDynctrlMessage and its string)
	CDynctrlMessage message = { 0 };
	message.line		= line;
	message.col			= col;
	message.severity	= severity;
	message.text		= text;
	ret = resize_message_buffer(ctx, &message);
	if(ret) {
		ret = C_NO_MEMORY;
		goto done;
	}

done:
	if(text) free(text);
	return ret;
}


/**
 * Adds a new message to the message list.
 *
 * @param ctx		current parse context
 * @param line		line number that the message concerns (e.g. line of the syntax error)
 * @param col		column number that the message concerns
 * @param severity	severity level of the message (warning, error, etc.)
 * @param format	a printf compatible format to print the variable parameter list
 */
static CResult add_message (ParseContext *ctx, int line, int col, CDynctrlMessageSeverity severity, const char *format, ...)
{
	va_list va;

	va_start(va, format);
	CResult ret = add_message_v(ctx, line, col, severity, format, va);
	va_end(va);

	return ret;
}


/**
 * Adds a new informational message to the message list.
 */
static CResult add_info (ParseContext *ctx, const char *format, ...)
{
	va_list va;

	va_start(va, format);
	CResult ret = add_message_v(ctx, 0, 0, CD_SEVERITY_INFO, format, va);
	va_end(va);

	return ret;
}


/*
static CResult add_warning (ParseContext *ctx, const char *format, ...)
{
	va_list va;

	va_start(va, format);
	CResult ret = add_message_v(ctx, 0, 0, CD_SEVERITY_WARNING, format, va);
	va_end(va);

	return ret;
}
*/


/**
 * Adds a new error message to the message list.
 */
static CResult add_error (ParseContext *ctx, const char *format, ...)
{
	va_list va;

	va_start(va, format);
	CResult ret = add_message_v(ctx, 0, 0, CD_SEVERITY_ERROR, format, va);
	va_end(va);

	return ret;
}


/**
 * Adds a new error message concerning a given xmlNode to the message list.
 *
 * The node is used to extract the line number.
 */
static CResult add_error_at_node (ParseContext *ctx, const xmlNode *node, const char *format, ...)
{
	assert(node);
	va_list va;

	va_start(va, format);
	CResult ret = add_message_v(ctx, node->line, 0, CD_SEVERITY_ERROR, format, va);
	va_end(va);

	return ret;
}



/*
 * Parsing functions
 */

/**
 * Parse a dynamic controls configuration XML file and return an XML document tree.
 *
 * Note that the pointer stored in @a *xml_doc must be freed using xmlFreeDoc() if
 * the return value of this functino is C_SUCCESS.
 *
 * @param file_name		name (with an optional path) of the file to be parsed
 * @param xml_doc		address of a pointer to store the XML document tree
 * @param ctx		current parse context
 *
 * @return
 * 		- C_NO_MEMORY if a buffer or structure could not be allocated
 * 		- C_PARSE_ERROR if the XML file is malformed
 * 		- C_SUCCESS if parsing was successful
 */
static CResult parse_dynctrl_file (const char *file_name, xmlDoc **xml_doc, ParseContext *ctx)
{
	CResult ret = C_SUCCESS;
	xmlParserCtxt *parser = NULL;
	xmlDoc *doc = NULL;
	assert(xml_doc);

	parser = xmlNewParserCtxt();
	if(!parser)
		return C_NO_MEMORY;
	
	// Read and parse the XML file
	doc = xmlCtxtReadFile(parser, file_name, NULL, XML_PARSE_NOBLANKS);
	if(!doc) {
		xmlError *e = &parser->lastError;
		add_message(ctx, e->line, e->int2, CD_SEVERITY_ERROR,
				"Malformed control mapping file encountered. Unable to parse: %s",
				e->message);
		ret = C_PARSE_ERROR;
	}
	assert(parser->valid);

	// Validate the XML file against the schema
	if(!ctx->info || !(ctx->info->flags & CD_DONT_VALIDATE)) {
		// TODO implement
	}

	// Free the document tree if there was an error
	if(ret) {
		xmlFreeDoc(doc);
		doc = NULL;
	}

	// Return the document (or NULL if there was an error)
	*xml_doc = doc;
	
	// Clean up
	xmlFreeParserCtxt(parser);

	return ret;
}


/**
 * Process a @c mapping node and add the contained mapping to the UVC driver.
 */
static CResult process_mapping (const xmlNode *node_mapping, ParseContext *ctx)
{
	CResult ret;
	assert(node_mapping);

	struct uvc_xu_control_mapping mapping_info = { 0 };

	// At the moment only V4L2 mappings are supported
	xmlNode *node_v4l2 = xml_get_first_child_by_name(node_mapping, "v4l2");
	if(!node_v4l2) {
		// TODO implement
		return C_NOT_IMPLEMENTED;
	}

	// Search for the node containing UVC information
	xmlNode *node_uvc = xml_get_first_child_by_name(node_mapping, "uvc");
	if(!node_uvc) {
		add_error_at_node(ctx, node_mapping,
			"Mapping does not have UVC information. <uvc> is mandatory.");
		return C_PARSE_ERROR;
	}

	// Look up the referenced control definition and fill in the UVC fields of
	// the uvc_xu_control_mapping structure.
	xmlNode *node_control_ref = xml_get_first_child_by_name(node_uvc, "control_ref");
	if(!node_control_ref) {
		add_error_at_node(ctx, node_uvc,
			"Control reference missing. <control_ref> is mandatory.");
		return C_PARSE_ERROR;
	}
	xmlChar *control_ref = xmlGetProp(node_control_ref, BAD_CAST("idref"));
	if(!control_ref) {
		add_error_at_node(ctx, node_control_ref,
			"Invalid control reference. 'idref' attribute referencing a <control> is mandatory.");
		return C_PARSE_ERROR;
	}
	UVCXUControl *control = lookup_control(control_ref, ctx);
	if(!control) {
		add_error_at_node(ctx, node_control_ref,
			"Invalid control reference: control with ID '%s' could not be found.", (char *)control_ref);
		xmlFree(control_ref);
		return C_PARSE_ERROR;
	}
	xmlFree(control_ref); control_ref = NULL;
	memcpy(mapping_info.entity, control->info.entity, GUID_SIZE);
	mapping_info.selector = control->info.selector;

	// Copy the descriptive name (truncate if it's too long for V4L2/uvcvideo)
	char *name = UNICODE_TO_NORM_ASCII(xml_get_node_text(xml_get_first_child_by_name(node_mapping, "name")));
	if(!name) {
		add_error_at_node(ctx, node_mapping,
			"Control mapping has no name. <name> is mandatory.");
		return C_PARSE_ERROR;
	}
	strncpy((char *)mapping_info.name, name, sizeof(mapping_info.name) - 1);
	mapping_info.name[sizeof(mapping_info.name) - 1] = '\0';
	free(name);
	name = NULL;

	// Fill in the V4L2 fields of the uvc_xu_control_mapping structure
	int value = 0;
	const xmlChar *text = xml_get_node_text(xml_get_first_child_by_name(node_v4l2, "id"));
	ret = lookup_or_convert_to_integer(text, &value, ctx);
	if(ret) {
		add_error_at_node(ctx, node_v4l2,
			"V4L2 ID contains invalid number or references unknown constant: '%s'",
			text ? (char *)text : "<empty>");
		return C_PARSE_ERROR;
	}
	mapping_info.id = (__u32)value;
	text = xml_get_node_text(xml_get_first_child_by_name(node_v4l2, "v4l2_type"));
	enum v4l2_ctrl_type v4l2_type = get_v4l2_ctrl_type_by_name(text);
	if(v4l2_type == 0) {
		add_error_at_node(ctx, node_v4l2,
			"Invalid V4L2 control type specified: '%s'",
			text ? (char *)text : "<empty>");
		return C_PARSE_ERROR;
	}
	mapping_info.v4l2_type = v4l2_type;

	// Fill in the remaining UVC fields of the uvc_xu_control_mappings structure
	char *string = UNICODE_TO_ASCII(xml_get_node_text(xml_get_first_child_by_name(node_uvc, "size")));
	if(!is_valid_size_string(string, &value, 0xFF)) {
		add_error_at_node(ctx, node_v4l2,
			"Invalid UVC control size specified: '%s'",
			string ? string : "<empty>");
		return C_PARSE_ERROR;
	}
	mapping_info.size = value;
	free(string);

	string = UNICODE_TO_ASCII(xml_get_node_text(xml_get_first_child_by_name(node_uvc, "offset")));
	if(!is_valid_size_string(string, &value, 0xFF)) {
		add_error_at_node(ctx, node_v4l2,
			"Invalid UVC control offset specified: '%s'",
			string ? string : "<empty>");
		return C_PARSE_ERROR;
	}
	mapping_info.offset = value;
	free(string);

	text = xml_get_node_text(xml_get_first_child_by_name(node_uvc, "uvc_type"));
	enum uvc_control_data_type uvc_type = get_uvc_ctrl_type_by_name(xml_get_node_text(xml_get_first_child_by_name(node_uvc, "uvc_type")));
	if(uvc_type == -1) {
		add_error_at_node(ctx, node_v4l2,
			"Invalid UVC control type specified: '%s'",
			text ? (char *)text : "<empty>");
		return C_PARSE_ERROR;
	}
	mapping_info.data_type = uvc_type;

	// Add the control to the UVC driver's control list
	/*
	printf(
		"uvc_xu_control_mapping = {\n"
		"	id        = 0x%08x,\n"
		"	name      = '%s',\n"
		"	entity    = {" GUID_FORMAT "},\n"
		"	selector  = %u,\n"
		"	size      = %u,\n"
		"	offset    = %u,\n"
		"	v4l2_type = %u,\n"
		"	data_type = %u\n"
		"}\n",
		mapping_info.id,
		mapping_info.name,
		GUID_ARGS(mapping_info.entity),
		mapping_info.selector,
		mapping_info.size,
		mapping_info.offset,
		mapping_info.v4l2_type,
		mapping_info.data_type
	);
	*/
	int v4l2_ret = ioctl(ctx->v4l2_handle, UVCIOC_CTRL_MAP, &mapping_info);
	if(v4l2_ret != 0
#ifdef DYNCTRL_IGNORE_EEXIST_AFTER_PASS1
			&& (ctx->pass == 1 || errno != EEXIST)
#endif
		)
	{
		add_error(ctx,
			"%s: unable to map '%s' control. ioctl(UVCIOC_CTRL_MAP) failed with return value %d (error %d: %s)",
			GET_HANDLE(ctx->handle).device->v4l2_name,
			mapping_info.name, v4l2_ret, errno, strerror(errno));
		return C_V4L2_ERROR;
	}

	return C_SUCCESS;
}


/**
 * Process a @c mappings node.
 */
static CResult process_mappings (const xmlNode *node_mappings, ParseContext *ctx)
{
	assert(node_mappings);

	// Process all <mapping> nodes
	xmlNode *node_mapping = xml_get_first_child_by_name(node_mappings, "mapping");
	while(node_mapping) {
		CResult ret = process_mapping(node_mapping, ctx);
		if(ctx->info) {
			if(ret)
				ctx->info->stats.mappings.successful++;
			else
				ctx->info->stats.mappings.failed++;
		}
		node_mapping = xml_get_next_sibling_by_name(node_mapping, "mapping");
	}

	return C_SUCCESS;
}


/**
 * Process a @c control node by adding the contained control to the UVC driver.
 */
static CResult process_control (xmlNode *node_control, ParseContext *ctx)
{
	CResult ret = C_SUCCESS;
	assert(node_control);

	const xmlChar *text = NULL;
	int value = 0;

	// Allocate memory for the extension unit control definition
	UVCXUControl *xu_control = (UVCXUControl *)malloc(sizeof(UVCXUControl));
	if(!xu_control)
		return C_NO_MEMORY;
	memset(xu_control, 0, sizeof *xu_control);

	// Get the ID of the extension unit control definition
	xu_control->id = xmlGetProp(node_control, BAD_CAST("id"));
	if(!xu_control->id) {
		add_error_at_node(ctx, node_control,
			"Control has no ID. 'id' attribute is mandatory.");
		ret = C_PARSE_ERROR;
		goto done;
	}

	// Retrieve the entity and check whether it's a constant or a GUID
	text = xml_get_node_text(xml_get_first_child_by_name(node_control, "entity"));
	ret = lookup_or_convert_to_guid(text, xu_control->info.entity, ctx);
	if(ret) {
		add_error_at_node(ctx, node_control,
			"Control entity contains invalid GUID or references unknown constant: '%s'",
			text ? (char *)text : "<empty>");
		goto done;
	}

	// Retrieve the selector and check whether it's a constant or a GUID
	text = xml_get_node_text(xml_get_first_child_by_name(node_control, "selector"));
	ret = lookup_or_convert_to_integer(text, &value, ctx);
	if(ret) {
		add_error_at_node(ctx, node_control,
			"Control selector contains invalid number or references unknown constant: '%s'",
			text ? (char *)text : "<empty>");
		goto done;
	}
	xu_control->info.selector = (__u8)value;

	// Retrieve the index
	text = xml_get_node_text(xml_get_first_child_by_name(node_control, "index"));
	ret = lookup_or_convert_to_integer(text, &value, ctx);
	if(ret) {
		add_error_at_node(ctx, node_control,
			"Invalid control index specified: '%s'", text ? (char *)text : "<empty>");
		goto done;
	}
	xu_control->info.index = (__u8)value;

	// Retrieve the size
	text = xml_get_node_text(xml_get_first_child_by_name(node_control, "size"));
	ret = lookup_or_convert_to_integer(text, &value, ctx);
	if(ret || !is_valid_size(value, 0xFFFF)) {
		add_error_at_node(ctx, node_control,
			"Invalid control size specified: '%s'", text ? (char *)text : "<empty>");
		goto done;
	}
	xu_control->info.size = (__u16)value;

	// Retrieve the list of supported requests
	xmlNode *node_requests = xml_get_first_child_by_name(node_control, "requests");
	if(!node_requests) {
		add_error_at_node(ctx, node_control,
			"List of supported UVC requests missing. <requests> is mandatory.");
		ret = C_PARSE_ERROR;
		goto done;
	}
	xmlNode *node_request = xml_get_first_child_by_name(node_requests, "request");
	while(node_request) {
		text = xml_get_node_text(node_request);
		__u32 flag = get_uvc_request_by_name(text);
		if(!flag) {
			add_error_at_node(ctx, node_request,
				"Invalid UVC request specified: '%s'", text ? (char *)text : "<empty>");
		}
		xu_control->info.flags |= flag;
		node_request = xml_get_next_sibling_by_name(node_request, "request");
	}

	// Disable the UVC driver's caching mechanism for XU controls
	xu_control->info.flags |= UVC_CTRL_FLAG_AUTO_UPDATE;

	// Add the control to the UVC driver's control list
	/*
	printf(
		"uvc_xu_control_info = {\n"
		"	entity   = {" GUID_FORMAT "},\n"
		"	index    = %u,\n"
		"	selector = %u,\n"
		"	size     = %u,\n"
		"	flags    = %u\n"
		"}\n",
		GUID_ARGS(xu_control->info.entity),
		xu_control->info.index,
		xu_control->info.selector,
		xu_control->info.size,
		xu_control->info.flags
	);
	*/
	int v4l2_ret = ioctl(ctx->v4l2_handle, UVCIOC_CTRL_ADD, &xu_control->info);
	if(v4l2_ret != 0
#ifdef DYNCTRL_IGNORE_EEXIST_AFTER_PASS1
			&& (ctx->pass == 1 || errno != EEXIST)
#endif
		)
	{
		add_error(ctx,
			"%s: unable to add control with GUID {"GUID_FORMAT"} and selector %d. "
			"ioctl(UVCIOC_CTRL_ADD) failed with return value %d (error %d: %s)",
			GET_HANDLE(ctx->handle).device->v4l2_name,
			GUID_ARGS(xu_control->info.entity), xu_control->info.selector,
			v4l2_ret, errno, strerror(errno));
		ret = C_V4L2_ERROR;
	}

	// Add the extension unit control definition to the internal list for later
	// reference by mappings.
	// Note that we add controls to the list whether or not the UVCIOC_CTRL_ADD above
	// succeeded or not. The control might already exist in the driver but a referencing
	// mapping might not, but the mapping needs to be able to look up the control.
	xu_control->next = ctx->controls;
	ctx->controls = xu_control;

done:
	if(xu_control && xu_control != ctx->controls) {	// Only free xu_control if it was not added
		if(xu_control->id)							// to the list.
			free(xu_control->id);
		free(xu_control);
	}
	return ret;
}


/**
 * Process a @c controls node.
 */
static CResult process_controls (const xmlNode *node_controls, ParseContext *ctx)
{
	assert(node_controls);

	// Process all <control> nodes
	xmlNode *node_control = xml_get_first_child_by_name(node_controls, "control");
	while(node_control) {
		CResult ret = process_control(node_control, ctx);
		if(ctx->info) {
			if(ret)
				ctx->info->stats.controls.successful++;
			else
				ctx->info->stats.controls.failed++;
		}
		node_control = xml_get_next_sibling_by_name(node_control, "control");
	}

	return C_SUCCESS;
}


/**
 * Process a @c device node.
 *
 * Note that the contained @c match sections are currently ignored.
 */
static CResult process_device (const xmlNode *node_device, ParseContext *ctx)
{
	assert(node_device);

	// Process the <controls> node
	xmlNode *node_controls = xml_get_first_child_by_name(node_device, "controls");
	if(node_controls)
		process_controls(node_controls, ctx);

	return C_SUCCESS;
}


/**
 * Process a @c devices node.
 */
static CResult process_devices (const xmlNode *node_devices, ParseContext *ctx)
{
	assert(node_devices);

	// Process all <device> nodes
	xmlNode *node_device = xml_get_first_child_by_name(node_devices, "device");
	while(node_device) {
		process_device(node_device, ctx);
		node_device = xml_get_next_sibling_by_name(node_device, "device");
	}

	return C_SUCCESS;
}


/**
 * Process a @c constant node by adding the contained constant to an internal list.
 */
static CResult process_constant (xmlNode *node_constant, ParseContext *ctx)
{
	CResult ret = C_SUCCESS;
	xmlChar *type = NULL;

	assert(node_constant);

	// Allocate memory for the constant list element
	Constant *constant = (Constant *)malloc(sizeof(Constant));
	if(!constant) return C_NO_MEMORY;
	memset(constant, 0, sizeof(*constant));

	// Read and convert the name
	constant->name = UNICODE_TO_ASCII(xml_get_node_text(xml_get_first_child_by_name(node_constant, "id")));
	if(!constant->name) {
		add_error_at_node(ctx, node_constant, "Constant has no name. <id> is mandatory.");
		ret = C_PARSE_ERROR;
		goto done;
	}
	if(lookup_constant(constant->name, CT_INVALID, ctx)) {
		add_error_at_node(ctx, node_constant,
			"Constant '%s' has already been defined. Ignoring redefinition.", constant->name);
		ret = C_PARSE_ERROR;
		goto done;
	}

	// Read the type of the constant
	type = xmlGetProp(node_constant, BAD_CAST("type"));
	if(xmlStrEqual(type, BAD_CAST("integer"))) {
		constant->type = CT_INTEGER;
	}
	else if(xmlStrEqual(type, BAD_CAST("guid"))) {
		constant->type = CT_GUID;
	}

	// Read the value of the constant
	const xmlNode *node_value;
	const xmlChar *value = xml_get_node_text(node_value = xml_get_first_child_by_name(node_constant, "value"));
	switch(constant->type) {
		case CT_INTEGER:
			if(!is_valid_integer_string((char *)value, &constant->value)) {
				add_error_at_node(ctx, node_value,
					"Integer constant %s has invalid value '%s'.", constant->name, (char *)value);
				ret = C_PARSE_ERROR;
				goto done;
			}
			break;

		case CT_GUID:
			guid_to_byte_array((char *)value, constant->guid);
			break;

		default:
			add_error_at_node(ctx, node_constant,
				"Constant has unknown type '%s' (must be 'integer' or 'guid').", (char *)type);
			ret = C_PARSE_ERROR;
			goto done;
			break;
	}

	// Add the constant to the internal list for later reference
	constant->next = ctx->constants;
	ctx->constants = constant;

done:
	// Clean up
	if(type)
		xmlFree(type);
	if(constant != ctx->constants)
		free(constant);
	
	return ret;
}


/**
 * Process a @c constants node.
 */
static CResult process_constants (const xmlNode *node_constants, ParseContext *ctx)
{
	assert(node_constants);

	// Process all <constant> nodes
	xmlNode *node_constant = xml_get_first_child_by_name(node_constants, "constant");
	while(node_constant) {
		CResult ret = process_constant(node_constant, ctx);
		if(ctx->info) {
			if(ret)
				ctx->info->stats.constants.successful++;
			else
				ctx->info->stats.constants.failed++;
		}
		node_constant = xml_get_next_sibling_by_name(node_constant, "constant");
	}

	return C_SUCCESS;
}


/**
 * Process a @c meta node by filling in the corresponding info structure.
 */
static CResult process_meta (const xmlNode *node_meta, ParseContext *ctx)
{
	assert(node_meta);

	// Extract meta information if required
	if(ctx->info && ctx->info->flags & CD_RETRIEVE_META_INFO) {
		// Copy the version and revision numbers
		string_to_version(
				(char *)xml_get_node_text(xml_get_first_child_by_name(node_meta, "version")),
				&ctx->info->meta.version.major, &ctx->info->meta.version.minor);
		string_to_version(
				(char *)xml_get_node_text(xml_get_first_child_by_name(node_meta, "revision")),
				&ctx->info->meta.revision.major, &ctx->info->meta.revision.minor);

		// Copy the strings for author (normalized), contact, and copyright
		ctx->info->meta.author = unicode_to_normalized_ascii(
				xml_get_node_text(xml_get_first_child_by_name(node_meta, "author")), ctx);
		ctx->info->meta.contact = unicode_to_ascii(
				xml_get_node_text(xml_get_first_child_by_name(node_meta, "contact")), ctx);
		ctx->info->meta.copyright = unicode_to_ascii(
				xml_get_node_text(xml_get_first_child_by_name(node_meta, "copyright")), ctx);
	}

	return C_SUCCESS;
}


/**
 * Process an XML document tree representing a dynamic controls configuration file.
 */
static CResult process_dynctrl_doc (xmlDoc *xml_doc, ParseContext *ctx)
{
	CResult ret = C_SUCCESS;
	if(!xml_doc)
		return C_INVALID_ARG;

	xmlNode *node_root = xmlDocGetRootElement(xml_doc);
	assert(node_root);
	ctx->pass++;	// We start at pass 1 ...

	// Some processing only needs to be done in the first pass
	if(ctx->pass == 1) {
		// Find and process the <meta> node
		ret = process_meta(xml_get_first_child_by_name(node_root, "meta"), ctx);
		if(ret) return ret;

		// Find the <constants> list (if it exists)
		ret = process_constants(xml_get_first_child_by_name(node_root, "constants"), ctx);
		if(ret) return ret;
	}

	// Process all <devices> lists
	xmlNode *node_devices = xml_get_first_child_by_name(node_root, "devices");
	while(node_devices) {
		process_devices(node_devices, ctx);
		node_devices = xml_get_next_sibling_by_name(node_devices, "devices");
	}

	// Process the <mappings> node
	ret = process_mappings(xml_get_first_child_by_name(node_root, "mappings"), ctx);

	return ret;
}


/**
 * Checks whether the driver behind the current device supports dynamic controls.
 *
 * The check is done by redefining the brightness control which is hardcoded in the
 * UVC driver. If the driver supports dynamic controls, it will return EEXIST.
 * If the driver does not support dynamic controls, the ioctl will fail with EINVAL.
 *
 * @param ctx		current parse context
 *
 * @return
 * 		- #C_SUCCESS if the driver supports dynamic controls
 * 		- #C_NOT_IMPLEMENTED if the driver does not support dynamic controls
 */
static CResult device_supports_dynctrl(ParseContext *ctx)
{
	CResult ret = C_SUCCESS;
	struct uvc_xu_control_info xu_control = {
		.entity		= /* UVC_GUID_UVC_PROCESSING */ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01 },
		.selector	= /* PU_BRIGHTNESS_CONTROL */ 0x02,
		.index		= 0,
		.size		= 0,
		.flags		= 0
	};
	
	assert(ctx->v4l2_handle);

	int v4l2_ret = ioctl(ctx->v4l2_handle, UVCIOC_CTRL_ADD, &xu_control);
	if(v4l2_ret == -1) {
		if(errno == EPERM) {
			/* User is not root (newer drivers require root permissions) */
			ret = C_CANNOT_WRITE;
		}
		else if(errno == EEXIST) {
			/* Driver supports dynamic controls */
		}
		else {
			/* Unexpected error: Assume not supported */
			ret = C_NOT_IMPLEMENTED;
		}
	}
	else {
		/* Success: Assume not supported */
		ret = C_NOT_IMPLEMENTED;
	}

	return ret;
}


/** 
 * Adds controls and control mappings contained in the given XML tree to the UVC driver.
 *
 * @param xml_doc	XML document tree corresponding to the dynctrl format
 * @param ctx		current parse context
 *
 * @return
 * 		- #C_INVALID_DEVICE if no devices are available
 * 		- #C_CANNOT_WRITE if the user does not have permissions to add the mappings
 * 		- #C_SUCCESS if adding the controls and control mappings was successful
 */
static CResult add_control_mappings (xmlDoc *xml_doc, ParseContext *ctx)
{
	CResult ret = C_SUCCESS;

	assert(HANDLE_OPEN(ctx->handle));
	assert(HANDLE_VALID(ctx->handle));

	// Open the V4L2 device
	ctx->v4l2_handle = open_v4l2_device(GET_HANDLE(ctx->handle).device->v4l2_name);
	if(!ctx->v4l2_handle) {
		ret = C_INVALID_DEVICE;
		goto done;
	}

	// Check if the driver supports dynamic controls
	ret = device_supports_dynctrl(ctx);
	if(ret) goto done;

	// Process the contained control mappings
	ret = process_dynctrl_doc(xml_doc, ctx);

done:
	// Close the device handle
	if(ctx && ctx->v4l2_handle) {
		close(ctx->v4l2_handle);
		ctx->v4l2_handle = 0;
	}

	return ret;
}



/*
 * API
 */

/**
 * Parses a dynamic controls configuration file and adds the contained controls and control
 * mappings to the UVC driver.
 *
 * Notes:
 * - Just because the function returns C_SUCCESS doesn't mean there were no errors.
 *   The dynamic controls parsing process tries to be very forgiving on syntax errors
 *   or if processing of a single control/mapping fails. Check the info->messages list
 *   for details after processing is done.
 * - If the @a info parameter is not NULL the caller must free the info->messages field
 *   if it is not NULL.
 * - Note that this function is not thread-safe.
 *
 * @param file_name		name of the device to open.
 * @param info			structure to pass operation flags and retrieve status information.
 * 						Can be NULL.
 *
 * @return
 * 		- #C_INIT_ERROR if the library has not been initialized
 * 		- #C_INVALID_DEVICE if no supported devices are available
 * 		- #C_NO_MEMORY if memory could not be allocated
 * 		- #C_SUCCESS if the parsing was successful and no fatal error occurred
 * 		- #C_NOT_IMPLEMENTED if libwebcam was compiled with dynctrl support disabled
 */
CResult c_add_control_mappings_from_file (const char *file_name, CDynctrlInfo *info)
{
	CResult ret = C_SUCCESS;
	CDevice *devices = NULL;
	ParseContext *ctx = NULL;
	xmlDoc *xml_doc = NULL;

	if(!initialized)
		return C_INIT_ERROR;
	if(!file_name)
		return C_INVALID_ARG;
	
	// Enumerate the devices and abort if there are no devices present
	unsigned int size = 0, device_count = 0;
	ret = c_enum_devices(NULL, &size, &device_count);
	if(ret == C_SUCCESS) {
		// Our zero buffer was large enough, so no devices are present
		return C_INVALID_DEVICE;
	}
	else if(ret != C_BUFFER_TOO_SMALL) {
		// Something bad has happened, so bail out
		return ret;
	}
	assert(device_count > 0);
	devices = (CDevice *)malloc(size);
	ret = c_enum_devices(devices, &size, &device_count);
	if(ret) goto done;

	// Allocate memory for the parsing context
	ctx = (ParseContext *)malloc(sizeof(ParseContext));
	if(!ctx) {
		ret = C_NO_MEMORY;
		goto done;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->info = info;

	// Parse the dynctrl configuration file
	ret = parse_dynctrl_file(file_name, &xml_doc, ctx);
	if(ret) goto done;

	// Allocate a conversion descriptor
	ctx->cd = iconv_open("ASCII", "UTF-8");
	assert(ctx->cd != (iconv_t)-1);

	// Loop through the devices and check which ones have a supported uvcvideo driver behind them
	int i, successful_devices = 0;
	for(i = 0; i < device_count; i++) {
		CDevice *device = &devices[i];

		// Skip non-UVC devices
		if(strcmp(device->driver, "uvcvideo") != 0) {
			add_info(ctx,
				"device '%s' skipped because it is not a UVC device.",
				device->shortName
			);
			continue;
		}

		// Create a device handle
		ctx->handle = c_open_device(device->shortName);
		if(!ctx->handle) {
			add_error(ctx,
				"device '%s' skipped because it could not be opened.",
				device->shortName
			);
			continue;
		}

		// Add the parsed control mappings to this device
		ret = add_control_mappings(xml_doc, ctx);
		if(ret == C_SUCCESS) {
			successful_devices++;
		}
		else if(ret == C_NOT_IMPLEMENTED) {
			add_error(ctx,
				"device '%s' skipped because the driver '%s' behind it does not seem "
				"to support dynamic controls.",
				device->shortName, device->driver
			);
		}
		else if(ret == C_CANNOT_WRITE) {
			add_error(ctx,
				"device '%s' skipped because you do not have the right permissions. "
				"Newer driver versions require root permissions.",
				device->shortName
			);
		}
		else {
			char *error = c_get_handle_error_text(ctx->handle, ret);
			assert(error);
			add_error(ctx,
				"device '%s' was not processed successfully: %s. (Code: %d)",
				device->shortName, error, ret
			);
			free(error);
		}

		// Close the device handle
		c_close_device(ctx->handle);
		ctx->handle = 0;
	}
	if(successful_devices == 0)
		ret = C_INVALID_DEVICE;

done:
	// Close the conversion descriptor
	if(ctx && ctx->cd && ctx->cd != (iconv_t)-1)
		iconv_close(ctx->cd);

	// Clean up
	if(xml_doc) xmlFreeDoc(xml_doc);
	if(ctx) {
		// Free the ParseContext.constants list
		Constant *celem = ctx->constants;
		while(celem) {
			Constant *next = celem->next;
			free(celem->name);
			free(celem);
			celem = next;
		}

		// Free the ParseContext.controls list
		UVCXUControl *elem = ctx->controls;
		while(elem) {
			UVCXUControl *next = elem->next;
			xmlFree(elem->id);
			free(elem);
			elem = next;
		}

		free(ctx);
	}
	if(devices) free(devices);

	return ret;
}


#else


CResult c_add_control_mappings_from_file (const char *file_name, CDynctrlInfo *info)
{
	return C_NOT_IMPLEMENTED;
}


#endif
