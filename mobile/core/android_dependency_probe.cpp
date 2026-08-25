/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <glib.h>
#include <jpeglib.h>
#include <lcms2.h>
#include <libraw/libraw.h>
#include <png.h>
#include <sqlite3.h>
#include <tiffio.h>
#include <zlib.h>
#include <exiv2/exiv2.hpp>
#include <pugixml.hpp>
#include <string>

extern "C" unsigned long dt_mobile_dependency_probe(void)
{
  jpeg_error_mgr jpeg_error{};
  jpeg_std_error(&jpeg_error);
  const char *const glib_error = glib_check_version(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, 0);
  const std::string exiv2_version = Exiv2::versionString();
  const char *const libraw_version = LibRaw::version();
  const char *const tiff_version = TIFFGetVersion();
  return zlibCompileFlags() ^ png_access_version_number() ^ sqlite3_libversion_number()
         ^ cmsGetEncodedCMMversion() ^ PUGIXML_VERSION ^ (glib_error == nullptr)
         ^ !exiv2_version.empty() ^ (libraw_version != nullptr) ^ (tiff_version != nullptr);
}
