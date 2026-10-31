/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DT_MOBILE_MASK_CONTROLS_H
#define DT_MOBILE_MASK_CONTROLS_H

/* Native forms belong to this private develop. Never use the GUI helpers that
 * register forms against darktable.develop. History deep-copies these original
 * forms through dt_dev_add_masks_history_item_ext before the engine snapshots. */
typedef void (*mobile_mask_sync_function)(dt_iop_module_t *);
typedef void (*mobile_retouch_add_function)(dt_iop_module_t *, int);
typedef int (*mobile_retouch_get_function)(dt_iop_module_t *, int32_t, const char *, double *);
typedef int (*mobile_retouch_set_function)(dt_iop_module_t *, int32_t, const char *, double);

static gboolean mobile_mask_special(dt_iop_module_t *module)
{
  return !strcmp(module->op, "spots") || !strcmp(module->op, "retouch");
}

static mobile_mask_sync_function mobile_mask_sync(dt_iop_module_t *module)
{
  mobile_mask_sync_function function = NULL;
  if(module && module->module)
    g_module_symbol(module->module, "dt_iop_mobile_sync_mask_forms", (gpointer *)&function);
  return function;
}

static gboolean mobile_masks_available(dt_iop_module_t *module)
{
  return module && module->dev && module->blend_params
    && (module->flags() & IOP_FLAGS_SUPPORTS_BLENDING)
    && (!(module->flags() & IOP_FLAGS_NO_MASKS) || mobile_mask_special(module))
    && (!mobile_mask_special(module) || mobile_mask_sync(module));
}

static dt_masks_form_t *mobile_mask_group(dt_iop_module_t *module)
{
  dt_masks_form_t *group = dt_masks_get_from_id(module->dev, module->blend_params->mask_id);
  return group && (group->type & DT_MASKS_GROUP) ? group : NULL;
}

static void mobile_mask_number(parameter_json *description, int32_t id, const char *shape,
    const char *field, const char *label, const char *type, double value, double minimum, double maximum)
{
  char name[96], title[192];
  snprintf(name, sizeof(name), "mask.%d.%s", id, field);
  snprintf(title, sizeof(title), "%s %d · %s", shape, id, label);
  append_mobile_blend_number(description, name, title, type, value, minimum, maximum, NAN);
}

static void mobile_mask_action(parameter_json *description, const char *name, const char *label)
{
  append_mobile_blend_number(description, name, label, "action", 1, 1, 1, NAN);
}

static gboolean mobile_mask_shape_supported(const dt_masks_form_t *form)
{
  if(!form || !form->points || form->points->next || !form->functions) return FALSE;
  const dt_masks_type_t type = form->type & ~(DT_MASKS_CLONE | DT_MASKS_NON_CLONE);
  return (type == DT_MASKS_CIRCLE && form->functions->point_struct_size == sizeof(dt_masks_point_circle_t))
    || (type == DT_MASKS_ELLIPSE && form->functions->point_struct_size == sizeof(dt_masks_point_ellipse_t))
    || (type == DT_MASKS_GRADIENT && form->functions->point_struct_size == sizeof(dt_masks_point_gradient_t));
}

