/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "common/file_location.h"
#include "common/module_layout.h"

gboolean dt_module_get_directory(char *directory, size_t size, const char *subdir)
{
#ifdef __ANDROID__
  if(!dt_module_android_prefix(subdir)) return FALSE;
#endif
  dt_loc_get_plugindir(directory, size);
#ifndef __ANDROID__
  if(g_strlcat(directory, subdir, size) >= size) return FALSE;
#endif
  return directory[0] != '\0';
}

gchar *dt_module_get_name(const char *filename, const char *subdir)
{
#ifdef __ANDROID__
  const char *prefix = dt_module_android_prefix(subdir);
#else
  (void)subdir;
  const char *prefix = SHARED_MODULE_PREFIX;
#endif
  return dt_module_name_from_filename(filename, prefix, SHARED_MODULE_SUFFIX);
}
