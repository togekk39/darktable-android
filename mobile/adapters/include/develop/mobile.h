/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
struct dt_iop_module_t *dt_dev_module_duplicate_at_priority(dt_develop_t *dev,
                                                          struct dt_iop_module_t *base,
                                                          const gboolean reorder_iop,
                                                          const int minimum_priority);
