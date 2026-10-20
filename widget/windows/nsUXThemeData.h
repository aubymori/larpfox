/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUXThemeData_h
#define nsUXThemeData_h

#include <uxtheme.h>
#include <windows.h>

#include "mozilla/Maybe.h"

enum nsUXThemeClass {
  eUXButton = 0,
  eUXEdit,
  eUXTooltip,
  eUXRebar,
  eUXMediaRebar,
  eUXCommunicationsRebar,
  eUXBrowserTabBarRebar,
  eUXToolbar,
  eUXMediaToolbar,
  eUXCommunicationsToolbar,
  eUXProgress,
  eUXTab,
  eUXScrollbar,
  eUXTrackbar,
  eUXSpin,
  eUXStatus,
  eUXCombobox,
  eUXHeader,
  eUXListview,
  eUXTreeview,
  eUXMenu,
  eUXWindowFrame,
  eUXNumClasses
};

enum CmdButtonIdx {
  CMDBUTTONIDX_MINIMIZE = 0,
  CMDBUTTONIDX_RESTORE,
  CMDBUTTONIDX_CLOSE,
  CMDBUTTONIDX_BUTTONBOX
};

// Main-thread cache shared by native rendering and Windows look-and-feel.
class nsUXThemeData {
  class ThemeHandle final {
    mozilla::Maybe<HANDLE> mHandle;

   public:
    ThemeHandle() = default;
    ~ThemeHandle();

    ThemeHandle(const ThemeHandle&) = delete;
    ThemeHandle(ThemeHandle&&) = delete;
    ThemeHandle& operator=(const ThemeHandle&) = delete;
    ThemeHandle& operator=(ThemeHandle&&) = delete;

    operator HANDLE() const;
    void OpenOnce(LPCWSTR aClassList);
    void Close();
  };

  static ThemeHandle sThemes[eUXNumClasses];
  static SIZE sCommandButtonMetrics[CMDBUTTONIDX_BUTTONBOX];
  static bool sCommandButtonMetricsInitialized;
  static bool sTitlebarInfoPopulated;
  static bool sNativeThemeInfoInitialized;
  static bool sIsDefaultWindowsTheme;
  static bool sIsHighContrastOn;
  static bool sFlatMenus;

  static const wchar_t* GetClassName(nsUXThemeClass);
  static void EnsureCommandButtonMetrics();
  static void EnsureNativeThemeInfo();

 public:
  // Call on theme or system-metric changes and before widget shutdown.
  static void Invalidate();
  static HANDLE GetTheme(nsUXThemeClass);

  // Call once the top-level window has usable caption-button rectangles.
  static void UpdateTitlebarInfo(HWND);
  static SIZE GetCommandButtonMetrics(CmdButtonIdx);
  static SIZE GetCommandButtonBoxMetrics();

  static void UpdateNativeThemeInfo();
  static bool IsDefaultWindowTheme();
  static bool IsHighContrastOn();
  static bool AreFlatMenusEnabled();
  static bool IsAppThemed();
};

#endif  // nsUXThemeData_h
