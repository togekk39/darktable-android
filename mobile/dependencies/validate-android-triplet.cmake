set(_triplet_file "${CMAKE_CURRENT_LIST_DIR}/triplets/arm64-android.cmake")

if(NOT EXISTS "${_triplet_file}")
  message(FATAL_ERROR "Android overlay triplet not found: ${_triplet_file}")
endif()

include("${_triplet_file}")

function(require_triplet_value variable expected)
  if(NOT DEFINED ${variable})
    message(FATAL_ERROR "${variable} must be defined by ${_triplet_file}")
  endif()

  if(NOT "${${variable}}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "${variable} must be '${expected}', but is '${${variable}}'")
  endif()
endfunction()

require_triplet_value(VCPKG_TARGET_ARCHITECTURE arm64)
require_triplet_value(VCPKG_CMAKE_SYSTEM_NAME Android)
require_triplet_value(VCPKG_CMAKE_SYSTEM_VERSION 26)
require_triplet_value(
  VCPKG_MAKE_BUILD_TRIPLET
  "--host=aarch64-linux-android"
)

if(NOT DEFINED VCPKG_CMAKE_CONFIGURE_OPTIONS)
  message(FATAL_ERROR
    "VCPKG_CMAKE_CONFIGURE_OPTIONS must be defined by ${_triplet_file}")
endif()

set(_required_cmake_option "-DANDROID_ABI=arm64-v8a")
list(FIND VCPKG_CMAKE_CONFIGURE_OPTIONS "${_required_cmake_option}"
  _required_cmake_option_index)
if(_required_cmake_option_index EQUAL -1)
  message(FATAL_ERROR
    "VCPKG_CMAKE_CONFIGURE_OPTIONS must contain ${_required_cmake_option}")
endif()

set(_forbidden_variables
  VCPKG_CHAINLOAD_TOOLCHAIN_FILE
  VCPKG_C_FLAGS
  VCPKG_CXX_FLAGS
  VCPKG_LINKER_FLAGS
)

foreach(variable IN LISTS _forbidden_variables)
  if(DEFINED ${variable})
    message(FATAL_ERROR
      "${variable} must not be set in the Android overlay triplet")
  endif()
endforeach()

message(STATUS "Android arm64 overlay triplet validation passed")
