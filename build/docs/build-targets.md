(build-targets)=

# Build Targets

When you build with `mach build`, there are some special targets that can be
built. This page attempts to document them.

## Larpfox theme archives

`mach build-theme` builds the UI resources and writes
`obj-theme-<platform>/dist/omni.ja`. It does not compile C++ or Rust, build
installers, or download a prebuilt browser. No previous browser build is
required. The command uses the source tree's manifests, preprocessors, and
JavaScript generators, so edits to HTML/XHTML, CSS, JavaScript, and localization
files are included.

The default platform is the host platform. Use `mach build-theme --platform
windows`, `--platform linux`, or `--platform macos` to select a different target.
These select the x86-64 Windows/Linux or ARM64 macOS resource configuration,
including platform-specific files and preprocessor branches; they do not
cross-compile native binaries.

Theme builds use a separate generated mozconfig and object directory for each
platform, leaving the normal build configuration untouched. Python, Node.js,
make, and the usual Firefox frontend build dependencies must be available
(`mach bootstrap` can install them). Build themes from the same Larpfox revision
and for the same platform as the browser that will load them: privileged UI
JavaScript and native interfaces must remain compatible.

### Installing and selecting a theme

1. Open the profile directory from `about:support` and create `lfx-themes`.
2. Copy the matching platform's `omni.ja` there as, for example, `echelon.ja`.
3. Set the string preference `larpfox.current-theme` to `echelon` in
   `about:config`, then restart Larpfox.

The preference is a filename basename without `.ja`, not a path. An empty
value selects the built-in UI. `user.js` can also set the preference and
overrides the saved value in `prefs.js`, following normal startup precedence.

Theme archives replace both toolkit and browser resources before privileged
startup JavaScript loads, including in content processes. Switching themes,
replacing an archive, or returning to the built-in UI invalidates startup
caches using the archive's path, modification time, and size.

Safe Mode (Troubleshoot Mode) ignores the preference and loads the built-in UI,
without clearing the saved selection. Missing archives, invalid names, and
archives with missing or ZIP-corrupt startup entries fall back to the built-in
UI. If a theme's code prevents startup, use Safe Mode to recover.

**Only install themes you trust.** These archives contain privileged JavaScript
with the browser's own permissions; they are not sandboxed WebExtension themes.

## Partial Tree Targets

The targets in this section only build part of the tree. Please note that
partial tree builds can be unreliable. Use at your own risk.

export

: Build the *export* tier. The *export* tier builds everything that is
  required for C/C++ compilation. It stages all header files, processes
  IDLs, etc.

compile

: Build the *compile* tier. The *compile* tier compiles all C/C++ files.

libs

: Build the *libs* tier. The *libs* tier performs linking and performs
  most build steps which aren't related to compilation.

tools

: Build the *tools* tier. The *tools* tier mostly deals with supplementary
  tools and compiled tests. It will link tools against libXUL, including
  compiled test binaries.

binaries:

: Recompiles and relinks C/C++ files. Only works after a complete normal
  build, but allows for much faster rebuilds of C/C++ code. For performance
  reasons, however, it skips nss, nspr, icu and ffi. This is targeted to
  improve local developer workflow when touching C/C++ code.

install-manifests

: Process install manifests. Install manifests handle the installation of
  files into the object directory.

  Unless `NO_REMOVE=1` is defined in the environment, files not accounted
  in the install manifests will be deleted from the object directory.

install-tests

: Processes the tests install manifest.

## Common Actions

The targets in this section correspond to common build-related actions. Many
of the actions in this section are effectively frontends to shell scripts.
These actions will likely all be replaced by mach commands someday.

buildsymbols

: Create a symbols archive for the current build.

  This must be performed after a successful build.

check

: Run build system tests.
