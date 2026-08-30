/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dt_mobile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void test_arguments(void)
{
  dt_mobile_session *session = NULL;
  assert(dt_mobile_open(NULL, &session) == DT_MOBILE_ERROR_INVALID_ARGUMENT);
  assert(dt_mobile_render_preview(NULL, 10, 10, NULL, NULL, NULL) == DT_MOBILE_ERROR_INVALID_ARGUMENT);
  assert(dt_mobile_open("does-not-exist.gpr", &session) == DT_MOBILE_ERROR_IO);
}
int main(void)
{
  test_arguments();
  dt_mobile_session *session = NULL;
  const char *path = "dt-mobile-test-input.dng"; FILE *f = fopen(path, "wb"); assert(f);
  const char bytes[16] = { 'I', 'I', 42, 0 }; assert(fwrite(bytes, sizeof(bytes), 1, f) == 1); fclose(f);
  assert(dt_mobile_open(path, &session) == DT_MOBILE_OK); assert(session);
  uint8_t *pixels = (uint8_t *)1; int w = 1, h = 1;
  const dt_mobile_status render = dt_mobile_render_preview(session, 2048, 2048, &pixels, &w, &h);
  assert(render != DT_MOBILE_OK);
  assert(pixels == NULL && w == 0 && h == 0);
  dt_mobile_cancel(session);
  assert(dt_mobile_export(session, "out.jpg", "jpeg", 90) == DT_MOBILE_ERROR_CANCELLED);
  dt_mobile_close(session); remove(path);

  const char *gpr = getenv("DT_MOBILE_GPR_TEST_FILE");
  if(gpr)
  {
    assert(dt_mobile_open(gpr, &session) == DT_MOBILE_OK);
    pixels = NULL; w = h = 0;
    assert(dt_mobile_render_preview(session, 2048, 2048, &pixels, &w, &h) == DT_MOBILE_OK);
    assert(pixels && w > 0 && h > 0 && w <= 2048 && h <= 2048);
    int different = 0;
    for(size_t i = 4; i < (size_t)w * h * 4; i += 4)
      if(memcmp(pixels, pixels + i, 4)) { different = 1; break; }
    assert(different);
    dt_mobile_free_buffer(pixels); dt_mobile_close(session);
  }
  return 0;
}
