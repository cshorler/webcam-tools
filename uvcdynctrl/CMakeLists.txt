#
# GENERAL
#

# Require CMake 2.6 because of the Debian packaging.
cmake_minimum_required (VERSION 2.6)

project (UVCDYNCTRL)



#
# TARGETS
#

add_executable (uvcdynctrl main.c controls.c cmdline.c)

set_target_properties (uvcdynctrl PROPERTIES VERSION 0.2.1)



#
# SPECIAL FILES
#

# gengetopt for command line parsing
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c" AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmdline.h")
	message("** Command line parser source files not found. Creating gengetopt rules ...")

	# Check if gengetopt is installed
	find_program(GENGETOPT gengetopt)
	if (NOT GENGETOPT)
		message(FATAL_ERROR "Unable to find gengetopt")
	endif (NOT GENGETOPT)

	add_custom_command (
		OUTPUT		${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c ${CMAKE_CURRENT_SOURCE_DIR}/cmdline.h
		COMMAND		gengetopt -i ${CMAKE_CURRENT_SOURCE_DIR}/uvcdynctrl.ggo --output-dir ${CMAKE_CURRENT_SOURCE_DIR} --unamed-opts=VALUES
		DEPENDS 	${CMAKE_CURRENT_SOURCE_DIR}/uvcdynctrl.ggo
		COMMENT		"Generating getopt parser code (cmdline.*) ..."
		VERBATIM
	)
endif (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmdline.c" AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmdline.h")

# config.h
# Note how we define HAVE_CONFIG_H. This will let gengetopt pick up
# the VERSION constant from config.h.
get_target_property (UVCDYNCTRL_VERSION uvcdynctrl VERSION)
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
add_definitions (-DHAVE_CONFIG_H)



#
# COMPILATION AND LINKING
#

include_directories (../common/include)
include_directories (${CMAKE_CURRENT_BINARY_DIR})
link_directories (${LIBWEBCAM_BINARY_DIR}/webcam)

target_link_libraries (uvcdynctrl webcam)

# Compiler flags
set_target_properties (uvcdynctrl PROPERTIES
	COMPILE_FLAGS "--std=gnu99 -Wall"
)

#
# INSTALLATION
#

# uvcdynctrl binary
message("** Installation directory for uvcdynctrl: ${CMAKE_INSTALL_PREFIX}/bin")
install (
	TARGETS					uvcdynctrl
	RUNTIME DESTINATION		bin
	COMPONENT				UVCDYNCTRL
)

# udev rules, script, and data
# Default permissions for DIRECTORY files: rw-r--r--
# Default permissions for PROGRAMS files:  rwxr-xr-x
# Note that having absolute paths here requires CPACK_SET_DESTDIR to be set to "ON".
install (
	DIRECTORY	udev/rules/
	DESTINATION /etc/udev/rules.d
	PATTERN ".svn" EXCLUDE
)
install (
	PROGRAMS	udev/scripts/uvcdynctrl
	DESTINATION	/lib/udev
)
install (
	DIRECTORY	data
	DESTINATION /etc/udev
	PATTERN ".svn" EXCLUDE
)



#
# Packaging
#

# Prerequisites
execute_process (
	COMMAND				/usr/bin/dpkg --print-architecture
	OUTPUT_VARIABLE		CPACK_DEBIAN_PACKAGE_ARCHITECTURE
	RESULT_VARIABLE		EXECUTE_RESULT
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
if (EXECUTE_RESULT)
	message(FATAL_ERROR "Unable to determine current dpkg architecture: ${EXECUTE_RESULT}")
else (EXECUTE_RESULT)
	message("** Debian package architecture: ${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
endif (EXECUTE_RESULT)

# Generic
set (CPACK_GENERATOR					"TBZ2;DEB")
set (CPACK_PACKAGE_NAME					"uvcdynctrl")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY	"Manage dynamic controls in uvcvideo")
set (CPACK_PACKAGE_DESCRIPTION			"Command line tool to obtain and change camera controls and manage dynamic controls.")
set (CPACK_RESOURCE_FILE_LICENSE		"${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set (CPACK_RESOURCE_FILE_README			"${CMAKE_CURRENT_SOURCE_DIR}/README")
set (CPACK_PACKAGE_VENDOR				"Logitech")
set (CPACK_PACKAGE_VERSION				"${UVCDYNCTRL_VERSION}")
set (CPACK_PACKAGE_FILE_NAME			"${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
set (CPACK_SET_DESTDIR					"ON")	# Necessary because of the absolute install paths

# Debian
# The dependencies are obtained by extracting the (dependency lacking) .deb package
# into a directory called 'debian', adding a fake "Source: ..." line to the debian/control
# file and running 'dpkg-shlibdeps -O debian/usr/bin/uvcdynctrl'.
set (CPACK_DEBIAN_PACKAGE_MAINTAINER	"Martin Rubli <martin_rubli@logitech.com>")
set (CPACK_DEBIAN_PACKAGE_DEPENDS		"libwebcam (>= 0.2), libc6 (>= 2.7)")

# Instead of actually packing we just generate a CPack configuration file. The top level
# README file explains how to package the project.
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../common/build/CPackConfig.cmake.conf")
	message ("** Creating CPack configuration file for ${CPACK_PACKAGE_NAME} ...")
	configure_file (
		${CMAKE_CURRENT_SOURCE_DIR}/../common/build/CPackConfig.cmake.conf
		${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake
	)
endif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../common/build/CPackConfig.cmake.conf")

# Source packaging
# Note that we only enable source packaging if we're not building from the top-level
# directory. The reason for this is that source packaging at the top-level includes
# the entire tree by default and exclude patterns are not suitable in our case.
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	set (CPACK_SOURCE_GENERATOR				"TBZ2")
	set (CPACK_SOURCE_PACKAGE_FILE_NAME		"${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")

	# The following is a list of semicolon separate regexp patterns of files and
	# directories to ignore. Regexp anchors like ^ and $ can be used but because
	# CPack matches entire paths ^ won't work for file matching. We can use / on
	# the left-hand side instead. Also note that backslashes need to be double-
	# escaped (i.e. \\\\). Therefore Perl's \. to match a dot becomes \\\\. here.
	set (CPACK_SOURCE_IGNORE_FILES
		"/\\\\.svn/"								# Subversion directories
		"/\\\\..+$"									# Hidden files and directories
		"build"										# Build directories
		"tags;/doxygen-output"						# Misc
	)

	include (CPack)
endif (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
