# SPDX-License-Identifier: GPL-3.0-or-later
if(NOT DT_HEADLESS_ANDROID OR NOT ANDROID)
  message(FATAL_ERROR "src/headless is the Android, GTK-free darktable engine graph")
endif()

set(DT_SRC "${CMAKE_CURRENT_LIST_DIR}/..")

# Keep this graph explicit: adding a desktop source to src/ must never silently add it
# to Android. Each group corresponds to a facility used by the mobile RAW pipeline.
set(DT_HEADLESS_CORE
  "${DT_SRC}/common/atomic.c"
  "${DT_SRC}/common/cache.c"
  "${DT_SRC}/common/darktable.c"
  "${DT_SRC}/common/database.c"
  "${DT_SRC}/common/datetime.c"
  "${DT_SRC}/common/dtpthread.c"
  "${DT_SRC}/common/dynload.c"
  "${DT_SRC}/common/exif.cc"
  "${DT_SRC}/common/file_location.c"
  "${DT_SRC}/common/film.c"
  "${DT_SRC}/common/history.c"
  "${DT_SRC}/common/history_snapshot.c"
  "${DT_SRC}/common/image.c"
  "${DT_SRC}/common/image_cache.c"
  "${DT_SRC}/common/imagebuf.c"
  "${DT_SRC}/common/iop_group.c"
  "${DT_SRC}/common/iop_order.c"
  "${DT_SRC}/common/iop_profile.c"
  "${DT_SRC}/common/metadata.c"
  "${DT_SRC}/common/metadata_export.c"
  "${DT_SRC}/common/mipmap_cache.c"
  "${DT_SRC}/common/module.c"
  "${DT_SRC}/common/noiseprofiles.c"
  "${DT_SRC}/common/presets.c"
  "${DT_SRC}/common/ratings.c"
  "${DT_SRC}/common/resource_limits.c"
  "${DT_SRC}/common/tags.c"
  "${DT_SRC}/common/undo.c"
  "${DT_SRC}/common/utility.c"
  "${DT_SRC}/common/variables.c"
  "${DT_SRC}/common/wb_presets.c"
)

set(DT_HEADLESS_PROCESSING
  "${DT_SRC}/common/bilateral.c"
  "${DT_SRC}/common/bilateralcl.c"
  "${DT_SRC}/common/box_filters.cc"
  "${DT_SRC}/common/calculator.c"
  "${DT_SRC}/common/color_harmony.c"
  "${DT_SRC}/common/color_vocabulary.c"
  "${DT_SRC}/common/colormatrices.c"
  "${DT_SRC}/common/colorspaces.c"
  "${DT_SRC}/common/curve_tools.c"
  "${DT_SRC}/common/custom_primaries.c"
  "${DT_SRC}/common/densecrf.cc"
  "${DT_SRC}/common/distance_transform.c"
  "${DT_SRC}/common/dlopencl.c"
  "${DT_SRC}/common/dng_opcode.c"
  "${DT_SRC}/common/dwt.c"
  "${DT_SRC}/common/eaw.c"
  "${DT_SRC}/common/gaussian.c"
  "${DT_SRC}/common/guided_filter.c"
  "${DT_SRC}/common/heal.c"
  "${DT_SRC}/common/histogram.c"
  "${DT_SRC}/common/interpolation.c"
  "${DT_SRC}/common/locallaplacian.c"
  "${DT_SRC}/common/locallaplaciancl.c"
  "${DT_SRC}/common/matrices.c"
  "${DT_SRC}/common/nlmeans_core.c"
  "${DT_SRC}/common/opencl.c"
  "${DT_SRC}/common/pfm.c"
  "${DT_SRC}/common/ras2vect.c"
)

set(DT_HEADLESS_RUNTIME
  "${DT_SRC}/control/conf.c"
  "${DT_SRC}/control/control.c"
  "${DT_SRC}/control/jobs.c"
  "${DT_SRC}/control/jobs/control_jobs.c"
  "${DT_SRC}/control/jobs/develop_jobs.c"
  "${DT_SRC}/control/jobs/film_jobs.c"
  "${DT_SRC}/control/jobs/image_jobs.c"
  "${DT_SRC}/control/jobs/sidecar_jobs.c"
  "${DT_SRC}/control/signal.c"
)

set(DT_HEADLESS_DEVELOP
  "${DT_SRC}/develop/blend.c"
  "${DT_SRC}/develop/blends/blendif_lab.c"
  "${DT_SRC}/develop/blends/blendif_raw.c"
  "${DT_SRC}/develop/blends/blendif_rgb_hsl.c"
  "${DT_SRC}/develop/blends/blendif_rgb_jzczhz.c"
  "${DT_SRC}/develop/develop.c"
  "${DT_SRC}/develop/format.c"
  "${DT_SRC}/develop/imageop.c"
  "${DT_SRC}/develop/imageop_math.c"
  "${DT_SRC}/develop/masks/brush.c"
  "${DT_SRC}/develop/masks/circle.c"
  "${DT_SRC}/develop/masks/detail.c"
  "${DT_SRC}/develop/masks/ellipse.c"
  "${DT_SRC}/develop/masks/gradient.c"
  "${DT_SRC}/develop/masks/group.c"
  "${DT_SRC}/develop/masks/masks.c"
  "${DT_SRC}/develop/masks/object.c"
  "${DT_SRC}/develop/masks/path.c"
  "${DT_SRC}/develop/pixelpipe.c"
  "${DT_SRC}/develop/pixelpipe_cache.c"
  "${DT_SRC}/develop/pixelpipe_hb.c"
  "${DT_SRC}/develop/tiling.c"
)

set(DT_HEADLESS_IMAGE
  "${DT_SRC}/imageio/imageio.c"
  "${DT_SRC}/imageio/imageio_dng.c"
  "${DT_SRC}/imageio/imageio_jpeg.c"
  "${DT_SRC}/imageio/imageio_pfm.c"
  "${DT_SRC}/imageio/imageio_png.c"
  "${DT_SRC}/imageio/imageio_pnm.c"
  "${DT_SRC}/imageio/imageio_qoi.c"
  "${DT_SRC}/imageio/imageio_rawspeed.cc"
  "${DT_SRC}/imageio/imageio_rgbe.c"
  "${DT_SRC}/imageio/imageio_tiff.c"
)

set(DT_HEADLESS_COMMON
  ${DT_HEADLESS_CORE}
  ${DT_HEADLESS_PROCESSING}
  ${DT_HEADLESS_RUNTIME}
  ${DT_HEADLESS_DEVELOP}
  ${DT_HEADLESS_IMAGE}
)
add_library(darktable_headless SHARED ${DT_HEADLESS_COMMON})
target_compile_definitions(darktable_headless PUBLIC
  HAVE_CONFIG_H=1 DT_HEADLESS=1 DT_ANDROID=1
  DARKTABLE_DATADIR="." DARKTABLE_LIBDIR=".")
target_include_directories(darktable_headless PUBLIC
  "${DT_SRC}" "${DT_SRC}/external" "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(darktable_headless PUBLIC dt_mobile_android_dependencies android log dl m)
set_target_properties(darktable_headless PROPERTIES OUTPUT_NAME darktable_headless)

# Keep processing modules as ordinary Android shared libraries beneath the
# plugins directory expected by dt_iop_load_modules_so().  moduledir itself is
# ApplicationInfo.nativeLibraryDir at runtime.
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
    PREFIX "lib" OUTPUT_NAME "${_iop}"
    LIBRARY_OUTPUT_DIRECTORY "${DT_HEADLESS_MODULE_OUTPUT_DIRECTORY}/plugins")
endforeach()
