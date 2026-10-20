/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeWin_h
#define nsNativeThemeWin_h

#include <windows.h>

#include "Theme.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsUXThemeConstants.h"
#include "nsUXThemeData.h"

namespace mozilla::widget {

class nsNativeThemeWin final : public Theme {
 protected:
  virtual ~nsNativeThemeWin();

 public:
  // The nsITheme interface.
  void DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                            StyleAppearance aAppearance, const nsRect& aRect,
                            const nsRect& aDirtyRect, DrawOverflow) override;

  bool CreateWebRenderCommandsForWidget(wr::DisplayListBuilder&,
                                        wr::IpcResourceUpdateQueue&,
                                        const layers::StackingContextHelper&,
                                        layers::RenderRootStateManager*,
                                        nsIFrame*, StyleAppearance,
                                        const nsRect&) override;

  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;

  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                        StyleAppearance aAppearance,
                        LayoutDeviceIntMargin* aResult) override;

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                 StyleAppearance aAppearance,
                                 nsRect* aOverflowRect) override;

  LayoutDeviceIntSize GetMinimumWidgetSize(
      nsPresContext* aPresContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;

  virtual Transparency GetWidgetTransparency(
      nsIFrame* aFrame, StyleAppearance aAppearance) override;

  bool WidgetAttributeChangeRequiresRepaint(StyleAppearance, nsAtom*) override;
  void ThemeChanged() override;
  LayoutDeviceIntCoord GetScrollbarSize(const nsPresContext*,
                                        StyleScrollbarWidth, Overlay,
                                        const nsIFrame* = nullptr) override;
  bool ThemeSupportsScrollbarButtons() override { return true; }
  CSSCoord GetCheckboxRadioPrefSize(nsIFrame*) override;
  bool ThemeDrawsWidgetChildren(nsIFrame*, StyleAppearance) override;

  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                           StyleAppearance aAppearance) override;

  bool WidgetIsContainer(StyleAppearance aAppearance);

  bool ThemeDrawsFocusForWidget(nsIFrame*, StyleAppearance) override;

  bool ThemeNeedsComboboxDropmarker() override;

  bool WidgetAppearanceDependsOnWindowFocus(StyleAppearance) override;

  enum { eThemeGeometryTypeWindowButtons = eThemeGeometryTypeUnknown + 1 };
  ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame*,
                                               StyleAppearance) override;

  nsNativeThemeWin();

 protected:
  static bool ShouldUseNonNativeScrollbar(const nsIFrame*);
  bool ShouldUseNonNativeTheme(nsIFrame*, StyleAppearance);
  static ElementState GetContentState(nsIFrame*, StyleAppearance);
  static bool IsCheckedButton(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::checked);
  }
  static bool IsReadOnly(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::readonly);
  }
  static bool IsFocused(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::focused);
  }
  static bool IsSelectedTab(nsIFrame* aFrame) {
    return CheckBooleanAttr(aFrame, nsGkAtoms::visuallyselected) ||
           CheckBooleanAttr(aFrame, nsGkAtoms::selected);
  }
  static bool IsBottomTab(nsIFrame*);
  static bool IsNextToSelectedTab(nsIFrame*, int32_t aOffset);
  static bool IsLeftToSelectedTab(nsIFrame* aFrame) {
    return IsNextToSelectedTab(aFrame, IsFrameRTL(aFrame) ? 1 : -1);
  }
  static bool IsRightToSelectedTab(nsIFrame* aFrame) {
    return IsNextToSelectedTab(aFrame, IsFrameRTL(aFrame) ? -1 : 1);
  }

  Maybe<nsUXThemeClass> GetThemeClass(StyleAppearance aAppearance);
  HANDLE GetTheme(StyleAppearance aAppearance);
  nsresult GetThemePartAndState(nsIFrame* aFrame, StyleAppearance aAppearance,
                                int32_t& aPart, int32_t& aState);
  nsresult ClassicGetThemePartAndState(nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       int32_t& aPart, int32_t& aState,
                                       bool& aFocused);
  nsresult ClassicDrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       const nsRect& aRect,
                                       const nsRect& aClipRect);
  [[nodiscard]] LayoutDeviceIntMargin ClassicGetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance);
  bool ClassicGetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                               StyleAppearance aAppearance,
                               LayoutDeviceIntMargin* aResult);
  LayoutDeviceIntSize ClassicGetMinimumWidgetSize(nsIFrame* aFrame,
                                                  StyleAppearance aAppearance);
  bool ClassicThemeSupportsWidget(nsIFrame* aFrame,
                                  StyleAppearance aAppearance);
  void DrawCheckedRect(HDC hdc, const RECT& rc, int32_t fore, int32_t back,
                       HBRUSH defaultBack);
  uint32_t GetWidgetNativeDrawingFlags(StyleAppearance aAppearance);
  int32_t StandardGetState(nsIFrame* aFrame, StyleAppearance aAppearance,
                           bool wantFocused);
  bool IsMenuActive(nsIFrame* aFrame, StyleAppearance aAppearance);
  RECT CalculateProgressOverlayRect(nsIFrame* aFrame, RECT* aWidgetRect,
                                    bool aIsVertical, bool aIsIndeterminate,
                                    bool aIsClassic);
  void DrawThemedProgressMeter(nsIFrame* aFrame, StyleAppearance aAppearance,
                               HANDLE aTheme, HDC aHdc, int aPart, int aState,
                               RECT* aWidgetRect, RECT* aClipRect);

  [[nodiscard]] LayoutDeviceIntMargin GetCachedWidgetBorder(
      HANDLE aTheme, nsUXThemeClass aThemeClass, StyleAppearance aAppearance,
      int32_t aPart, int32_t aState);

  nsresult GetCachedMinimumWidgetSize(nsIFrame* aFrame, HANDLE aTheme,
                                      nsUXThemeClass aThemeClass,
                                      StyleAppearance aAppearance,
                                      int32_t aPart, int32_t aState,
                                      THEMESIZE aSizeReq,
                                      LayoutDeviceIntSize* aResult);

  SIZE GetCachedGutterSize(HANDLE theme);

 private:
  TimeStamp mProgressDeterminateTimeStamp;
  TimeStamp mProgressIndeterminateTimeStamp;

  // eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT is about 800 at the time of
  // writing this, and nsIntMargin is 16 bytes wide, which makes this cache (1/8
  // + 16) * 800 bytes, or about ~12KB. We could probably reduce this cache to
  // 3KB by caching on the aAppearance value instead, but there would be some
  // uncacheable values, since we derive some theme parts from other arguments.
  uint8_t
      mBorderCacheValid[(eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT + 7) /
                        8];
  LayoutDeviceIntMargin
      mBorderCache[eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT];

  // See the above not for mBorderCache and friends. However
  // LayoutDeviceIntSize is half the size of nsIntMargin, making the
  // cache roughly half as large. In total the caches should come to about 18KB.
  uint8_t mMinimumWidgetSizeCacheValid
      [(eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT + 7) / 8];
  LayoutDeviceIntSize
      mMinimumWidgetSizeCache[eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT];

  bool mGutterSizeCacheValid;
  SIZE mGutterSizeCache;
};

}  // namespace mozilla::widget

#endif
