/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LarpfoxTheme_h
#define mozilla_LarpfoxTheme_h

#include "mozilla/AlreadyAddRefed.h"

class nsIFile;

namespace mozilla {

// Read the profile's startup user preferences without initializing XPCOM or
// loading theme code. Returns a validated full omni archive, or nullptr to use
// the built-in UI. Safe mode returns before accessing the profile or any files.
// Called on the main thread with the profile locked, before XPCOM startup.
already_AddRefed<nsIFile> FindLarpfoxTheme(nsIFile* aProfile, bool aSafeMode);

}  // namespace mozilla

#endif  // mozilla_LarpfoxTheme_h
