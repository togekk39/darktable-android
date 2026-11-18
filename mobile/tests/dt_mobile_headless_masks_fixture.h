/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DT_MOBILE_HEADLESS_MASKS_FIXTURE_H
#define DT_MOBILE_HEADLESS_MASKS_FIXTURE_H

#include "common/darktable.h"
#include "develop/blend.h"
#include "develop/develop.h"
#include "develop/masks.h"
#include "develop/imageop.h"
#include "develop/mask_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the editor reads legacy mask fields for its existing controls. Rendering
 * develops, including one entered on the same thread, must still migrate */
static int dt_mobile_mask_edit_scope_fixture(dt_develop_t *editor)
{
  dt_iop_module_so_t *plugin = dt_iop_get_module_so("exposure");
  if(!plugin || !plugin->flags || !plugin->blend_colorspace || !plugin->default_colorspace) return 0;
  dt_develop_t renderer = {0};
  dt_iop_module_t edited = { .dev = editor, .flags = plugin->flags,
    .blend_colorspace = plugin->blend_colorspace, .default_colorspace = plugin->default_colorspace };
  dt_iop_module_t rendered = { .dev = &renderer, .flags = plugin->flags,
    .blend_colorspace = plugin->blend_colorspace, .default_colorspace = plugin->default_colorspace };
  dt_develop_blend_params_t classic, converted;
  dt_develop_blend_init_blend_parameters(&classic, DEVELOP_BLEND_CS_RGB_SCENE);
  classic.mask_mode = DEVELOP_MASK_ENABLED | DEVELOP_MASK_CONDITIONAL;
  const dt_develop_t *previous = dt_mobile_mask_edit_begin(editor);
  int valid = !dt_develop_blend_migrate_classic_mask(&edited, &classic, -1)
    && classic.mask_mode == (DEVELOP_MASK_ENABLED | DEVELOP_MASK_CONDITIONAL);
  valid = valid && !dt_develop_blend_legacy_params_ext(&edited, &classic,
    14, &converted, dt_develop_blend_version(), sizeof(classic), -1)
    && converted.mask_mode == classic.mask_mode;
  dt_develop_blend_params_t uniform = classic;
  uniform.mask_mode = DEVELOP_MASK_ENABLED;
  valid = valid && dt_develop_blend_migrate_classic_mask(&rendered, &uniform, -1)
    && (uniform.mask_mode & DEVELOP_MASK_FLEXI);
  const dt_develop_t *nested = dt_mobile_mask_edit_begin(&renderer);
  valid = valid && !dt_mobile_mask_edit_context(editor)
    && dt_mobile_mask_edit_context(&renderer);
  dt_mobile_mask_edit_end(nested);
  valid = valid && dt_mobile_mask_edit_context(editor)
    && !dt_mobile_mask_edit_context(&renderer);
  dt_mobile_mask_edit_end(previous);
  uniform.mask_mode = DEVELOP_MASK_ENABLED;
  return valid && !dt_mobile_mask_edit_context(editor)
    && dt_develop_blend_migrate_classic_mask(&edited, &uniform, -1)
    && (uniform.mask_mode & DEVELOP_MASK_FLEXI);
}

/* Call after opening the first real engine session, when configuration and the
 * upstream mask functions are initialized. dt_init(FALSE) may still allocate
 * darktable.develop; neither NULL nor a GUI-owned develop is substituted here.
 * This fixture checks a separate, real headless develop's mask ownership. */
