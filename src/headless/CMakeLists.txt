# SPDX-License-Identifier: GPL-3.0-or-later
if(NOT DT_HEADLESS_ANDROID OR NOT ANDROID)
  message(FATAL_ERROR "src/headless is the Android, GTK-free darktable engine graph")
endif()

set(DT_SRC "${CMAKE_CURRENT_LIST_DIR}/..")
file(GLOB DT_HEADLESS_COMMON CONFIGURE_DEPENDS
  "${DT_SRC}/common/*.c" "${DT_SRC}/common/*.cc"
  "${DT_SRC}/control/*.c" "${DT_SRC}/control/jobs/*.c"
  "${DT_SRC}/develop/*.c" "${DT_SRC}/develop/*.cc"
  "${DT_SRC}/develop/blends/*.c" "${DT_SRC}/develop/masks/*.c"
  "${DT_SRC}/imageio/imageio.c" "${DT_SRC}/imageio/imageio_dng.c"
  "${DT_SRC}/imageio/imageio_jpeg.c" "${DT_SRC}/imageio/imageio_png.c"
  "${DT_SRC}/imageio/imageio_tiff.c" "${DT_SRC}/imageio/imageio_pnm.c"
  "${DT_SRC}/imageio/imageio_pfm.c" "${DT_SRC}/imageio/imageio_qoi.c"
  "${DT_SRC}/imageio/imageio_rgbe.c" "${DT_SRC}/imageio/imageio_rawspeed.cc")

# These units own desktop presentation, services, or hardware integration.
# Processing-side blend/mask/imageop code remains in the graph.
list(FILTER DT_HEADLESS_COMMON EXCLUDE REGEX
  "/(act_on|camera_control|collection|color_picker|dbus|gimp|gpx|import_session|map_locations|overlay|pdf|pwstorage|selection|styles|system_signal_handling|usermanual_url)\\.(c|cc)$")
list(FILTER DT_HEADLESS_COMMON EXCLUDE REGEX
  "/(crawler|progress)\\.c$")
list(FILTER DT_HEADLESS_COMMON EXCLUDE REGEX
  "/(blend_gui|imageop_gui|borders_helper|lightroom)\\.c$")

add_library(darktable_headless SHARED ${DT_HEADLESS_COMMON})
target_compile_definitions(darktable_headless PUBLIC
  HAVE_CONFIG_H=1 DT_HEADLESS=1 DT_ANDROID=1
  DARKTABLE_DATADIR="." DARKTABLE_LIBDIR=".")
target_include_directories(darktable_headless PUBLIC
  "${DT_SRC}" "${DT_SRC}/external" "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(darktable_headless PUBLIC dt_mobile_android_dependencies android log dl m)
set_target_properties(darktable_headless PROPERTIES OUTPUT_NAME darktable_headless)

# Keep processing modules as ordinary Android shared libraries.  darktable's
# existing GModule loader can discover them in ApplicationInfo.nativeLibraryDir.
find_package(Perl REQUIRED)
set(_required_iops rawprepare demosaic temperature colorin exposure colorbalancergb
  filmicrgb sigmoid colorout gamma finalscale flip highlights hotpixels
  denoiseprofile cacorrect cacorrectrgb channelmixerrgb)
foreach(_iop IN LISTS _required_iops)
  set(_input "${DT_SRC}/iop/${_iop}.c")
  if(_iop STREQUAL "demosaic")
    set(_extra "${DT_SRC}/iop/demosaicing/amaze.cc")
  else()
    set(_extra)
  endif()
  set(_generated "${CMAKE_CURRENT_BINARY_DIR}/introspection_${_iop}.c")
  add_custom_command(OUTPUT "${_generated}" DEPENDS "${_input}"
    COMMAND "${PERL_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/../tools/introspection/parser.pl"
      "${DT_SRC}/" "${_input}" "${_generated}")
  add_library(dt_iop_${_iop} MODULE "${_generated}" ${_extra})
  target_compile_options(dt_iop_${_iop} PRIVATE
    "-include${DT_SRC}/common/module_api.h" "-include${DT_SRC}/iop/iop_api.h")
  target_include_directories(dt_iop_${_iop} PRIVATE "${DT_SRC}" "${DT_SRC}/external" "${CMAKE_CURRENT_LIST_DIR}/include")
  target_link_libraries(dt_iop_${_iop} PRIVATE darktable_headless)
  set_target_properties(dt_iop_${_iop} PROPERTIES
    PREFIX "lib" OUTPUT_NAME "${_iop}" LIBRARY_OUTPUT_DIRECTORY "${DT_HEADLESS_MODULE_OUTPUT_DIRECTORY}")
endforeach()