static void mobile_retouch_parameters(dt_iop_module_t *module, dt_masks_form_t *form, parameter_json *description)
{
  if(strcmp(module->op, "retouch")) return;
  mobile_retouch_get_function get = NULL;
  g_module_symbol(module->module, "dt_iop_mobile_retouch_get_form", (gpointer *)&get);
  if(!get) return;
  static const struct { const char *name, *label; double minimum, maximum; } fields[] = {
    { "scale", "wavelet scale (0 = original)", 0, 16 },
    { "blur_radius", "blur radius", .1f, 200 },
    { "fill_brightness", "fill brightness", -1, 1 },
    { "fill_color[0]", "fill red", 0, 1 },
    { "fill_color[1]", "fill green", 0, 1 },
    { "fill_color[2]", "fill blue", 0, 1 },
  };
  double algorithm;
  if(get(module, form->formid, "algorithm", &algorithm) != 1) return;
  for(size_t k = 0; k < sizeof(fields) / sizeof(fields[0]); k++)
  {
    if(!strcmp(fields[k].name, "blur_radius") && algorithm != 3) continue;
    if(!strncmp(fields[k].name, "fill_", 5) && algorithm != 4) continue;
    double value;
    double maximum = fields[k].maximum;
    if(!strcmp(fields[k].name, "scale"))
    {
      double active_maximum;
      if(get(module, form->formid, "max_scale", &active_maximum) == 1) maximum = active_maximum;
    }
    if(get(module, form->formid, fields[k].name, &value) == 1)
      mobile_mask_number(description, form->formid, "retouch", fields[k].name, fields[k].label,
        !strcmp(fields[k].name, "scale") ? "int" : "float", value, fields[k].minimum, maximum);
  }
  const char *names[] = { "algorithm", "blur_type", "fill_mode" };
  const char *labels[] = { "retouch algorithm", "blur type", "fill mode" };
  const char *choices[][4] = { { "clone", "heal", "blur", "fill" }, { "Gaussian", "bilateral", NULL, NULL },
                               { "erase", "color", NULL, NULL } };
  for(size_t k = 0; k < 3; k++)
  {
    if((k == 1 && algorithm != 3) || (k == 2 && algorithm != 4)) continue;
    double value;
    if(get(module, form->formid, names[k], &value) != 1) continue;
    if(!description->first) g_string_append_c(description->json, ',');
    description->first = FALSE;
    GString *json = description->json;
    char name[96], title[160];
    snprintf(name, sizeof(name), "mask.%d.%s", form->formid, names[k]);
    snprintf(title, sizeof(title), "retouch %d · %s", form->formid, labels[k]);
    g_string_append(json, "{\"name\":"); json_string(json, name);
    g_string_append(json, ",\"label\":"); json_string(json, title);
    g_string_append(json, ",\"type\":\"enum\",\"value\":"); json_number(json, value);
    g_string_append(json, ",\"choices\":[");
    const int first = k == 0 ? (algorithm < 3 ? 1 : 3) : 0;
    for(int index = 0; index < 2; index++)
    {
      if(index) g_string_append_c(json, ',');
      g_string_append_printf(json, "{\"value\":%d,\"label\":", first + index);
      json_string(json, choices[k][k == 0 ? first + index - 1 : index]);
      g_string_append_c(json, '}');
    }
    g_string_append(json, "]}");
  }
}

static void append_mobile_mask_parameters(dt_iop_module_t *module, parameter_json *description)
{
  if(!mobile_masks_available(module)) return;
  dt_masks_form_t *group = mobile_mask_group(module);
  const unsigned count = group ? g_list_length(group->points) : 0;
  const unsigned limit = !strcmp(module->op, "retouch") ? 300 : 64;
  if(count < limit && !(module->blend_params->mask_mode & DEVELOP_MASK_RASTER)
     && (!dt_is_valid_maskid(module->blend_params->mask_id) || group))
  {
    if(!strcmp(module->op, "retouch"))
    {
      mobile_retouch_add_function add = NULL;
      g_module_symbol(module->module, "dt_iop_mobile_retouch_add_form", (gpointer *)&add);
      if(add)
      {
        mobile_mask_action(description, "mask.add.heal", "add heal circle");
        mobile_mask_action(description, "mask.add.clone", "add clone circle");
        mobile_mask_action(description, "mask.add.blur", "add blur circle");
        mobile_mask_action(description, "mask.add.fill", "add fill circle");
      }
    }
    else
    {
      mobile_mask_action(description, "mask.add.circle", mobile_mask_special(module) ? "add clone circle" : "add circle mask");
      mobile_mask_action(description, "mask.add.ellipse", mobile_mask_special(module) ? "add clone ellipse" : "add ellipse mask");
      if(!mobile_mask_special(module)) mobile_mask_action(description, "mask.add.gradient", "add gradient mask");
    }
  }
  if(!group) return;
  for(GList *item = group->points; item; item = item->next)
  {
    dt_masks_point_group_t *member = item->data;
    dt_masks_form_t *form = dt_masks_get_from_id(module->dev, member->formid);
    if(!form || !mobile_mask_shape_supported(form)) continue;
    const char *shape = form->type & DT_MASKS_CIRCLE ? "circle" : form->type & DT_MASKS_ELLIPSE ? "ellipse" : "gradient";
    float *x = NULL, *y = NULL;
    if(form->type & DT_MASKS_CIRCLE)
    {
      dt_masks_point_circle_t *p = form->points->data;
      x = p->center; y = p->center + 1;
      mobile_mask_number(description, form->formid, shape, "radius", "radius", "float", p->radius, .0005f, .5f);
      mobile_mask_number(description, form->formid, shape, "feather", "feather", "float", p->border, .0005f, .5f);
    }
    else if(form->type & DT_MASKS_ELLIPSE)
    {
      dt_masks_point_ellipse_t *p = form->points->data;
      x = p->center; y = p->center + 1;
      mobile_mask_number(description, form->formid, shape, "radius_x", "horizontal radius", "float", p->radius[0], .0005f, .5f);
      mobile_mask_number(description, form->formid, shape, "radius_y", "vertical radius", "float", p->radius[1], .0005f, .5f);
      mobile_mask_number(description, form->formid, shape, "rotation", "rotation (degrees)", "float", p->rotation, -180, 180);
      mobile_mask_number(description, form->formid, shape, "feather", "feather", "float", p->border, .0005f, .5f);
    }
    else
    {
      dt_masks_point_gradient_t *p = form->points->data;
      x = p->anchor; y = p->anchor + 1;
      mobile_mask_number(description, form->formid, shape, "rotation", "rotation (degrees)", "float", p->rotation, -180, 180);
      mobile_mask_number(description, form->formid, shape, "compression", "compression", "float", p->compression, .001f, 1);
      mobile_mask_number(description, form->formid, shape, "curvature", "curvature", "float", p->curvature, -2, 2);
      mobile_mask_number(description, form->formid, shape, "sigmoid", "sigmoidal transition", "bool",
        p->state == DT_MASKS_GRADIENT_STATE_SIGMOIDAL, 0, 1);
    }
    mobile_mask_number(description, form->formid, shape, "x", "center x", "float", *x, 0, 1);
    mobile_mask_number(description, form->formid, shape, "y", "center y", "float", *y, 0, 1);
    mobile_mask_number(description, form->formid, shape, "opacity", "shape opacity", "float", member->opacity, 0, 1);
    if(!mobile_mask_special(module))
    {
      mobile_mask_number(description, form->formid, shape, "enabled", "use shape", "bool", !!(member->state & DT_MASKS_STATE_USE), 0, 1);
      mobile_mask_number(description, form->formid, shape, "inverted", "invert shape", "bool", !!(member->state & DT_MASKS_STATE_INVERSE), 0, 1);
    }
    if(form->type & DT_MASKS_CLONE)
    {
      mobile_mask_number(description, form->formid, shape, "source_x", "source x", "float", form->source[0], 0, 1);
      mobile_mask_number(description, form->formid, shape, "source_y", "source y", "float", form->source[1], 0, 1);
      if(!strcmp(module->op, "retouch"))
        mobile_mask_number(description, form->formid, shape, "source_rotation", "source rotation (degrees)", "float",
          form->source[2] * (180.0 / G_PI), -180, 180);
    }
    mobile_retouch_parameters(module, form, description);
    char name[96], label[160];
    snprintf(name, sizeof(name), "mask.%d.delete", form->formid);
    snprintf(label, sizeof(label), "delete %s %d", shape, form->formid);
    mobile_mask_action(description, name, label);
  }
}

