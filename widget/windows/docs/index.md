# Firefox on Windows

```{toctree}
:maxdepth: 2

blocklist
shell/pin-to-taskbar
shell/set-default
notification-server/index
windows-pointing-device/index
LimitedAccessFeature
```

## Native controls

Windows uses `nsNativeThemeWin` for both browser chrome and web content.
The backend restores Firefox 78's non-glass control appearances using current
theme and layout interfaces: form controls, scrollbars, ranges, progress meters,
menus, tabs, trees, toolbars, status bars, tooltips, window frames, and caption
buttons. Legacy chrome-only appearance keywords remain restricted to UA and
chrome stylesheets. Firefox 115's Aero glass support is available only to custom
chrome themes; the default browser UI does not enable it.

`nsUXThemeData` owns the shared UXTheme handles and native metrics. Theme changes
invalidate these caches in both parent and content processes. Controls fall back
to classic GDI drawing when their visual-style theme is unavailable.

Light controls use the Windows palette. Dark form controls use the existing
non-native renderer and dark system colors, including its sizing and focus
indicators. Explicit author colors and CSS-only form-control styling remain
available. Native ranges and progress bars paint their separate subparts;
non-native parents paint their own thumbs and fills. Author padding overrides
native padding.

`widget.non-native-theme.scrollbar.enabled` selects the Windows scrollbar
renderer independently of other controls:

| Value | Behavior |
| --- | --- |
| `0` | Always native, including dark and custom-colored scrollbars. |
| `1` | Always non-native. |
| `2` (default) | Native unless the scrollbar is dark or its CSS `scrollbar-color` is non-`auto`. |

Other integer values behave like `2`. Dark scrollbar detection uses the existing
non-native scrollbar color-scheme policy, including background-based detection.
Preference and style changes update both painting and layout, including stable
scrollbar gutters. Non-native scrollbars retain the existing style and size
preferences. Native scrollbar minimum sizes include the whole bar's thickness
and button space; painted borders do not add layout borders that would hide
oversized thumbs.

The Windows 10-style non-native renderer uses the pre-`ScrollbarDrawing`
appearance: thumbs and buttons are inset by one CSS pixel on each side,
rounded to device pixels, and the legacy arrow shapes are drawn without
antialiasing. High-contrast button outlines are retained. These painted insets
do not shrink the scrollbar hit areas or reserved layout space.

There is no `widget.non-native-theme.enabled` preference or enablement gate.

### Opt-in Aero glass

Privileged stylesheets can select `-moz-win-borderless-glass` on the window
root. Its CSS background color is retained, so a theme can use transparency or
a translucent tint. Root opacity effects and rounded corners still take
precedence over glass, as in Firefox 115.

The `-moz-win-exclude-glass` default-appearance marker limits the opaque region
used for DWM frame margins. It must be set through `-moz-default-appearance`;
it remains effective even with `appearance: none`. With no exclusions, DWM
extends a full sheet of glass behind the window.

A custom theme can use these hooks:

```css
:root {
  appearance: auto !important;
  -moz-default-appearance: -moz-win-borderless-glass !important;
  box-sizing: border-box !important;
  background-color: transparent !important;
}

.titlebar-buttonbox {
  appearance: -moz-window-button-box !important;
  align-self: start !important;
  margin: 0 !important;
}

:root[sizemode="maximized"] .titlebar-buttonbox {
  appearance: -moz-window-button-box-maximized !important;
}

.titlebar-button {
  display: none !important;
}

#tabbrowser-tabbox {
  -moz-default-appearance: -moz-win-exclude-glass !important;
}
```

The theme must also make its toolbar backgrounds transparent where glass should
show. Native caption rendering requires DWM composition and a usable Windows
`WINDOW` theme. Frame materials and blur come from Windows, not a Firefox blur
implementation. Normal windows retain their existing captions and rendering.

Native caption input bounds are tracked per HWND in client device pixels.
In restored windows, the visual exclusion follows the caption box's layout
height, as in Firefox 115, rather than DWM's larger caption envelope.
A root WebRender clip-out mask excludes the caption area from all browser
content, including filtered and transformed surfaces. This replaces Firefox
115's late clear primitive, which native-compositor paths did not execute.
Compositor alpha and opaque-region accounting include the exclusion, and
geometry changes replace the root scene rather than relying on an invalid-region
update.

