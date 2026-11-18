/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
struct dt_develop_t;

/* preserve classic edit controls only while loading this mobile editor */
const struct dt_develop_t *dt_mobile_mask_edit_begin(const struct dt_develop_t *dev);
void dt_mobile_mask_edit_end(const struct dt_develop_t *previous);
int dt_mobile_mask_edit_context(const struct dt_develop_t *dev);