static int mobile_mask_add(dt_iop_module_t *module, const char *kind)
{
  dt_masks_form_t *group = mobile_mask_group(module);
  const unsigned limit = !strcmp(module->op, "retouch") ? 300 : 64;
  if((group && g_list_length(group->points) >= limit)
     || (!group && dt_is_valid_maskid(module->blend_params->mask_id))
     || (module->blend_params->mask_mode & DEVELOP_MASK_RASTER)) return -1;
  dt_masks_type_t type;
  int algorithm = 0;
  if(!strcmp(kind, "circle")) type = DT_MASKS_CIRCLE;
  else if(!strcmp(kind, "ellipse")) type = DT_MASKS_ELLIPSE;
  else if(!strcmp(kind, "gradient") && !mobile_mask_special(module)) type = DT_MASKS_GRADIENT;
  else if(!strcmp(module->op, "retouch"))
  {
    type = DT_MASKS_CIRCLE;
    if(!strcmp(kind, "clone")) algorithm = 1;
    else if(!strcmp(kind, "heal")) algorithm = 2;
    else if(!strcmp(kind, "blur")) algorithm = 3;
    else if(!strcmp(kind, "fill")) algorithm = 4;
    else return -1;
  }
  else return -1;
  mobile_retouch_add_function add = NULL;
  if(!strcmp(module->op, "retouch"))
  {
    if(!algorithm) return -1;
    g_module_symbol(module->module, "dt_iop_mobile_retouch_add_form", (gpointer *)&add);
    if(!add) return -1;
    type |= algorithm < 3 ? DT_MASKS_CLONE : DT_MASKS_NON_CLONE;
  }
  else if(!strcmp(module->op, "spots")) type |= DT_MASKS_CLONE;
  dt_masks_form_t *form = dt_masks_create(type);
  dt_masks_form_t *created_group = group ? NULL : dt_masks_create(DT_MASKS_GROUP);
  if(!form || (!group && !created_group))
  {
    if(form) dt_masks_free_form(form);
    if(created_group) dt_masks_free_form(created_group);
    return -1;
  }
  if(!group) group = created_group;
  if(created_group)
    while(dt_masks_get_from_id(module->dev, group->formid) || group->formid == form->formid) group->formid++;
  while(dt_masks_get_from_id(module->dev, form->formid) || form->formid == group->formid) form->formid++;
  g_strlcpy(form->name, "mobile editing shape", sizeof(form->name));
  if(created_group) g_strlcpy(group->name, "mobile editing shapes", sizeof(group->name));
  if(type & DT_MASKS_CIRCLE)
  {
    dt_masks_point_circle_t *p = g_new0(dt_masks_point_circle_t, 1);
    p->center[0] = p->center[1] = .5f; p->radius = .1f; p->border = .05f;
    form->points = g_list_append(NULL, p);
  }
  else if(type & DT_MASKS_ELLIPSE)
  {
    dt_masks_point_ellipse_t *p = g_new0(dt_masks_point_ellipse_t, 1);
    p->center[0] = p->center[1] = .5f; p->radius[0] = .15f; p->radius[1] = .1f;
    p->border = .05f; p->flags = DT_MASKS_ELLIPSE_EQUIDISTANT;
    form->points = g_list_append(NULL, p);
  }
  else
  {
    dt_masks_point_gradient_t *p = g_new0(dt_masks_point_gradient_t, 1);
    p->anchor[0] = p->anchor[1] = .5f; p->compression = .5f;
    p->state = DT_MASKS_GRADIENT_STATE_LINEAR;
    form->points = g_list_append(NULL, p);
  }
  form->source[0] = .25f; form->source[1] = .5f;
  dt_masks_point_group_t *member = dt_masks_group_add_form(group, form);
  if(!member)
  {
    dt_masks_free_form(form);
    if(created_group) dt_masks_free_form(created_group);
    return -1;
  }
  member->opacity = 1;
  if(created_group) module->dev->forms = g_list_append(module->dev->forms, group);
  module->dev->forms = g_list_append(module->dev->forms, form);
  dt_develop_blend_params_t blend = *module->blend_params;
  blend.mask_id = group->formid;
  if(!mobile_mask_special(module)) blend.mask_mode |= DEVELOP_MASK_ENABLED | DEVELOP_MASK_MASK;
  dt_iop_commit_blend_params(module, &blend, NULL);
  if(add) add(module, algorithm);
  else if(mobile_mask_special(module)) mobile_mask_sync(module)(module);
  return 1;
}