Maximized glass extends the Win32 client through the top resize frame so DWM can
track native caption hover. The glass root reserves that frame inset for layout,
and the root scene clips the hidden strip to prevent browser pixels bleeding
onto adjacent displays. A full-height theme root needs `box-sizing: border-box`
so the inset does not push content below the viewport.

Native caption input first uses `DwmDefWindowProc`. When Windows declines a hit
test, cached native button bounds provide the fallback; Windows still handles
button tracking and window commands.

Glass exclusions are stored in window-root coordinates, including inside
transformed stacking contexts. Actual painted opacity and the subset used for
DWM margins are published together for the same client extent. Native clearing
uses actual opacity, so opaque toolbars and page content are not erased merely
because glass extends behind them.

Glass resizing keeps the last valid DWM edge margins until layout publishes
opacity for the new client size. Borderless glass uses Firefox 115's two-pixel
border adjustment, not the full native resize-frame thickness. Native painting
and ordinary resize composites remain enabled; size-dependent safe-client clips
still require a fresh root scene.

#### Caption mask controls

These integer preferences under `widget.native-controls.` update existing
windows without restarting:

| Preference suffix | Effect |
| --- | --- |
| `override-aero-caption-buttons-mask-width` | Override caption-box width metrics and the visual mask width. |
| `override-aero-caption-buttons-mask-height` | Override caption-box height metrics and the visual mask height. |
| `aero-caption-buttons-mask-radius` | Round the physical bottom-left corner in restored and maximized windows. |
| `aero-caption-buttons-mask-radius-right` | Round the physical bottom-right corner in maximized windows only. |

All four default to `0`. Nonpositive dimensions use native metrics; nonpositive
radii leave square corners. Dimensions are system-DPI device pixels before the
existing frame-border adjustments and per-window DPI scaling. Height loses the
resize-frame and padded-border thickness; maximized boxes additionally gain
one pixel of width and lose two pixels of height. Positive overrides also
change the caption box's reserved layout space, not Windows' actual buttons.

Radii are client device pixels after scaling, limited to half the mask width
and the mask height. Top corners remain square. The mask stays anchored to the
native outer edge; its maximized top remains at the native caption origin.
These controls do not change native caption hit areas or window commands.

### Content sandbox tradeoff

Native content rendering requires Win32 USER/GDI and UXTheme access. Content
processes therefore report `MissingNonNativeTheming` for Win32k lockdown and use
an alternate desktop on the interactive window station. Their access token is
`USER_LIMITED`, including at configured sandbox levels that upstream Firefox
would give stricter tokens. Higher levels retain untrusted integrity, and the
existing job, file-access, and other mitigation policies remain in place.

This is intentionally weaker than upstream Firefox's content sandbox. A configured
level of 9 does not imply upstream level-9 token isolation in this build.

### Verification

The Windows browser tests cover author padding, independently styled native
range and progress subparts, dark non-native colors and parent painting,
normal/thin/hidden native scrollbar geometry, all three scrollbar modes,
custom colors, stable gutters, live preference and style changes, inset thumbs,
crisp arrows pointing in all four directions, and glass caption masking across
filtered content, partial updates, caption-height changes, resize, maximize,
restore, and glass-mode removal. They also cover live size overrides, native
metric restoration, radius-only updates, bounded corners, and the maximized-only
right radius. Maximized checks cover the hidden frame strip and the browser
content's viewport boundary:

```sh
./mach mochitest --flavor browser \
  widget/windows/tests/browser/browser_native_aero_glass.js \
  widget/windows/tests/browser/browser_native_control_padding.js \
  widget/windows/tests/browser/browser_native_control_subparts.js \
  widget/windows/tests/browser/browser_native_controls_content.js \
  widget/windows/tests/browser/browser_native_scrollbar_modes.js
```

The glass test needs real native windows and is skipped in headless mode.
Repeat it with hardware ANGLE/layer/WGL and software WebRender configurations;
native button pixels and mouse interaction also require an OS-level capture
and input check, not just a WebRender snapshot.

Visual verification also needs a running Windows desktop: check themed and
classic controls, theme-cache refresh, input interaction, animated progress,
RTL layout, and scaled rendering. Windows 11, OS high-contrast themes, and
cross-monitor DPI transitions require separate desktop coverage.
