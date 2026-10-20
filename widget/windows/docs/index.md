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
chrome stylesheets. Glass and DWM-composited appearances are not implemented.

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

There is no `widget.non-native-theme.enabled` preference or enablement gate.

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
custom colors, stable gutters, and live preference and style changes:

```sh
./mach mochitest --flavor browser \
  widget/windows/tests/browser/browser_native_control_padding.js \
  widget/windows/tests/browser/browser_native_control_subparts.js \
  widget/windows/tests/browser/browser_native_controls_content.js \
  widget/windows/tests/browser/browser_native_scrollbar_modes.js
```

Visual verification also needs a running Windows desktop: check themed and
classic controls, theme-cache refresh, input interaction, animated progress,
RTL layout, and scaled rendering. Windows 11, OS high-contrast themes, and
cross-monitor DPI transitions require separate desktop coverage.
