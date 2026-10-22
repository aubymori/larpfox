/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WindowButtonMask_h
#define mozilla_widget_WindowButtonMask_h

#include <cstdint>

#include "Units.h"

namespace mozilla::widget {

// Visual exclusion in client device pixels, independent of native hit bounds.
struct WindowButtonMask {
  LayoutDeviceIntRect mRect;
  int32_t mBottomLeftRadius = 0;
  int32_t mBottomRightRadius = 0;

  bool operator==(const WindowButtonMask& aOther) const {
    return mRect == aOther.mRect &&
           mBottomLeftRadius == aOther.mBottomLeftRadius &&
           mBottomRightRadius == aOther.mBottomRightRadius;
  }
  bool operator!=(const WindowButtonMask& aOther) const {
    return !(*this == aOther);
  }
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_WindowButtonMask_h
