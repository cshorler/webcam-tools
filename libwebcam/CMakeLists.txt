#
# GENERAL
#

# Require CMake 2.6 because of the Debian packaging.
cmake_minimum_required (VERSION 2.6)

project (LIBWEBCAM)



#
# TARGETS
#

add_library (webcam SHARED libwebcam.c dynctrl.c)

set_target_properties (webcam PROPERTIES VERSION 0.2.1)



#
# COMPILATION AND LINKING
#

# Require LibXML
#include (FindLibXml2)
include (FindPkgConfig)
pkg_check_modules (LIBXML2 REQUIRED libxml-2.0)

# Require uvcvideo
message ("** Checking for uvcvideo ...")
unset (UVCVIDEO_INCLUDE_DIR CACHE)
unset (UVCVIDEO_INCLUDE_DIR)
if (UVCVIDEO_INCLUDE_PATH)
	message ("**   Overriding uvcvideo.h search path: ${UVCVIDEO_INCLUDE_PATH}")
	find_path (UVCVIDEO_INCLUDE_DIR
		#uvcvideo.h
		NAMES linux/uvcvideo.h uvcvideo.h
		PATHS ${UVCVIDEO_INCLUDE_PATH}
		#PATH_SUFFIXES linux
		NO_DEFAULT_PATH		# If the user is explicit don't look in standard paths
	)
else (UVCVIDEO_INCLUDE_PATH)
	find_path (UVCVIDEO_INCLUDE_DIR
		#uvcvideo.h
		NAMES linux/uvcvideo.h uvcvideo.h
		PATHS ${UVCVIDEO_INCLUDE_PATH}
		#PATH_SUFFIXES linux
	)
endif (UVCVIDEO_INCLUDE_PATH)
if (NOT UVCVIDEO_INCLUDE_DIR)
	message(FATAL_ERROR "Linux UVC Driver include file (linux/uvcvideo.h or uvcvideo.h) not found. Try adding -DUVCVIDEO_INCLUDE_PATH=... to specify its path.")
else (NOT UVCVIDEO_INCLUDE_DIR)
	message("**   Found Linux UVC Driver include file in: ${UVCVIDEO_INCLUDE_DIR}")
	find_file (PUBLIC_UVCVIDEO_H_PATH "linux/uvcvideo.h" PATHS ${UVCVIDEO_INCLUDE_DIR} NO_DEFAULT_PATH)
	if (NOT PUBLIC_UVCVIDEO_H_PATH)
		message("**   Using old-style uvcvideo.h from uvcvideo sources instead of public linux/uvcvideo.h.")
		set (EXTRA_COMPILE_FLAGS "-DUSE_PRIVATE_UVCVIDEO_H")
	endif (NOT PUBLIC_UVCVIDEO_H_PATH)
endif (NOT UVCVIDEO_INCLUDE_DIR)

# Require V4L2
message ("** Checking for V4L2 ...")
unset (V4L2_INCLUDE_DIR CACHE)
unset (V4L2_INCLUDE_DIR)
if (V4L2_INCLUDE_PATH)
	message ("**   Overriding videodev2.h search path: ${V4L2_INCLUDE_PATH}")
	find_path (V4L2_INCLUDE_DIR
		linux/videodev2.h
		PATHS ${V4L2_INCLUDE_PATH}
		NO_DEFAULT_PATH		# If the user is explicit don't look in standard paths
	)
else (V4L2_INCLUDE_PATH)
	find_path (V4L2_INCLUDE_DIR
		linux/videodev2.h
		PATHS ${V4L2_INCLUDE_PATH}
	)
endif (V4L2_INCLUDE_PATH)
if (NOT V4L2_INCLUDE_DIR)
	message(FATAL_ERROR "V4L2 include file (linux/videodev2.h) not found. Try adding -DV4L2_INCLUDE_PATH=... to specify its path.")
else (NOT V4L2_INCLUDE_DIR)
	message("**   Found V4L2 include file in: ${V4L2_INCLUDE_DIR}")
endif (NOT V4L2_INCLUDE_DIR)

# Feature check: V4L2 with V4L2_STRING_CONTROLS support
try_compile (HAS_V4L2_STRING_CONTROLS
	${CMAKE_BINARY_DIR}/
	${CMAKE_CURRENT_SOURCE_DIR}/../common/build/cmake_try_v4l2_ctrl_type_string.c
	CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${V4L2_INCLUDE_DIR}"
)
if (HAS_V4L2_STRING_CONTROLS)
	message("**   Your V4L2 has V4L2_CTRL_TYPE_STRING support. Enabling raw control support.")
	add_definitions (-DENABLE_RAW_CONTROLS)
else (HAS_V4L2_STRING_CONTROLS)
	message("**   Your V4L2 does NOT have V4L2_CTRL_TYPE_STRING support. Compiling WITHOUT raw control support.")
endif (HAS_V4L2_STRING_CONTROLS)

# Includes
include_directories (include ../common/include ${LIBXML2_INCLUDE_DIRS} ${UVCVIDEO_INCLUDE_DIR} ${V4L2_INCLUDE_DIR})

# Libraries
target_link_libraries (webcam ${LIBXML2_LIBRARIES})

# Compiler flags
set_target_properties (webcam PROPERTIES
	COMPILE_FLAGS "-Wall ${EXTRA_COMPILE_FLAGS}"
)



#
# INSTALLATION
#

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
	SET(CMAKE_INSTALL_LIBDIR "lib")
endif (NOT DEFINED CMAKE_INSTALL_LIBDIR)

if (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
	message("** Installation directory for libwebcam: ${CMAKE_INSTALL_LIBDIR}")
else (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
	message("** Installation directory for libwebcam: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
install (
	TARGETS					webcam
	LIBRARY DESTINATION		${CMAKE_INSTALL_LIBDIR}
	COMPONENT				LIBWEBCAM
)



#
# PACKAGING
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
set (CPACK_PACKAGE_NAME					"libwebcam")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY	"Webcam utility library")
set (CPACK_PACKAGE_DESCRIPTION			"Provides camera, control, and frame format enumeration and simplifies camera control access. Also allows for configuring the Linux UVC driver's dynamic control mechanism.")
set (CPACK_RESOURCE_FILE_LICENSE		"${CMAKE_CURRENT_SOURCE_DIR}/COPYING.LESSER")
set (CPACK_RESOURCE_FILE_README			"${CMAKE_CURRENT_SOURCE_DIR}/README")
set (CPACK_PACKAGE_VENDOR				"Logitech")
set (CPACK_PACKAGE_VERSION				"0.2.1")
set (CPACK_PACKAGE_FILE_NAME			"${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

# Debian
# The dependencies are obtained by extracting the (dependency lacking) .deb package
# into a directory called 'debian', adding a fake "Source: ..." line to the debian/control
# file and running 'dpkg-shlibdeps -O debian/usr/lib/libwebcam.so'.
set (CPACK_DEBIAN_PACKAGE_MAINTAINER	"Martin Rubli <martin_rubli@logitech.com>")
set (CPACK_DEBIAN_PACKAGE_DEPENDS		"libxml2 (>= 2.6.27), libc6 (>= 2.4)")

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
