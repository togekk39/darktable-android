/* SPDX-License-Identifier: GPL-3.0-or-later */
dt_iop_module_t *dt_dev_module_duplicate_ext(dt_develop_t *dev,
                                             dt_iop_module_t *base,
                                             const gboolean reorder_iop)
{
  return dt_dev_module_duplicate_at_priority(dev, base, reorder_iop, 0);
}
