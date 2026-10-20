/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUXThemeData.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

#include "WinUtils.h"
#include "mozilla/Assertions.h"

using mozilla::Some;

nsUXThemeData::ThemeHandle nsUXThemeData::sThemes[eUXNumClasses];
SIZE nsUXThemeData::sCommandButtonMetrics[CMDBUTTONIDX_BUTTONBOX];
bool nsUXThemeData::sCommandButtonMetricsInitialized = false;
bool nsUXThemeData::sTitlebarInfoPopulated = false;
bool nsUXThemeData::sNativeThemeInfoInitialized = false;
bool nsUXThemeData::sIsDefaultWindowsTheme = false;
bool nsUXThemeData::sIsHighContrastOn = false;
bool nsUXThemeData::sFlatMenus = false;

nsUXThemeData::ThemeHandle::~ThemeHandle() { Close(); }

void nsUXThemeData::ThemeHandle::OpenOnce(LPCWSTR aClassList) {
  // Cache failed opens as well, until the next theme change.
  if (mHandle.isNothing()) {
    mHandle = Some(::OpenThemeData(nullptr, aClassList));
  }
}

void nsUXThemeData::ThemeHandle::Close() {
  if (mHandle.isSome()) {
    if (HANDLE handle = mHandle.extract()) {
      ::CloseThemeData(handle);
    }
  }
}

nsUXThemeData::ThemeHandle::operator HANDLE() const {
  return mHandle.valueOr(nullptr);
}

void nsUXThemeData::Invalidate() {
  for (auto& theme : sThemes) {
    theme.Close();
  }
  sCommandButtonMetricsInitialized = false;
  sTitlebarInfoPopulated = false;
  sNativeThemeInfoInitialized = false;
}

HANDLE nsUXThemeData::GetTheme(nsUXThemeClass aClass) {
  MOZ_ASSERT(aClass >= eUXButton && aClass < eUXNumClasses);
  auto& theme = sThemes[aClass];
  theme.OpenOnce(GetClassName(aClass));
  return theme;
}

const wchar_t* nsUXThemeData::GetClassName(nsUXThemeClass aClass) {
  switch (aClass) {
    case eUXButton:
      return L"Button";
    case eUXEdit:
      return L"Edit";
    case eUXTooltip:
      return L"Tooltip";
    case eUXRebar:
      return L"Rebar";
    case eUXMediaRebar:
      return L"Media::Rebar";
    case eUXCommunicationsRebar:
      return L"Communications::Rebar";
    case eUXBrowserTabBarRebar:
      return L"BrowserTabBar::Rebar";
    case eUXToolbar:
      return L"Toolbar";
    case eUXMediaToolbar:
      return L"Media::Toolbar";
    case eUXCommunicationsToolbar:
      return L"Communications::Toolbar";
    case eUXProgress:
      return L"Progress";
    case eUXTab:
      return L"Tab";
    case eUXScrollbar:
      return L"Scrollbar";
    case eUXTrackbar:
      return L"Trackbar";
    case eUXSpin:
      return L"Spin";
    case eUXStatus:
      return L"Status";
    case eUXCombobox:
      return L"Combobox";
    case eUXHeader:
      return L"Header";
    case eUXListview:
      return L"Listview";
    case eUXTreeview:
      return L"TreeView";
    case eUXMenu:
      return L"Menu";
    case eUXWindowFrame:
      return L"Window";
    case eUXNumClasses:
      break;
  }
  MOZ_ASSERT_UNREACHABLE("Unknown UX theme class");
  return L"";
}

void nsUXThemeData::EnsureCommandButtonMetrics() {
  if (sCommandButtonMetricsInitialized) {
    return;
  }
  SIZE size = {::GetSystemMetrics(SM_CXSIZE), ::GetSystemMetrics(SM_CYSIZE)};
  for (auto& button : sCommandButtonMetrics) {
    button = size;
  }
  sCommandButtonMetricsInitialized = true;
}

