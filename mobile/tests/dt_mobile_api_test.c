/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dt_mobile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
  dt_mobile_session *session = NULL;
  assert(dt_mobile_open(NULL, &session) == DT_MOBILE_ERROR_INVALID_ARGUMENT);
  const char *path = "dt-mobile-test-input.dng"; FILE *f = fopen(path, "wb"); assert(f);
  const char bytes[16] = { 'I', 'I', 42, 0 }; assert(fwrite(bytes, sizeof(bytes), 1, f) == 1); fclose(f);
  assert(dt_mobile_open(path, &session) == DT_MOBILE_OK); assert(session);
  uint8_t *pixels = (uint8_t *)1; int w = 1, h = 1;
  assert(dt_mobile_render_preview(session, 2048, 2048, &pixels, &w, &h) == DT_MOBILE_ERROR_UNSUPPORTED);
  assert(pixels == NULL && w == 0 && h == 0); assert(strstr(dt_mobile_last_error(session), "pending"));
  dt_mobile_cancel(session);
  assert(dt_mobile_export(session, "out.jpg", "jpeg", 90) == DT_MOBILE_ERROR_CANCELLED);
  dt_mobile_close(session); remove(path); return 0;
}