/* One numeric request updates one original form member. IDs are stable native
 * form identities, never list indexes, and must belong to this module's group. */
static int update_mobile_mask_parameter(dt_iop_module_t *module, const char *name, double value)
{
  if(strncmp(name, "mask.", 5)) return 0;
  if(!mobile_masks_available(module) || !isfinite(value)) return -1;
  if(!strncmp(name, "mask.add.", 9)) return value == 1 ? mobile_mask_add(module, name + 9) : -1;
  char *end;
  const long id = strtol(name + 5, &end, 10);
  if(end == name + 5 || *end != '.' || id <= 0 || id > INT32_MAX) return -1;
  char canonical[32]; snprintf(canonical, sizeof(canonical), "mask.%ld.", id);
  if(strncmp(name, canonical, strlen(canonical))) return -1;
  const char *field = end + 1;
  dt_masks_form_t *group = mobile_mask_group(module);
  if(!group) return -1;
  GList *link = NULL;
  for(GList *item = group->points; item; item = item->next)
    if(((dt_masks_point_group_t *)item->data)->formid == id) { link = item; break; }
  if(!link) return -1;
  dt_masks_point_group_t *member = link->data;
  dt_masks_form_t *form = dt_masks_get_from_id(module->dev, (int32_t)id);
  if(!mobile_mask_shape_supported(form)) return -1;
  if(!strcmp(field, "delete"))
  {
    if(value != 1) return -1;
    group->points = g_list_delete_link(group->points, link);
    free(member);
    /* Keep the now-empty group: mask snapshots use a nonempty form list to
     * distinguish clearing the last shape from an unchanged mask history. */
    if(mobile_mask_special(module)) mobile_mask_sync(module)(module);
    /* History owns independent copies. Reclaim an unused live form after its
     * last group reference disappears; preserve imported/shared references. */
    gboolean referenced = FALSE;
    for(GList *item = module->dev->forms; item && !referenced; item = item->next)
    {
      const dt_masks_form_t *candidate = item->data;
      if(!(candidate->type & DT_MASKS_GROUP)) continue;
      for(GList *child = candidate->points; child; child = child->next)
        if(((dt_masks_point_group_t *)child->data)->formid == id) { referenced = TRUE; break; }
    }
    for(GList *item = module->dev->iop; item && !referenced; item = item->next)
    {
      const dt_iop_module_t *candidate = item->data;
      if(candidate->blend_params && candidate->blend_params->mask_id == id) referenced = TRUE;
    }
    if(!referenced)
    {
      module->dev->forms = g_list_remove(module->dev->forms, form);
      dt_masks_free_form(form);
    }
    return 1;
  }
  float *destination = NULL;
  double minimum = 0, maximum = 1;
  int *boolean_destination = NULL, boolean_bit = 0;
  if(!strcmp(field, "opacity")) destination = &member->opacity;
  else if(!strcmp(field, "enabled") && !mobile_mask_special(module))
    { boolean_destination = &member->state; boolean_bit = DT_MASKS_STATE_USE; }
  else if(!strcmp(field, "inverted") && !mobile_mask_special(module))
    { boolean_destination = &member->state; boolean_bit = DT_MASKS_STATE_INVERSE; }
  else if((form->type & DT_MASKS_CLONE) && (!strcmp(field, "source_x") || !strcmp(field, "source_y")))
    destination = &form->source[!strcmp(field, "source_y")];
  else if((form->type & DT_MASKS_CLONE) && !strcmp(module->op, "retouch") && !strcmp(field, "source_rotation"))
  {
    if(value < -180 || value > 180) return -1;
    form->source[2] = (float)(value * (G_PI / 180.0));
    return 1;
  }
  else if(form->type & DT_MASKS_CIRCLE)
  {
    dt_masks_point_circle_t *p = form->points->data;
    if(!strcmp(field, "x")) destination = p->center;
    else if(!strcmp(field, "y")) destination = p->center + 1;
    else if(!strcmp(field, "radius")) { destination = &p->radius; minimum = .0005f; maximum = .5f; }
    else if(!strcmp(field, "feather")) { destination = &p->border; minimum = .0005f; maximum = .5f; }
  }
  else if(form->type & DT_MASKS_ELLIPSE)
  {
    dt_masks_point_ellipse_t *p = form->points->data;
    if(!strcmp(field, "x")) destination = p->center;
    else if(!strcmp(field, "y")) destination = p->center + 1;
    else if(!strcmp(field, "radius_x")) { destination = p->radius; minimum = .0005f; maximum = .5f; }
    else if(!strcmp(field, "radius_y")) { destination = p->radius + 1; minimum = .0005f; maximum = .5f; }
    else if(!strcmp(field, "rotation")) { destination = &p->rotation; minimum = -180; maximum = 180; }
    else if(!strcmp(field, "feather")) { destination = &p->border; minimum = .0005f; maximum = .5f; }
  }
  else if(form->type & DT_MASKS_GRADIENT)
  {
    dt_masks_point_gradient_t *p = form->points->data;
    if(!strcmp(field, "x")) destination = p->anchor;
    else if(!strcmp(field, "y")) destination = p->anchor + 1;
    else if(!strcmp(field, "rotation")) { destination = &p->rotation; minimum = -180; maximum = 180; }
    else if(!strcmp(field, "compression")) { destination = &p->compression; minimum = .001f; }
    else if(!strcmp(field, "curvature")) { destination = &p->curvature; minimum = -2; maximum = 2; }
    else if(!strcmp(field, "sigmoid"))
    {
      if(value != 0 && value != 1) return -1;
      p->state = value ? DT_MASKS_GRADIENT_STATE_SIGMOIDAL : DT_MASKS_GRADIENT_STATE_LINEAR;
      return 1;
    }
  }
  if(boolean_destination)
  {
    if(value != 0 && value != 1) return -1;
    if(value) *boolean_destination |= boolean_bit;
    else *boolean_destination &= ~boolean_bit;
    return 1;
  }
  if(destination)
  {
    if(value < -FLT_MAX || value > FLT_MAX) return -1;
    const double stored = (double)(float)value;
    if(!isfinite(stored) || stored < minimum || stored > maximum) return -1;
    *destination = (float)stored;
    return 1;
  }
  if(!strcmp(module->op, "retouch"))
  {
    mobile_retouch_set_function set = NULL;
    g_module_symbol(module->module, "dt_iop_mobile_retouch_set_form", (gpointer *)&set);
    if(set) return set(module, (int32_t)id, field, value);
  }
  return -1;
}

#endif
