/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included in the current upstream module; private types and algorithms stay upstream. */
/* Native clients edit original warp records through typed, bounded operations.
 * Coordinates describe the RAW input, as in get_point_scale, not a transformed
 * preview. Imported linked paths remain intact; this first editor operates on
 * isolated point warps and reuses the original allocation/deletion routines. */
static int mobile_liquify_valid(dt_iop_module_t *self, int *count, float *width, float *height)
{
  if(!self || !self->params || !self->dev || self->params_size != sizeof(dt_iop_liquify_params_t)) return 0;
  *width = self->dev->image_storage.width;
  *height = self->dev->image_storage.height;
  if(*width < 2.f || *height < 2.f) return 0;
  dt_iop_liquify_params_t *params = self->params;
  *count = 0;
  int ended = 0;
  for(int index = 0; index < MAX_NODES; index++)
  {
    const dt_liquify_path_data_t *node = &params->nodes[index];
    if(node->header.type == DT_LIQUIFY_PATH_INVALIDATED) { ended = 1; continue; }
    if(ended || node->header.type < DT_LIQUIFY_PATH_MOVE_TO_V1 || node->header.type > DT_LIQUIFY_PATH_CURVE_TO_V1
       || node->header.idx != index || node->header.prev < -1 || node->header.prev >= MAX_NODES
       || node->header.next < -1 || node->header.next >= MAX_NODES
       || node->warp.type < DT_LIQUIFY_WARP_TYPE_LINEAR || node->warp.type >= DT_LIQUIFY_WARP_TYPE_LAST
       || !isfinite(crealf(node->warp.point)) || !isfinite(cimagf(node->warp.point))
       || !isfinite(crealf(node->warp.strength)) || !isfinite(cimagf(node->warp.strength))
       || !isfinite(crealf(node->warp.radius)) || !isfinite(cimagf(node->warp.radius))
       || !isfinite(node->warp.control1) || !isfinite(node->warp.control2)
       || node->warp.control1 < 0.f || node->warp.control2 > 1.f
       || node->warp.control1 > node->warp.control2
       || !isfinite(cabsf(node->warp.radius - node->warp.point))
       || !isfinite(cabsf(node->warp.strength - node->warp.point))
       || cabsf(node->warp.radius - node->warp.point) < .5f) return 0;
    if(node->header.prev >= 0 && (params->nodes[node->header.prev].header.type == DT_LIQUIFY_PATH_INVALIDATED
       || params->nodes[node->header.prev].header.next != index)) return 0;
    if(node->header.next >= 0 && (params->nodes[node->header.next].header.type == DT_LIQUIFY_PATH_INVALIDATED
       || params->nodes[node->header.next].header.prev != index)) return 0;
    if((node->header.type == DT_LIQUIFY_PATH_MOVE_TO_V1) != (node->header.prev == -1)) return 0;
    if(node->header.type == DT_LIQUIFY_PATH_CURVE_TO_V1
       && (!isfinite(crealf(node->node.ctrl1)) || !isfinite(cimagf(node->node.ctrl1))
           || !isfinite(crealf(node->node.ctrl2)) || !isfinite(cimagf(node->node.ctrl2)))) return 0;
    int previous = index;
    for(int depth = 0; depth <= MAX_NODES; depth++)
    {
      previous = params->nodes[previous].header.prev;
      if(previous == -1) break;
      if(previous < 0 || previous >= MAX_NODES || depth == MAX_NODES) return 0;
    }
    (*count)++;
  }
  return 1;
}

