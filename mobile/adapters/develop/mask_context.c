/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "develop/mask_context.h"

/* exports on other threads or with another develop keep upstream migration */
static _Thread_local const struct dt_develop_t *_editor;

const struct dt_develop_t *dt_mobile_mask_edit_begin(const struct dt_develop_t *dev)
{
  const struct dt_develop_t *previous = _editor;
  _editor = dev;
  return previous;
}

void dt_mobile_mask_edit_end(const struct dt_develop_t *previous)
{
  _editor = previous;
}

int dt_mobile_mask_edit_context(const struct dt_develop_t *dev)
{
  return dev && dev == _editor;
}
