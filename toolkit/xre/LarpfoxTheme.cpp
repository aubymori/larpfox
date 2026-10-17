/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LarpfoxTheme.h"

#include <string.h>

#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/URLPreloader.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIFile.h"
#include "nsIPrefService.h"
#include "nsString.h"
#include "nsZipArchive.h"

namespace mozilla {
namespace {

class ThemePrefObserver final : public nsIPrefObserver {
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD OnStringPref(const char* aKind, const char* aName,
                          const char* aValue, bool aIsSticky,
                          bool aIsLocked) override {
    if (!strcmp(aName, "larpfox.current-theme")) {
      mName.Assign(aValue);
    }
    return NS_OK;
  }

  // The built-in preference is a string, so startup ignores wrong-typed user
  // values rather than replacing a preceding valid string value with them.
  NS_IMETHOD OnIntPref(const char*, const char*, int32_t, bool, bool) override {
    return NS_OK;
  }
  NS_IMETHOD OnBoolPref(const char*, const char*, bool, bool, bool) override {
    return NS_OK;
  }
  NS_IMETHOD OnError(const char* aMessage) override {
    printf_stderr("Larpfox theme preferences: %s\n", aMessage);
    return NS_OK;
  }

  nsCString mName;

 private:
  ~ThemePrefObserver() = default;
};

NS_IMPL_ISUPPORTS(ThemePrefObserver, nsIPrefObserver)

void ReadThemePrefs(nsIFile* aProfile, const nsAString& aLeaf,
                    nsIPrefObserver* aObserver) {
  nsCOMPtr<nsIFile> file;
  if (NS_FAILED(aProfile->Clone(getter_AddRefs(file))) ||
      NS_FAILED(file->Append(aLeaf))) {
    return;
  }

  // URLPreloader explicitly supports synchronous reads before initialization;
  // this is also the reader used by normal preference-file loading.
  auto data = URLPreloader::ReadFile(file);
  if (data.isErr()) {
    nsresult rv = data.unwrapErr();
    if (rv != NS_ERROR_FILE_NOT_FOUND) {
      printf_stderr(
          "Larpfox theme: could not read startup preferences (0x%08x)\n",
          static_cast<unsigned>(rv));
    }
    return;
  }
  nsAutoString path;
  (void)file->GetPath(path);
  // Like normal startup, retain valid assignments even if another statement
  // contains a syntax error. user.js is parsed after prefs.js.
  (void)Preferences::ParsePrefsBuffer(data.unwrap(), PrefValueKind::User,
                                      aObserver,
                                      NS_ConvertUTF16toUTF8(path).get());
}

bool IsThemeBasename(const nsCString& aName) {
  if (aName.IsEmpty() || aName.EqualsLiteral(".") ||
      aName.EqualsLiteral("..")) {
    return false;
  }
  for (uint32_t i = 0; i < aName.Length(); ++i) {
    char c = aName[i];
    // Reject both platform separators and Windows drive/alternate-stream syntax
    // on every platform, as profiles and theme selections can be moved.
    if (c == '/' || c == '\\' || c == ':' ||
        static_cast<unsigned char>(c) < 0x20) {
      return false;
    }
  }
  return true;
}

}  // namespace

already_AddRefed<nsIFile> FindLarpfoxTheme(nsIFile* aProfile, bool aSafeMode) {
  if (aSafeMode || !aProfile) {
    return nullptr;
  }

  RefPtr<ThemePrefObserver> observer = new ThemePrefObserver();
  ReadThemePrefs(aProfile, u"prefs.js"_ns, observer);
  ReadThemePrefs(aProfile, u"user.js"_ns, observer);
  if (observer->mName.IsEmpty()) {
    return nullptr;
  }
  if (!IsThemeBasename(observer->mName)) {
    printf_stderr("Larpfox theme: selection must be a single theme basename\n");
    return nullptr;
  }

  nsCOMPtr<nsIFile> file;
  nsAutoString leaf(NS_ConvertUTF8toUTF16(observer->mName));
  leaf.AppendLiteral(".ja");
  if (NS_FAILED(aProfile->Clone(getter_AddRefs(file))) ||
      NS_FAILED(file->Append(u"lfx-themes"_ns)) ||
      NS_FAILED(file->Append(leaf))) {
    return nullptr;
  }

  RefPtr<nsZipArchive> archive = nsZipArchive::OpenArchive(file);
  if (!archive || !archive->GetItem("chrome.manifest"_ns) ||
      !archive->GetItem("browser/chrome.manifest"_ns) ||
      !archive->GetItem("greprefs.js"_ns) ||
      NS_FAILED(archive->Test("chrome.manifest"_ns)) ||
      NS_FAILED(archive->Test("browser/chrome.manifest"_ns)) ||
      NS_FAILED(archive->Test("greprefs.js"_ns))) {
    printf_stderr(
        "Larpfox theme: missing, corrupt, or incomplete archive; "
        "using built-in UI\n");
    return nullptr;
  }
  return file.forget();
}

}  // namespace mozilla
