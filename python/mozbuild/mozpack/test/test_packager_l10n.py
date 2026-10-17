# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest

import mozunit

from mozpack.chrome.manifest import Manifest, ManifestContent, ManifestLocale
from mozpack.copier import FileCopier, FileRegistry
from mozpack.files import GeneratedFile, ManifestFile
from mozpack.packager import l10n
from mozpack.packager.formats import FlatFormatter, OmniJarFormatter
from mozpack.packager.unpack import UnpackFinder
from mozpack.test.test_files import TestWithTmpDir
from test_packager import MockFinder


class TestL10NRepack(unittest.TestCase):
    def test_l10n_repack(self):
        foo = GeneratedFile(b"foo")
        foobar = GeneratedFile(b"foobar")
        qux = GeneratedFile(b"qux")
        bar = GeneratedFile(b"bar")
        baz = GeneratedFile(b"baz")
        dict_aa = GeneratedFile(b"dict_aa")
        dict_bb = GeneratedFile(b"dict_bb")
        dict_cc = GeneratedFile(b"dict_cc")
        barbaz = GeneratedFile(b"barbaz")
        lst = GeneratedFile(b"foo\nbar")
        app_finder = MockFinder({
            "bar/foo": foo,
            "chrome/foo/foobar": foobar,
            "chrome/qux/qux.properties": qux,
            "chrome/qux/baz/baz.properties": baz,
            "chrome/chrome.manifest": ManifestFile(
                "chrome",
                [
                    ManifestContent("chrome", "foo", "foo/"),
                    ManifestLocale("chrome", "qux", "en-US", "qux/"),
                ],
            ),
            "chrome.manifest": ManifestFile(
                "", [Manifest("", "chrome/chrome.manifest")]
            ),
            "dict/aa": dict_aa,
            "app/chrome/bar/barbaz.dtd": barbaz,
            "app/chrome/chrome.manifest": ManifestFile(
                "app/chrome", [ManifestLocale("app/chrome", "bar", "en-US", "bar/")]
            ),
            "app/chrome.manifest": ManifestFile(
                "app", [Manifest("app", "chrome/chrome.manifest")]
            ),
            "app/dict/bb": dict_bb,
            "app/dict/cc": dict_cc,
            "app/chrome/bar/search/foo.xml": foo,
            "app/chrome/bar/search/bar.xml": bar,
            "app/chrome/bar/search/lst.txt": lst,
            "META-INF/foo": foo,  # Stripped.
            "inner/META-INF/foo": foo,  # Not stripped.
            "app/META-INF/foo": foo,  # Stripped.
            "app/inner/META-INF/foo": foo,  # Not stripped.
        })
        app_finder.jarlogs = {}
        app_finder.base = "app"
        foo_l10n = GeneratedFile(b"foo_l10n")
        qux_l10n = GeneratedFile(b"qux_l10n")
        baz_l10n = GeneratedFile(b"baz_l10n")
        barbaz_l10n = GeneratedFile(b"barbaz_l10n")
        lst_l10n = GeneratedFile(b"foo\nqux")
        l10n_finder = MockFinder({
            "chrome/qux-l10n/qux.properties": qux_l10n,
            "chrome/qux-l10n/baz/baz.properties": baz_l10n,
            "chrome/chrome.manifest": ManifestFile(
                "chrome",
                [
                    ManifestLocale("chrome", "qux", "x-test", "qux-l10n/"),
                ],
            ),
            "chrome.manifest": ManifestFile(
                "", [Manifest("", "chrome/chrome.manifest")]
            ),
            "dict/bb": dict_bb,
            "dict/cc": dict_cc,
            "app/chrome/bar-l10n/barbaz.dtd": barbaz_l10n,
            "app/chrome/chrome.manifest": ManifestFile(
                "app/chrome",
                [ManifestLocale("app/chrome", "bar", "x-test", "bar-l10n/")],
            ),
            "app/chrome.manifest": ManifestFile(
                "app", [Manifest("app", "chrome/chrome.manifest")]
            ),
            "app/dict/aa": dict_aa,
            "app/chrome/bar-l10n/search/foo.xml": foo_l10n,
            "app/chrome/bar-l10n/search/qux.xml": qux_l10n,
            "app/chrome/bar-l10n/search/lst.txt": lst_l10n,
        })
        l10n_finder.base = "l10n"
        copier = FileRegistry()
        formatter = l10n.FlatFormatter(copier)

        l10n._repack(
            app_finder,
            l10n_finder,
            copier,
            formatter,
            ["dict", "chrome/**/search/*.xml"],
        )
        self.maxDiff = None

        repacked = {
            "bar/foo": foo,
            "chrome/foo/foobar": foobar,
            "chrome/qux-l10n/qux.properties": qux_l10n,
            "chrome/qux-l10n/baz/baz.properties": baz_l10n,
            "chrome/chrome.manifest": ManifestFile(
                "chrome",
                [
                    ManifestContent("chrome", "foo", "foo/"),
                    ManifestLocale("chrome", "qux", "x-test", "qux-l10n/"),
                ],
            ),
            "chrome.manifest": ManifestFile(
                "", [Manifest("", "chrome/chrome.manifest")]
            ),
            "dict/bb": dict_bb,
            "dict/cc": dict_cc,
            "app/chrome/bar-l10n/barbaz.dtd": barbaz_l10n,
            "app/chrome/chrome.manifest": ManifestFile(
                "app/chrome",
                [ManifestLocale("app/chrome", "bar", "x-test", "bar-l10n/")],
            ),
            "app/chrome.manifest": ManifestFile(
                "app", [Manifest("app", "chrome/chrome.manifest")]
            ),
            "app/dict/aa": dict_aa,
            "app/chrome/bar-l10n/search/foo.xml": foo_l10n,
            "app/chrome/bar-l10n/search/qux.xml": qux_l10n,
            "app/chrome/bar-l10n/search/lst.txt": lst_l10n,
            "inner/META-INF/foo": foo,
            "app/inner/META-INF/foo": foo,
        }

        self.assertEqual(
            dict((p, f.open().read()) for p, f in copier),
            dict((p, f.open().read()) for p, f in repacked.items()),
        )


