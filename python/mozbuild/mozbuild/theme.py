# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import sys
from pathlib import Path

from mozpack.copier import FileCopier
from mozpack.errors import errors
from mozpack.files import FileFinder
from mozpack.packager import SimplePackager
from mozpack.packager.formats import OmniJarFormatter

from mozbuild.backend.configenvironment import ConfigEnvironment

PLATFORMS = {
    "windows": "x86_64-pc-windows-msvc",
    "linux": "x86_64-pc-linux-gnu",
    "macos": "aarch64-apple-darwin",
}


def build_theme(topsrcdir, platform=None):
    if platform is None:
        platform = {"win32": "windows", "linux": "linux", "darwin": "macos"}.get(
            sys.platform
        )
    if platform not in PLATFORMS:
        raise ValueError("Select a theme platform: windows, linux, or macos")

    source = Path(topsrcdir)
    objdir = source / f"obj-theme-{platform}"
    objdir.mkdir(exist_ok=True)
    mozconfig = objdir / "theme.mozconfig"
    mozconfig.write_text(
        f'mk_add_options MOZ_OBJDIR="{objdir.as_posix()}"\n'
        "ac_add_options --enable-application=browser\n"
        "ac_add_options --disable-tests\n"
        "ac_add_options --enable-theme-build\n"
        f"ac_add_options --target={PLATFORMS[platform]}\n",
        encoding="utf-8",
    )
    env = dict(os.environ, MOZCONFIG=str(mozconfig))
    result = subprocess.call(
        [sys.executable, str(source / "mach"), "configure"], cwd=source, env=env
    )
    if result:
        return result

    config = ConfigEnvironment.from_config_status(str(objdir / "config.status"))
    # Invoke only the resource graph, never mach's artifact installation or
    # native build targets. The artifact configuration disables compilation.
    result = subprocess.call(
        [
            config.substs["GMAKE"],
            "-C",
            str(objdir / "faster"),
            f"-j{os.cpu_count() or 1}",
            "NO_XPIDL=1",
            "FASTER_RECURSIVE_MAKE=1",
        ],
        cwd=source,
        env=env,
    )
    if result:
        return result

    copier = FileCopier()
    formatter = OmniJarFormatter(copier, "omni.ja", unify_browser=True)
    with errors.accumulate():
        packager = SimplePackager(formatter)
        for name, file in FileFinder(str(objdir / "dist" / "bin")):
            packager.add(name, file)
        packager.close()

    output = objdir / "dist" / "omni.ja"
    copier["omni.ja"].copy(str(output))
    print(f"Theme archive ({platform}): {output}")
    return 0