SIZE nsUXThemeData::GetCommandButtonMetrics(CmdButtonIdx aMetric) {
  MOZ_ASSERT(aMetric >= CMDBUTTONIDX_MINIMIZE &&
             aMetric < CMDBUTTONIDX_BUTTONBOX);
  EnsureCommandButtonMetrics();
  return sCommandButtonMetrics[aMetric];
}

SIZE nsUXThemeData::GetCommandButtonBoxMetrics() {
  EnsureCommandButtonMetrics();
  SIZE size = {};
  for (const auto& button : sCommandButtonMetrics) {
    size.cx += button.cx;
    size.cy = std::max(size.cy, button.cy);
  }
  return size;
}

void nsUXThemeData::UpdateTitlebarInfo(HWND aWindow) {
  if (!aWindow || sTitlebarInfoPopulated) {
    return;
  }
  TITLEBARINFOEX info = {};
  info.cbSize = sizeof(info);
  ::SendMessageW(aWindow, WM_GETTITLEBARINFOEX, 0,
                 reinterpret_cast<LPARAM>(&info));

  constexpr size_t indices[] = {2, 3, 5};
  for (size_t index : indices) {
    const RECT& rect = info.rgrect[index];
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
      return;
    }
  }
  const double scale = mozilla::widget::WinUtils::SystemScaleFactor() /
                       mozilla::widget::WinUtils::LogToPhysFactor(aWindow);
  for (size_t i = 0; i < CMDBUTTONIDX_BUTTONBOX; ++i) {
    const RECT& rect = info.rgrect[indices[i]];
    sCommandButtonMetrics[i] = {
        LONG(std::lround((rect.right - rect.left) * scale)),
        LONG(std::lround((rect.bottom - rect.top) * scale))};
  }
  sCommandButtonMetricsInitialized = true;
  sTitlebarInfoPopulated = true;
}

void nsUXThemeData::EnsureNativeThemeInfo() {
  if (!sNativeThemeInfoInitialized) {
    UpdateNativeThemeInfo();
  }
}

void nsUXThemeData::UpdateNativeThemeInfo() {
  HIGHCONTRAST highContrast = {};
  highContrast.cbSize = sizeof(highContrast);
  sIsHighContrastOn =
      ::SystemParametersInfoW(SPI_GETHIGHCONTRAST, 0, &highContrast, 0) &&
      (highContrast.dwFlags & HCF_HIGHCONTRASTON);

  BOOL flatMenus = FALSE;
  sFlatMenus =
      ::SystemParametersInfoW(SPI_GETFLATMENU, 0, &flatMenus, 0) && flatMenus;

  sIsDefaultWindowsTheme = false;
  sNativeThemeInfoInitialized = true;
  if (!IsAppThemed() || sIsHighContrastOn) {
    return;
  }

  WCHAR themeFileName[MAX_PATH + 1];
  if (FAILED(::GetCurrentThemeName(themeFileName, MAX_PATH + 1, nullptr, 0,
                                   nullptr, 0))) {
    return;
  }
  const wchar_t* themeName = wcsrchr(themeFileName, L'\\');
  themeName = themeName ? themeName + 1 : themeFileName;
  sIsDefaultWindowsTheme = !::lstrcmpiW(themeName, L"aero.msstyles") ||
                           !::lstrcmpiW(themeName, L"aerolite.msstyles") ||
                           !::lstrcmpiW(themeName, L"luna.msstyles");
}

bool nsUXThemeData::IsDefaultWindowTheme() {
  EnsureNativeThemeInfo();
  return sIsDefaultWindowsTheme;
}

bool nsUXThemeData::IsHighContrastOn() {
  EnsureNativeThemeInfo();
  return sIsHighContrastOn;
}

bool nsUXThemeData::AreFlatMenusEnabled() {
  EnsureNativeThemeInfo();
  return sFlatMenus;
}

bool nsUXThemeData::IsAppThemed() { return !!::IsAppThemed(); }
