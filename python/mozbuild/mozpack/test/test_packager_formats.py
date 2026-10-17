# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
from itertools import chain

import mozunit

import mozpack.path as mozpath
from mozpack.chrome.manifest import (
    ManifestBinaryComponent,
    ManifestComponent,
    ManifestContent,
    ManifestLocale,
    ManifestResource,
    ManifestSkin,
)
from mozpack.copier import FileRegistry
from mozpack.errors import ErrorMessage
from mozpack.files import GeneratedFile, ManifestFile
from mozpack.packager.formats import FlatFormatter, JarFormatter, OmniJarFormatter
from mozpack.test.test_files import bar_xpt, foo2_xpt, foo_xpt
from test_errors import TestErrors

CONTENTS = {
    "bases": {
        # base_path: is_addon?
        "": False,
        "app": False,
        "addon0": "unpacked",
        "addon1": True,
        "app/chrome/addons/addon2": True,
    },
    "manifests": [
        ManifestContent("chrome/f", "oo", "oo/"),
        ManifestContent("chrome/f", "bar", "oo/bar/"),
        ManifestResource("chrome/f", "foo", "resource://bar/"),
        ManifestBinaryComponent("components", "foo.so"),
        ManifestContent("app/chrome", "content", "foo/"),
        ManifestComponent("app/components", "{foo-id}", "foo.js"),
        ManifestContent("addon0/chrome", "addon0", "foo/bar/"),
        ManifestContent("addon1/chrome", "addon1", "foo/bar/"),
        ManifestContent("app/chrome/addons/addon2/chrome", "addon2", "foo/bar/"),
    ],
    "files": {
        "chrome/f/oo/bar/baz": GeneratedFile(b"foobarbaz"),
        "chrome/f/oo/baz": GeneratedFile(b"foobaz"),
        "chrome/f/oo/qux": GeneratedFile(b"fooqux"),
        "components/foo.so": GeneratedFile(b"foo.so"),
        "components/foo.xpt": foo_xpt,
        "components/bar.xpt": bar_xpt,
        "foo": GeneratedFile(b"foo"),
        "app/chrome/foo/foo": GeneratedFile(b"appfoo"),
        "app/components/foo.js": GeneratedFile(b"foo.js"),
        "addon0/chrome/foo/bar/baz": GeneratedFile(b"foobarbaz"),
        "addon0/components/foo.xpt": foo2_xpt,
        "addon0/components/bar.xpt": bar_xpt,
        "addon1/chrome/foo/bar/baz": GeneratedFile(b"foobarbaz"),
        "addon1/components/foo.xpt": foo2_xpt,
        "addon1/components/bar.xpt": bar_xpt,
        "app/chrome/addons/addon2/chrome/foo/bar/baz": GeneratedFile(b"foobarbaz"),
        "app/chrome/addons/addon2/components/foo.xpt": foo2_xpt,
        "app/chrome/addons/addon2/components/bar.xpt": bar_xpt,
    },
}

FILES = CONTENTS["files"]

RESULT_FLAT = {
    "chrome.manifest": [
        "manifest chrome/chrome.manifest",
        "manifest components/components.manifest",
    ],
    "chrome/chrome.manifest": [
        "manifest f/f.manifest",
    ],
    "chrome/f/f.manifest": [
        "content oo oo/",
        "content bar oo/bar/",
        "resource foo resource://bar/",
    ],
    "chrome/f/oo/bar/baz": FILES["chrome/f/oo/bar/baz"],
    "chrome/f/oo/baz": FILES["chrome/f/oo/baz"],
    "chrome/f/oo/qux": FILES["chrome/f/oo/qux"],
    "components/components.manifest": [
        "binary-component foo.so",
        "interfaces bar.xpt",
        "interfaces foo.xpt",
    ],
    "components/foo.so": FILES["components/foo.so"],
    "components/foo.xpt": foo_xpt,
    "components/bar.xpt": bar_xpt,
    "foo": FILES["foo"],
    "app/chrome.manifest": [
        "manifest chrome/chrome.manifest",
        "manifest components/components.manifest",
    ],
    "app/chrome/chrome.manifest": [
        "content content foo/",
    ],
    "app/chrome/foo/foo": FILES["app/chrome/foo/foo"],
    "app/components/components.manifest": [
        "component {foo-id} foo.js",
    ],
    "app/components/foo.js": FILES["app/components/foo.js"],
}

