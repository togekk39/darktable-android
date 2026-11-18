/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <pthread.h>
#ifdef __ANDROID__
#define DT_MOBILE_CANCEL_DISABLE 0
/* Bionic workers exit cooperatively through dt_control_running() */
static inline int dt_mobile_setcancelstate(int state, int *old)
{
  (void)state;
  if(old) *old = DT_MOBILE_CANCEL_DISABLE;
  return 0;
}
#else
#define DT_MOBILE_CANCEL_DISABLE PTHREAD_CANCEL_DISABLE
#define dt_mobile_setcancelstate pthread_setcancelstate
#endif