G_MODULE_EXPORT int dt_iop_mobile_liquify_get(dt_iop_module_t *self, int index, const char *name, double *value)
{
  int count;
  float width, height;
  if(!name || !value || !mobile_liquify_valid(self, &count, &width, &height)) return -1;
  if(index == -1)
  {
    if(!strcmp(name, "count")) *value = count;
    else if(!strcmp(name, "min_radius")) *value = MAX(.001f, 1.f / MIN(width, height));
    else return 0;
    return 1;
  }
  if(index < 0 || index >= count) return -1;
  const dt_liquify_path_data_t *node = &((dt_iop_liquify_params_t *)self->params)->nodes[index];
  const int isolated = node->header.type == DT_LIQUIFY_PATH_MOVE_TO_V1
    && node->header.prev == -1 && node->header.next == -1;
  if(!strcmp(name, "editable")) { *value = isolated; return 1; }
  if(!isolated) return 0;
  const float scale = MIN(width, height);
  if(!strcmp(name, "x")) *value = crealf(node->warp.point) / width;
  else if(!strcmp(name, "y")) *value = cimagf(node->warp.point) / height;
  else if(!strcmp(name, "radius")) *value = cabsf(node->warp.radius - node->warp.point) / scale;
  else if(!strcmp(name, "strength_x")) *value = crealf(node->warp.strength - node->warp.point) / scale;
  else if(!strcmp(name, "strength_y")) *value = cimagf(node->warp.strength - node->warp.point) / scale;
  else if(!strcmp(name, "hardness1")) *value = node->warp.control1;
  else if(!strcmp(name, "hardness2")) *value = node->warp.control2;
  else if(!strcmp(name, "type")) *value = node->warp.type;
  else return 0;
  return 1;
}

G_MODULE_EXPORT int dt_iop_mobile_liquify_set(dt_iop_module_t *self, int index, const char *name, double value)
{
  int count;
  float width, height;
  if(!name || !isfinite(value) || !mobile_liquify_valid(self, &count, &width, &height)) return -1;
  dt_iop_liquify_params_t *params = self->params;
  const float scale = MIN(width, height);
  if(index == -1 && !strcmp(name, "add"))
  {
    if(value != 1.0 || count >= MAX_NODES) return -1;
    int allocated;
    dt_liquify_path_data_t *node = node_alloc(params, &allocated);
    if(!node) return -1;
    node->header.type = DT_LIQUIFY_PATH_MOVE_TO_V1;
    node->header.node_type = DT_LIQUIFY_NODE_TYPE_AUTOSMOOTH;
    init_warp(&node->warp, width * .5f + I * height * .5f);
    node->warp.radius += MAX(1.f, .15f * scale);
    node->warp.strength += .05f * scale;
    return 1;
  }
  if(index < 0 || index >= count) return -1;
  dt_liquify_path_data_t *node = &params->nodes[index];
  if(node->header.type != DT_LIQUIFY_PATH_MOVE_TO_V1 || node->header.prev != -1 || node->header.next != -1) return -1;
  if(!strcmp(name, "delete"))
  {
    if(value != 1.0) return -1;
    node_delete(params, node);
    return 1;
  }
  const float number = (float)value;
  if(!isfinite(number)) return -1;
  if(!strcmp(name, "x") || !strcmp(name, "y"))
  {
    if(number < 0.f || number > 1.f) return -1;
    const float complex position = !strcmp(name, "x") ? number * width + I * cimagf(node->warp.point)
      : crealf(node->warp.point) + I * number * height;
    const float complex delta = position - node->warp.point;
    node->warp.radius += delta;
    node->warp.strength += delta;
    node->warp.point = position;
  }
  else if(!strcmp(name, "radius"))
  {
    if(number < MAX(.001f, 1.f / scale) || number > .5f) return -1;
    node->warp.radius = node->warp.point + number * scale;
  }
  else if(!strcmp(name, "strength_x") || !strcmp(name, "strength_y"))
  {
    if(number < -.5f || number > .5f) return -1;
    const float complex previous = node->warp.strength - node->warp.point;
    node->warp.strength = node->warp.point + (!strcmp(name, "strength_x")
      ? number * scale + I * cimagf(previous) : crealf(previous) + I * number * scale);
  }
  else if(!strcmp(name, "hardness1"))
  {
    if(number < 0.f || number > node->warp.control2) return -1;
    node->warp.control1 = number;
  }
  else if(!strcmp(name, "hardness2"))
  {
    if(number < node->warp.control1 || number > 1.f) return -1;
    node->warp.control2 = number;
  }
  else if(!strcmp(name, "type"))
  {
    if(value != DT_LIQUIFY_WARP_TYPE_LINEAR && value != DT_LIQUIFY_WARP_TYPE_RADIAL_GROW
       && value != DT_LIQUIFY_WARP_TYPE_RADIAL_SHRINK) return -1;
    node->warp.type = (dt_liquify_warp_type_enum_t)(int)value;
  }
  else return 0;
  return 1;
}