for addon in ("addon0", "addon1", "app/chrome/addons/addon2"):
    RESULT_FLAT.update({
        mozpath.join(addon, p): f
        for p, f in {
            "chrome.manifest": [
                "manifest chrome/chrome.manifest",
                "manifest components/components.manifest",
            ],
            "chrome/chrome.manifest": ["content %s foo/bar/" % mozpath.basename(addon)],
            "chrome/foo/bar/baz": FILES[mozpath.join(addon, "chrome/foo/bar/baz")],
            "components/components.manifest": [
                "interfaces bar.xpt",
                "interfaces foo.xpt",
            ],
            "components/bar.xpt": bar_xpt,
            "components/foo.xpt": foo2_xpt,
        }.items()
    })

RESULT_JAR = {
    p: RESULT_FLAT[p]
    for p in (
        "chrome.manifest",
        "chrome/chrome.manifest",
        "components/components.manifest",
        "components/foo.so",
        "components/foo.xpt",
        "components/bar.xpt",
        "foo",
        "app/chrome.manifest",
        "app/components/components.manifest",
        "app/components/foo.js",
        "addon0/chrome.manifest",
        "addon0/components/components.manifest",
        "addon0/components/foo.xpt",
        "addon0/components/bar.xpt",
    )
}

RESULT_JAR.update({
    "chrome/f/f.manifest": [
        "content oo jar:oo.jar!/",
        "content bar jar:oo.jar!/bar/",
        "resource foo resource://bar/",
    ],
    "chrome/f/oo.jar": {
        "bar/baz": FILES["chrome/f/oo/bar/baz"],
        "baz": FILES["chrome/f/oo/baz"],
        "qux": FILES["chrome/f/oo/qux"],
    },
    "app/chrome/chrome.manifest": [
        "content content jar:foo.jar!/",
    ],
    "app/chrome/foo.jar": {
        "foo": FILES["app/chrome/foo/foo"],
    },
    "addon0/chrome/chrome.manifest": [
        "content addon0 jar:foo.jar!/bar/",
    ],
    "addon0/chrome/foo.jar": {
        "bar/baz": FILES["addon0/chrome/foo/bar/baz"],
    },
    "addon1.xpi": {
        mozpath.relpath(p, "addon1"): f
        for p, f in RESULT_FLAT.items()
        if p.startswith("addon1/")
    },
    "app/chrome/addons/addon2.xpi": {
        mozpath.relpath(p, "app/chrome/addons/addon2"): f
        for p, f in RESULT_FLAT.items()
        if p.startswith("app/chrome/addons/addon2/")
    },
})

RESULT_OMNIJAR = {
    p: RESULT_FLAT[p]
    for p in (
        "components/foo.so",
        "foo",
    )
}

RESULT_OMNIJAR.update({p: RESULT_JAR[p] for p in RESULT_JAR if p.startswith("addon")})

RESULT_OMNIJAR.update({
    "omni.foo": {
        "components/components.manifest": [
            "interfaces bar.xpt",
            "interfaces foo.xpt",
        ],
    },
    "chrome.manifest": [
        "manifest components/components.manifest",
    ],
    "components/components.manifest": [
        "binary-component foo.so",
    ],
    "app/omni.foo": {
        p: RESULT_FLAT["app/" + p]
        for p in chain(
            (
                "chrome.manifest",
                "chrome/chrome.manifest",
                "chrome/foo/foo",
                "components/components.manifest",
                "components/foo.js",
            ),
            (
                mozpath.relpath(p, "app")
                for p in RESULT_FLAT.keys()
                if p.startswith("app/chrome/addons/addon2/")
            ),
        )
    },
})

RESULT_OMNIJAR["omni.foo"].update({
    p: RESULT_FLAT[p]
    for p in (
        "chrome.manifest",
        "chrome/chrome.manifest",
        "chrome/f/f.manifest",
        "chrome/f/oo/bar/baz",
        "chrome/f/oo/baz",
        "chrome/f/oo/qux",
        "components/foo.xpt",
        "components/bar.xpt",
    )
})

RESULT_OMNIJAR_WITH_SUBPATH = {
    k.replace("omni.foo", "bar/omni.foo"): v for k, v in RESULT_OMNIJAR.items()
}

CONTENTS_WITH_BASE = {
    "bases": {
        mozpath.join("base/root", b) if b else "base/root": a
        for b, a in CONTENTS["bases"].items()
    },
    "manifests": [
        m.move(mozpath.join("base/root", m.base)) for m in CONTENTS["manifests"]
    ],
    "files": {mozpath.join("base/root", p): f for p, f in CONTENTS["files"].items()},
}

EXTRA_CONTENTS = {
    "extra/file": GeneratedFile(b"extra file"),
}

CONTENTS_WITH_BASE["files"].update(EXTRA_CONTENTS)


