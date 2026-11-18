# SPDX-License-Identifier: GPL-3.0-or-later
function(dt_mobile_generate_upstream_math source_root)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(_dt_math_output "${CMAKE_BINARY_DIR}/mobile-math")
  execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${source_root}/mobile/adapters/common/generate_math.py"
    --source-root "${source_root}" --output-dir "${_dt_math_output}"
    RESULT_VARIABLE _dt_math_result)
  if(NOT _dt_math_result EQUAL 0)
    message(FATAL_ERROR "The upstream numerical API changed; review the mobile adapter")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${source_root}/mobile/adapters/common/generate_math.py"
    "${source_root}/src/iop/temperature.c" "${source_root}/src/iop/zonesystem.c"
    "${source_root}/src/common/colorspaces.h")
  set(DT_MOBILE_MATH_INCLUDE_DIR "${_dt_math_output}" PARENT_SCOPE)
endfunction()