static int dt_mobile_headless_masks_fixture(void)
{
  dt_develop_t local;
  int initialized = 0;
  int success = 0;
  GList *source_forms = NULL;
  dt_develop_t *const global = darktable.develop;
  GList *const global_forms_before = global ? global->allforms : NULL;
  const guint global_count_before = global ? g_list_length(global->allforms) : 0;

#define MASK_FIXTURE_CHECK(condition) do { if(!(condition)) { \
  fprintf(stderr, "headless mask fixture:%d: check failed: %s\n", __LINE__, #condition); \
  goto finished; } } while(0)

  dt_dev_init(&local, FALSE);
  initialized = 1;
  MASK_FIXTURE_CHECK(!local.gui_attached && !local.full.pipe && !local.preview_pipe && !local.preview2.pipe);
  MASK_FIXTURE_CHECK(!local.forms && !local.allforms);
  MASK_FIXTURE_CHECK(dt_mobile_mask_edit_scope_fixture(&local));

  dt_masks_form_t *const source = dt_masks_create(DT_MASKS_CIRCLE);
  MASK_FIXTURE_CHECK(source != NULL);
  source_forms = g_list_append(source_forms, source);
  MASK_FIXTURE_CHECK(source->functions != NULL);
  MASK_FIXTURE_CHECK((size_t)source->functions->point_struct_size == sizeof(dt_masks_point_circle_t));
  source->formid = 700001;
  g_strlcpy(source->name, "mobile headless circle fixture", sizeof(source->name));

  dt_masks_point_circle_t *const point = calloc(1, sizeof(*point));
  MASK_FIXTURE_CHECK(point != NULL);
  point->center[0] = 0.25f;
  point->center[1] = 0.75f;
  point->radius = 0.15f;
  point->border = 0.05f;
  source->points = g_list_append(source->points, point);

  dt_masks_replace_current_forms(&local, source_forms);
  MASK_FIXTURE_CHECK(g_list_length(local.forms) == 1 && !local.allforms);
  MASK_FIXTURE_CHECK(local.forms != source_forms);
  dt_masks_form_t *const first = local.forms->data;
  MASK_FIXTURE_CHECK(first != source && first->type == source->type);
  MASK_FIXTURE_CHECK(first->formid == source->formid && first->version == source->version);
  MASK_FIXTURE_CHECK(!strcmp(first->name, source->name));
  MASK_FIXTURE_CHECK(first->functions == source->functions);
  MASK_FIXTURE_CHECK(g_list_length(first->points) == 1 && first->points != source->points);
  const dt_masks_point_circle_t *const first_point = first->points->data;
  MASK_FIXTURE_CHECK(first_point != point);
  MASK_FIXTURE_CHECK(first_point->center[0] == 0.25f && first_point->center[1] == 0.75f);
  MASK_FIXTURE_CHECK(first_point->radius == 0.15f && first_point->border == 0.05f);

  /* Changing the source must not modify a previously installed snapshot. */
  point->radius = 0.30f;
  MASK_FIXTURE_CHECK(first_point->radius == 0.15f);
  dt_masks_replace_current_forms(&local, source_forms);
  MASK_FIXTURE_CHECK(g_list_length(local.forms) == 1);
  MASK_FIXTURE_CHECK(g_list_length(local.allforms) == 1 && local.allforms->data == first);
  dt_masks_form_t *const second = local.forms->data;
  MASK_FIXTURE_CHECK(second != first && second != source);
  MASK_FIXTURE_CHECK(g_list_length(second->points) == 1);
  const dt_masks_point_circle_t *const second_point = second->points->data;
  MASK_FIXTURE_CHECK(second_point != point && second_point != first_point);
  MASK_FIXTURE_CHECK(second_point->radius == 0.30f);
  MASK_FIXTURE_CHECK(first_point->radius == 0.15f);

  dt_masks_replace_current_forms(&local, NULL);
  MASK_FIXTURE_CHECK(local.forms == NULL && g_list_length(local.allforms) == 2);
  MASK_FIXTURE_CHECK(g_list_find(local.allforms, first) && g_list_find(local.allforms, second));
  MASK_FIXTURE_CHECK(!g_list_find(local.allforms, source));
  MASK_FIXTURE_CHECK(darktable.develop == global);
  MASK_FIXTURE_CHECK(!global || (global->allforms == global_forms_before
                               && g_list_length(global->allforms) == global_count_before));
  success = 1;

finished:
  if(initialized) dt_dev_cleanup(&local);
  g_list_free_full(source_forms, (GDestroyNotify)dt_masks_free_form);
  if(darktable.develop != global
     || (global && (global->allforms != global_forms_before
                    || g_list_length(global->allforms) != global_count_before)))
  {
    fprintf(stderr, "headless mask fixture: a local develop changed global mask ownership\n");
    success = 0;
  }
#undef MASK_FIXTURE_CHECK
  return success;
}

#endif