def result_with_base(results):
    result = {mozpath.join("base/root", p): v for p, v in results.items()}
    result.update(EXTRA_CONTENTS)
    return result


RESULT_FLAT_WITH_BASE = result_with_base(RESULT_FLAT)
RESULT_JAR_WITH_BASE = result_with_base(RESULT_JAR)
RESULT_OMNIJAR_WITH_BASE = result_with_base(RESULT_OMNIJAR)

CONTENTS_WITH_BROWSER = {
    "bases": {b.replace("app", "browser", 1): a for b, a in CONTENTS["bases"].items()},
    "manifests": [
        m.move(m.base.replace("app", "browser", 1)) for m in CONTENTS["manifests"]
    ],
    "files": {p.replace("app", "browser", 1): f for p, f in CONTENTS["files"].items()},
}


def fill_formatter(formatter, contents):
    for base, is_addon in sorted(contents["bases"].items()):
        formatter.add_base(base, is_addon)

    for manifest in contents["manifests"]:
        formatter.add_manifest(manifest)

    for k, v in sorted(contents["files"].items()):
        if k.endswith(".xpt"):
            formatter.add_interfaces(k, v)
        else:
            formatter.add(k, v)


def get_contents(registry, read_all=False, mode="rt"):
    result = {}
    for k, v in registry:
        if isinstance(v, FileRegistry):
            result[k] = get_contents(v, read_all=read_all, mode=mode)
        elif isinstance(v, ManifestFile) or read_all:
            if "b" in mode:
                result[k] = v.open().read()
            else:
                result[k] = v.open().read().decode().splitlines()
        else:
            result[k] = v
    return result


