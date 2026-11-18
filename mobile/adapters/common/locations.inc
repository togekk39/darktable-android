/* SPDX-License-Identifier: GPL-3.0-or-later */
/* included after dt_loc_init_generic: reuse upstream normalization and creation */
static char *_dt_mobile_localedir(const char *application_directory, const char *localedir)
{
#ifdef __ANDROID__
  (void)application_directory;
  gchar *runtime_root = g_path_get_dirname(darktable.datadir);
  gchar *default_localedir = g_build_filename(runtime_root, "locale", NULL);
  gchar *resolved = dt_loc_init_generic(localedir, NULL, default_localedir);
  g_free(default_localedir);
  g_free(runtime_root);
  return resolved;
#else
  return dt_loc_init_generic(localedir, application_directory, DARKTABLE_LOCALEDIR);
#endif
}

static char *_dt_mobile_sharedir(const char *application_directory)
{
#ifdef __ANDROID__
  (void)application_directory;
  gchar *runtime_root = g_path_get_dirname(darktable.datadir);
  gchar *resolved = dt_loc_init_generic(runtime_root, NULL, NULL);
  g_free(runtime_root);
  return resolved;
#else
  return dt_loc_init_generic(NULL, application_directory, DARKTABLE_SHAREDIR);
#endif
}
