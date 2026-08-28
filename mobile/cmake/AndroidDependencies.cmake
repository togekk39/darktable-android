# SPDX-License-Identifier: GPL-3.0-or-later
# The vcpkg toolchain must be supplied before project(). Gradle does that using
# android/app/build.gradle.kts; command-line builds use build-android-dependencies.sh.
if(NOT DEFINED VCPKG_TARGET_TRIPLET OR NOT VCPKG_TARGET_TRIPLET STREQUAL "arm64-android")
  message(FATAL_ERROR "Android dependencies require the pinned arm64-android vcpkg toolchain")
endif()

find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
find_package(lcms2 CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
find_package(pugixml CONFIG REQUIRED)
find_package(exiv2 CONFIG REQUIRED)
find_package(libraw CONFIG REQUIRED)

# FindPkgConfig runs the host pkg-config executable during a cross-build, so
# verify that the module it found describes the target installation. This is
# deliberately stricter than merely rejecting /usr: every absolute GLib search
# path must be below vcpkg's selected arm64-android prefix.
if(NOT DEFINED VCPKG_INSTALLED_DIR)
  message(FATAL_ERROR "VCPKG_INSTALLED_DIR is required to validate the Android GLib package")
endif()
cmake_path(ABSOLUTE_PATH VCPKG_INSTALLED_DIR NORMALIZE OUTPUT_VARIABLE _vcpkg_installed_dir)
set(_glib2_prefix "${_vcpkg_installed_dir}/${VCPKG_TARGET_TRIPLET}")
foreach(_glib2_path IN LISTS GLIB2_INCLUDE_DIRS GLIB2_LIBRARY_DIRS GLIB2_LINK_LIBRARIES)
  if(IS_ABSOLUTE "${_glib2_path}")
    cmake_path(NORMAL_PATH _glib2_path OUTPUT_VARIABLE _glib2_normalized_path)
    cmake_path(IS_PREFIX _glib2_prefix "${_glib2_normalized_path}" NORMALIZE _glib2_is_vcpkg_target)
    if(NOT _glib2_is_vcpkg_target)
      message(FATAL_ERROR
        "glib-2.0 resolved outside the vcpkg ${VCPKG_TARGET_TRIPLET} prefix: ${_glib2_path}")
    endif()
  endif()
endforeach()

# RawSpeed is darktable's pinned gitlink, not a second downloaded copy. Its
# camera database is consequently guaranteed to match the decoder code.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(RAWSPEED_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(RAWSPEED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RAWSPEED_PATH "${CMAKE_CURRENT_LIST_DIR}/../../src/external/rawspeed")
if(NOT EXISTS "${RAWSPEED_PATH}/CMakeLists.txt")
  message(FATAL_ERROR "RawSpeed submodule is missing; run git submodule update --init --recursive")
endif()
add_subdirectory("${RAWSPEED_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/rawspeed" EXCLUDE_FROM_ALL)

add_library(dt_mobile_android_dependencies INTERFACE)
target_link_libraries(dt_mobile_android_dependencies INTERFACE
  ZLIB::ZLIB JPEG::JPEG PNG::PNG TIFF::TIFF
  lcms2::lcms2 unofficial::sqlite3::sqlite3 PkgConfig::GLIB2
  pugixml::pugixml Exiv2::exiv2lib libraw::raw_r
  # The decoder adapter does not reference RawSpeed yet. Keep its complete
  # archive in libdt_mobile.so so this dependency probe actually validates and
  # ships the decoder rather than letting the linker discard every object.
  "-Wl,--whole-archive" rawspeed "-Wl,--no-whole-archive")
