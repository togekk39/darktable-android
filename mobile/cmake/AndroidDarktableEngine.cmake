# SPDX-License-Identifier: GPL-3.0-or-later
# Android deliberately enters through mobile/CMakeLists.txt, never the GTK
# desktop root.  The headless source graph is maintained beside upstream src.
set(DT_HEADLESS_ANDROID ON)
set(DT_HEADLESS_MODULE_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../src/headless"
                 "${CMAKE_CURRENT_BINARY_DIR}/darktable-headless")
if(NOT TARGET darktable_headless)
  message(FATAL_ERROR "The Android headless darktable engine target was not created")
endif()
add_library(lib_darktable ALIAS darktable_headless)
