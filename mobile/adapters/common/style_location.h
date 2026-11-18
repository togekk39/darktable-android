/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
static inline gchar *dt_mobile_style_directory(const char *sharedir)
{
#ifdef __ANDROID__
  (void)sharedir;
  return g_build_filename(darktable.datadir, "styles", NULL);
#else
  return g_build_filename(sharedir, "darktable/styles", NULL);
#endif
}