class TestFormatters(TestErrors, unittest.TestCase):
    maxDiff = None

    def test_bases(self):
        formatter = FlatFormatter(FileRegistry())
        formatter.add_base("")
        formatter.add_base("addon0", addon=True)
        formatter.add_base("browser")
        self.assertEqual(formatter._get_base("platform.ini"), ("", "platform.ini"))
        self.assertEqual(
            formatter._get_base("browser/application.ini"),
            ("browser", "application.ini"),
        )
        self.assertEqual(
            formatter._get_base("addon0/install.rdf"), ("addon0", "install.rdf")
        )

    def do_test_contents(self, formatter, contents):
        for f in contents["files"]:
            # .xpt files are merged, so skip them.
            if not f.endswith(".xpt"):
                self.assertTrue(formatter.contains(f))

    def test_flat_formatter(self):
        registry = FileRegistry()
        formatter = FlatFormatter(registry)

        fill_formatter(formatter, CONTENTS)
        self.assertEqual(get_contents(registry), RESULT_FLAT)
        self.do_test_contents(formatter, CONTENTS)

    def test_jar_formatter(self):
        registry = FileRegistry()
        formatter = JarFormatter(registry)

        fill_formatter(formatter, CONTENTS)
        self.assertEqual(get_contents(registry), RESULT_JAR)
        self.do_test_contents(formatter, CONTENTS)

    def test_omnijar_formatter(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(registry, "omni.foo")

        fill_formatter(formatter, CONTENTS)
        self.assertEqual(get_contents(registry), RESULT_OMNIJAR)
        self.do_test_contents(formatter, CONTENTS)

    def test_flat_formatter_with_base(self):
        registry = FileRegistry()
        formatter = FlatFormatter(registry)

        fill_formatter(formatter, CONTENTS_WITH_BASE)
        self.assertEqual(get_contents(registry), RESULT_FLAT_WITH_BASE)
        self.do_test_contents(formatter, CONTENTS_WITH_BASE)

    def test_jar_formatter_with_base(self):
        registry = FileRegistry()
        formatter = JarFormatter(registry)

        fill_formatter(formatter, CONTENTS_WITH_BASE)
        self.assertEqual(get_contents(registry), RESULT_JAR_WITH_BASE)
        self.do_test_contents(formatter, CONTENTS_WITH_BASE)

    def test_omnijar_formatter_with_base(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(registry, "omni.foo")

        fill_formatter(formatter, CONTENTS_WITH_BASE)
        self.assertEqual(get_contents(registry), RESULT_OMNIJAR_WITH_BASE)
        self.do_test_contents(formatter, CONTENTS_WITH_BASE)

    def test_omnijar_formatter_with_subpath(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(registry, "bar/omni.foo")

        fill_formatter(formatter, CONTENTS)
        self.assertEqual(get_contents(registry), RESULT_OMNIJAR_WITH_SUBPATH)
        self.do_test_contents(formatter, CONTENTS)

    def test_omnijar_unified_browser(self):
        expected = {
            p.replace("app", "browser", 1): f for p, f in RESULT_OMNIJAR.items()
        }
        expected["omni.foo"] = dict(expected["omni.foo"])
        expected["omni.foo"].update({
            mozpath.join("browser", p): f
            for p, f in expected.pop("browser/omni.foo").items()
        })
        for base in ("", "Firefox.app/Contents/Resources"):
            with self.subTest(base=base):
                contents = {
                    "bases": {
                        (mozpath.join(base, b) if b else base): a
                        for b, a in CONTENTS_WITH_BROWSER["bases"].items()
                    },
                    "manifests": [
                        m.move(mozpath.join(base, m.base) if m.base else base)
                        for m in CONTENTS_WITH_BROWSER["manifests"]
                    ],
                    "files": {
                        mozpath.join(base, p): f
                        for p, f in CONTENTS_WITH_BROWSER["files"].items()
                    },
                }
                registry = FileRegistry()
                formatter = OmniJarFormatter(registry, "omni.foo", unify_browser=True)
                fill_formatter(formatter, contents)
                self.assertEqual(
                    get_contents(registry),
                    {mozpath.join(base, p): f for p, f in expected.items()},
                )
                self.do_test_contents(formatter, contents)

    def test_omnijar_unified_browser_non_resources(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(
            registry, "omni.foo", non_resources=["modules/external.js"],
            unify_browser=True,
        )
        formatter.add_base("")
        formatter.add_base("browser")
        formatter.add_manifest(ManifestContent("", "toolkit", "chrome/toolkit/"))
        formatter.add_manifest(ManifestContent("browser", "browser", "chrome/browser/"))
        formatter.add_manifest(ManifestBinaryComponent("browser/components", "foo.so"))
        external = (
            "browser/components/foo.so",
            "browser/chrome/icons/default/default.ico",
            "browser/defaults/preferences/channel-prefs.js",
            "browser/modules/external.js",
            "browser/application.ini",
        )
        for path in external:
            formatter.add(path, GeneratedFile(path.encode()))
            self.assertTrue(registry.contains(path))
            self.assertTrue(formatter.contains(path))
            self.assertFalse(registry["omni.foo"].contains(path))
        formatter.add("browser/modules/internal.js", GeneratedFile(b"internal"))
        self.assertTrue(registry["omni.foo"].contains("browser/modules/internal.js"))
        self.assertFalse(registry.contains("browser/omni.foo"))
        self.assertEqual(
            get_contents(registry)["browser/chrome.manifest"],
            ["manifest components/components.manifest"],
        )
        self.assertEqual(
            get_contents(registry["omni.foo"])["browser/chrome.manifest"],
            ["content browser chrome/browser/"],
        )

    def test_omnijar_browser_separate_by_default(self):
        registry = FileRegistry()
        formatter = OmniJarFormatter(registry, "omni.foo")
        fill_formatter(formatter, CONTENTS_WITH_BROWSER)
        self.assertTrue(registry.contains("omni.foo"))
        self.assertTrue(registry.contains("browser/omni.foo"))

    def test_omnijar_is_resource(self):
        def is_resource(base, path):
            registry = FileRegistry()
            f = OmniJarFormatter(
                registry,
                "omni.foo",
                non_resources=[
                    "defaults/messenger/mailViews.dat",
                    "defaults/foo/*",
                    "*/dummy",
                ],
            )
            f.add_base("")
            f.add_base("app")
            f.add(mozpath.join(base, path), GeneratedFile(b""))
            if f.copier.contains(mozpath.join(base, path)):
                return False
            self.assertTrue(f.copier.contains(mozpath.join(base, "omni.foo")))
            self.assertTrue(f.copier[mozpath.join(base, "omni.foo")].contains(path))
            return True

        for base in ["", "app/"]:
            self.assertTrue(is_resource(base, "chrome"))
            self.assertTrue(is_resource(base, "chrome/foo/bar/baz.properties"))
            self.assertFalse(is_resource(base, "chrome/icons/foo.png"))
            self.assertTrue(is_resource(base, "components/foo.js"))
            self.assertFalse(is_resource(base, "components/foo.so"))
            self.assertTrue(is_resource(base, "res/foo.css"))
            self.assertFalse(is_resource(base, "res/cursors/foo.png"))
            self.assertFalse(is_resource(base, "res/MainMenu.nib/foo"))
            self.assertTrue(is_resource(base, "defaults/pref/foo.js"))
            self.assertFalse(is_resource(base, "defaults/pref/channel-prefs.js"))
            self.assertTrue(is_resource(base, "defaults/preferences/foo.js"))
            self.assertFalse(is_resource(base, "defaults/preferences/channel-prefs.js"))
            self.assertTrue(is_resource(base, "modules/foo.sys.mjs"))
            self.assertTrue(is_resource(base, "greprefs.js"))
            self.assertTrue(is_resource(base, "hyphenation/foo"))
            self.assertTrue(is_resource(base, "default.locale"))
            self.assertFalse(is_resource(base, "foo"))
            self.assertFalse(is_resource(base, "foo/bar/greprefs.js"))
            self.assertTrue(is_resource(base, "defaults/messenger/foo.dat"))
            self.assertFalse(is_resource(base, "defaults/messenger/mailViews.dat"))
            self.assertTrue(is_resource(base, "defaults/pref/foo.js"))
            self.assertFalse(is_resource(base, "defaults/foo/bar.dat"))
            self.assertFalse(is_resource(base, "defaults/foo/bar/baz.dat"))
            self.assertTrue(is_resource(base, "chrome/foo/bar/baz/dummy_"))
            self.assertFalse(is_resource(base, "chrome/foo/bar/baz/dummy"))
            self.assertTrue(is_resource(base, "chrome/foo/bar/dummy_"))
            self.assertFalse(is_resource(base, "chrome/foo/bar/dummy"))

    def test_chrome_override(self):
        registry = FileRegistry()
        f = FlatFormatter(registry)
        f.add_base("")
        f.add_manifest(ManifestContent("chrome", "foo", "foo/unix"))
        # A more specific entry for a given chrome name can override a more
        # generic one.
        f.add_manifest(ManifestContent("chrome", "foo", "foo/win", "os=WINNT"))
        f.add_manifest(ManifestContent("chrome", "foo", "foo/osx", "os=Darwin"))

        # Chrome with the same name overrides the previous registration.
        with self.assertRaises(ErrorMessage) as e:
            f.add_manifest(ManifestContent("chrome", "foo", "foo/"))

        self.assertEqual(
            str(e.exception),
            'error: "content foo foo/" overrides "content foo foo/unix"',
        )

        # Chrome with the same name and same flags overrides the previous
        # registration.
        with self.assertRaises(ErrorMessage) as e:
            f.add_manifest(ManifestContent("chrome", "foo", "foo/", "os=WINNT"))

        self.assertEqual(
            str(e.exception),
            'error: "content foo foo/ os=WINNT" overrides '
            '"content foo foo/win os=WINNT"',
        )

        # We may start with the more specific entry first
        f.add_manifest(ManifestContent("chrome", "bar", "bar/win", "os=WINNT"))
        # Then adding a more generic one overrides it.
        with self.assertRaises(ErrorMessage) as e:
            f.add_manifest(ManifestContent("chrome", "bar", "bar/unix"))

        self.assertEqual(
            str(e.exception),
            'error: "content bar bar/unix" overrides "content bar bar/win os=WINNT"',
        )

        # Adding something more specific still works.
        f.add_manifest(
            ManifestContent("chrome", "bar", "bar/win", "os=WINNT osversion>=7.0")
        )

        # Variations of skin/locales are allowed.
        f.add_manifest(
            ManifestSkin("chrome", "foo", "classic/1.0", "foo/skin/classic/")
        )
        f.add_manifest(ManifestSkin("chrome", "foo", "modern/1.0", "foo/skin/modern/"))

        f.add_manifest(ManifestLocale("chrome", "foo", "en-US", "foo/locale/en-US/"))
        f.add_manifest(ManifestLocale("chrome", "foo", "ja-JP", "foo/locale/ja-JP/"))

        # But same-skin/locale still error out.
        with self.assertRaises(ErrorMessage) as e:
            f.add_manifest(
                ManifestSkin("chrome", "foo", "classic/1.0", "foo/skin/classic/foo")
            )

        self.assertEqual(
            str(e.exception),
            'error: "skin foo classic/1.0 foo/skin/classic/foo" overrides '
            '"skin foo classic/1.0 foo/skin/classic/"',
        )

        with self.assertRaises(ErrorMessage) as e:
            f.add_manifest(
                ManifestLocale("chrome", "foo", "en-US", "foo/locale/en-US/foo")
            )

        self.assertEqual(
            str(e.exception),
            'error: "locale foo en-US foo/locale/en-US/foo" overrides '
            '"locale foo en-US foo/locale/en-US/"',
        )

        # Duplicating existing manifest entries is not an error.
        f.add_manifest(ManifestContent("chrome", "foo", "foo/unix"))

        self.assertEqual(
            self.get_output(),
            [
                'warning: "content foo foo/unix" is duplicated. Skipping.',
            ],
        )


if __name__ == "__main__":
    mozunit.main()