class TestUnifiedL10NRepack(TestWithTmpDir):
    def test_unified_browser_repack(self):
        source = os.path.join(self.tmpdir, "source")
        localized = os.path.join(self.tmpdir, "localized")
        for destination, locale in ((source, "en-US"), (localized, "fr")):
            copier = FileCopier()
            formatter = (
                OmniJarFormatter(copier, "omni.ja", unify_browser=True)
                if destination == source
                else FlatFormatter(copier)
            )
            formatter.add_base("")
            formatter.add_base("browser")
            for base, package in (("", "toolkit"), ("browser", "browser")):
                formatter.add_manifest(
                    ManifestLocale(base, package, locale, "chrome/%s/" % locale)
                )
                path = "/".join(p for p in (base, "chrome", locale, "test.ftl") if p)
                formatter.add(path, GeneratedFile(locale.encode()))
            copier.copy(destination)

        l10n.repack(source, localized)

        self.assertTrue(os.path.isfile(os.path.join(source, "omni.ja")))
        self.assertFalse(os.path.exists(os.path.join(source, "browser", "omni.ja")))
        finder = UnpackFinder(source, omnijar_name="omni.ja")
        self.assertTrue(finder.unify_browser)
        self.assertTrue(finder.files.contains("browser/chrome.manifest"))
        for base in ("", "browser/"):
            self.assertEqual(
                finder.files[base + "chrome/fr/test.ftl"].open().read(), b"fr"
            )
            self.assertFalse(finder.files.contains(base + "chrome/en-US/test.ftl"))


if __name__ == "__main__":
    mozunit.main()
