# SPDX-License-Identifier: GPL-3.0-or-later
# include fragments share upstream's private types without cloning a module
include("${CMAKE_SOURCE_DIR}/mobile/cmake/UpstreamMath.cmake")
dt_mobile_generate_upstream_math("${CMAKE_SOURCE_DIR}")
include_directories("${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/mobile/adapters/include" "${DT_MOBILE_MATH_INCLUDE_DIR}")
list(APPEND SOURCES
  "${CMAKE_SOURCE_DIR}/mobile/adapters/common/module_layout.c"
  "${CMAKE_SOURCE_DIR}/mobile/adapters/common/sidecar.c"
  "${CMAKE_SOURCE_DIR}/mobile/adapters/common/whitebalance.c"
  "${CMAKE_SOURCE_DIR}/mobile/adapters/develop/mask_context.c")
if(ANDROID)
  list(REMOVE_ITEM SOURCES "common/system_signal_handling.c" "${CMAKE_CURRENT_SOURCE_DIR}/common/system_signal_handling.c")
  list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/mobile/adapters/common/system_signal_handling.c")
endif()
