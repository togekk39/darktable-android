# SPDX-License-Identifier: GPL-3.0-or-later
# this file is included only by the assembled mobile engine configuration
if(NOT DT_MOBILE_SOURCE_ROOT)
  message(FATAL_ERROR "Use mobile/tools/configure-engine.py to assemble the engine configuration")
endif()
if(NOT DT_MOBILE_INTEGRATION_ROOT)
  set(DT_MOBILE_INTEGRATION_ROOT "${DT_MOBILE_SOURCE_ROOT}/mobile")
endif()
option(DT_MOBILE_HOST_INTEGRATION_TESTS "Enable mobile engine adapters on a host for regression tests" OFF)
if(NOT ANDROID AND NOT DT_MOBILE_HOST_INTEGRATION_TESTS)
  message(FATAL_ERROR "The engine integration is Android-only; explicitly enable DT_MOBILE_HOST_INTEGRATION_TESTS for host tests")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${DT_MOBILE_INTEGRATION_ROOT}/tools/assemble-source.py"
  --source "${DT_MOBILE_SOURCE_ROOT}" --output "${CMAKE_SOURCE_DIR}"
  --manifest "${DT_MOBILE_INTEGRATION_ROOT}/upstream-lock.json"
  RESULT_VARIABLE _dt_mobile_assembled)
if(NOT _dt_mobile_assembled EQUAL 0)
  message(FATAL_ERROR "The upstream source integration failed; review the compatibility diagnostic above")
endif()
file(GLOB_RECURSE _dt_mobile_integration_inputs CONFIGURE_DEPENDS
  "${DT_MOBILE_INTEGRATION_ROOT}/adapters/*"
  "${DT_MOBILE_INTEGRATION_ROOT}/cmake/*"
  "${DT_MOBILE_INTEGRATION_ROOT}/patches/*"
  "${DT_MOBILE_INTEGRATION_ROOT}/tools/*.py"
  "${DT_MOBILE_SOURCE_ROOT}/src/*"
  "${DT_MOBILE_SOURCE_ROOT}/cmake/*"
  "${DT_MOBILE_SOURCE_ROOT}/tools/*"
  "${DT_MOBILE_SOURCE_ROOT}/data/*"
  "${DT_MOBILE_SOURCE_ROOT}/*CMakeLists.txt")
# include every copied input, including data and non-C/C++ build helpers
file(READ "${DT_MOBILE_INTEGRATION_ROOT}/upstream-lock.json" _dt_mobile_lock)
string(JSON _dt_mobile_inventory_count LENGTH "${_dt_mobile_lock}" inventory)
math(EXPR _dt_mobile_inventory_last "${_dt_mobile_inventory_count} - 1")
foreach(_dt_mobile_index RANGE ${_dt_mobile_inventory_last})
  string(JSON _dt_mobile_input GET "${_dt_mobile_lock}" inventory ${_dt_mobile_index})
  list(APPEND _dt_mobile_integration_inputs "${DT_MOBILE_SOURCE_ROOT}/${_dt_mobile_input}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${_dt_mobile_integration_inputs} "${DT_MOBILE_INTEGRATION_ROOT}/upstream-lock.json")
if(DT_MOBILE_BUILD_TESTS)
  enable_testing()
endif()
set(BUILD_MOBILE_ENGINE ON)
add_custom_target(dt_mobile_check_upstream
  COMMAND "${Python3_EXECUTABLE}" "${DT_MOBILE_INTEGRATION_ROOT}/tools/assemble-source.py"
    --source "${DT_MOBILE_SOURCE_ROOT}" --output "${CMAKE_SOURCE_DIR}"
    --manifest "${DT_MOBILE_INTEGRATION_ROOT}/upstream-lock.json" --verify-only
  COMMENT "Checking reviewed upstream APIs and submodule revisions"
  VERBATIM)
option(BUILD_MOBILE_LIBRARY_ONLY "Build editor libraries, plugins and data without desktop applications" ${ANDROID})
set(DT_MOBILE_WITH_DARKTABLE_ENGINE ON CACHE BOOL "" FORCE)
set(DT_MOBILE_ANDROID_DEPENDENCIES OFF CACHE BOOL "" FORCE)
if(BUILD_MOBILE_LIBRARY_ONLY)
  set(BUILD_TOOLS OFF)
  set(BUILD_RS_IDENTIFY OFF)
  set(BINARY_PACKAGE_BUILD ON)
endif()
if(ANDROID)
  set(USE_MCP OFF CACHE BOOL "" FORCE)
  set(USE_WAYLAND OFF CACHE BOOL "" FORCE)
  set(USE_OPENCV OFF CACHE BOOL "" FORCE)
  include("${CMAKE_SOURCE_DIR}/mobile/cmake/AndroidEnginePackages.cmake")
endif()
