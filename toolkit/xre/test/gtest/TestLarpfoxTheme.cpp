/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#include <string.h>

#include "gtest/gtest.h"
#include "mozilla/LarpfoxTheme.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIZipWriter.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "nsZipArchive.h"
#include "prio.h"
#include "prtime.h"

using mozilla::FindLarpfoxTheme;
using mozilla::Preferences;

class LarpfoxThemeTest : public ::testing::Test {
 protected:
  nsCOMPtr<nsIFile> mProfile;
  nsCOMPtr<nsIFile> mThemes;

  static nsCOMPtr<nsIFile> Child(nsIFile* aParent, const nsAString& aLeaf) {
    nsCOMPtr<nsIFile> file;
    EXPECT_EQ(NS_OK, aParent->Clone(getter_AddRefs(file)));
    EXPECT_EQ(NS_OK, file->Append(aLeaf));
    return file;
  }

  void SetUp() override {
    ASSERT_EQ(NS_OK,
              NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mProfile)));
    // URLPreloader may cache paths, so do not reuse a previous test's path.
    static unsigned counter = 0;
    nsAutoString leaf(u"larpfox-theme-test-"_ns);
    leaf.AppendInt(counter++);
    ASSERT_EQ(NS_OK, mProfile->Append(leaf));
    ASSERT_EQ(NS_OK, mProfile->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700));
    mThemes = Child(mProfile, u"lfx-themes"_ns);
    ASSERT_EQ(NS_OK, mThemes->Create(nsIFile::DIRECTORY_TYPE, 0700));
  }

  void TearDown() override {
    if (mProfile) {
      EXPECT_EQ(NS_OK, mProfile->Remove(true));
    }
  }

  static void Write(const nsCOMPtr<nsIFile>& aFile,
                    const nsACString& aContent) {
    PRFileDesc* fd = nullptr;
    ASSERT_EQ(NS_OK, aFile->OpenNSPRFileDesc(
                         PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0600, &fd));
    EXPECT_EQ(static_cast<int32_t>(aContent.Length()),
              PR_Write(fd, aContent.BeginReading(), aContent.Length()));
    EXPECT_EQ(PR_SUCCESS, PR_Close(fd));
  }

  void Prefs(const nsACString& aContent) {
    Write(Child(mProfile, u"prefs.js"_ns), aContent);
  }

  void User(const nsACString& aContent) {
    Write(Child(mProfile, u"user.js"_ns), aContent);
  }

  void Archive(const nsAString& aLeaf, const char* aOmit = nullptr) {
    nsCOMPtr<nsIZipWriter> writer =
        do_CreateInstance("@mozilla.org/zipwriter;1");
    ASSERT_TRUE(writer);
    nsCOMPtr<nsIFile> file = Child(mThemes, aLeaf);
    ASSERT_EQ(NS_OK,
              writer->Open(file, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
    for (const char* entry :
         {"chrome.manifest", "browser/chrome.manifest", "greprefs.js"}) {
      if (aOmit && !strcmp(entry, aOmit)) {
        continue;
      }
      nsCOMPtr<nsIInputStream> stream;
      ASSERT_EQ(NS_OK, NS_NewCStringInputStream(getter_AddRefs(stream),
                                                "// theme resource\n"_ns));
      ASSERT_EQ(NS_OK, writer->AddEntryStream(
                           nsDependentCString(entry), PR_Now(),
                           nsIZipWriter::COMPRESSION_NONE, stream, false));
    }
    ASSERT_EQ(NS_OK, writer->Close());
  }

  void ExpectTheme(const nsAString& aLeaf) {
    bool hadService = Preferences::IsServiceAvailable();
    nsCOMPtr<nsIFile> selected = FindLarpfoxTheme(mProfile, false);
    EXPECT_EQ(hadService, Preferences::IsServiceAvailable());
    ASSERT_TRUE(selected);
    bool equal = false;
    nsCOMPtr<nsIFile> expected = Child(mThemes, aLeaf);
    ASSERT_EQ(NS_OK, selected->Equals(expected, &equal));
    EXPECT_TRUE(equal);
  }
};

TEST_F(LarpfoxThemeTest, SelectsValidArchiveWithoutChangingPreferences) {
  Archive(u"night.ja"_ns);
  bool hadService = Preferences::IsServiceAvailable();
  auto type = hadService ? Preferences::GetType("larpfox.theme-selector-test")
                         : nsIPrefBranch::PREF_INVALID;
  Prefs(
      "user_pref('larpfox.current-theme', 'night');\n"
      "user_pref('larpfox.theme-selector-test', true);\n"_ns);
  ExpectTheme(u"night.ja"_ns);
  EXPECT_EQ(hadService, Preferences::IsServiceAvailable());
  if (hadService) {
    EXPECT_EQ(type, Preferences::GetType("larpfox.theme-selector-test"));
  }
}

TEST_F(LarpfoxThemeTest, UserJsAndLastAssignmentWin) {
  Archive(u"night.ja"_ns);
  Prefs("user_pref('larpfox.current-theme', 'missing');"_ns);
  User(
      "user_pref('larpfox.current-theme', 'also-missing');\n"
      "user_pref('larpfox.current-theme', 'night');"_ns);
  ExpectTheme(u"night.ja"_ns);
}

TEST_F(LarpfoxThemeTest, UsesRealParserEscapesCommentsAndRecovery) {
  Archive(u"night.ja"_ns);
  Prefs(
      "/* user_pref('larpfox.current-theme', 'fake'); */\n"
      "not_a_pref();\n"
      "user_pref('larpfox.current-theme', 'ni\\u0067ht');\n"
      "// user_pref('larpfox.current-theme', 'fake');\n"_ns);
  ExpectTheme(u"night.ja"_ns);
}

TEST_F(LarpfoxThemeTest, WrongTypesAndDefaultPrefCallsDoNotOverride) {
  Archive(u"night.ja"_ns);
  Prefs("user_pref('larpfox.current-theme', 'night');"_ns);
  User(
      "user_pref('larpfox.current-theme', 1);\n"
      "user_pref('larpfox.current-theme', false);\n"
      "pref('larpfox.current-theme', 'missing');"_ns);
  ExpectTheme(u"night.ja"_ns);
}

TEST_F(LarpfoxThemeTest, EmptyUserSelectionReturnsToDefault) {
  Archive(u"night.ja"_ns);
  Prefs("user_pref('larpfox.current-theme', 'night');"_ns);
  User("user_pref('larpfox.current-theme', '');"_ns);
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(mProfile, false)));
}

TEST_F(LarpfoxThemeTest, SafeModeBypassesSelection) {
  Archive(u"night.ja"_ns);
  Prefs("user_pref('larpfox.current-theme', 'night');"_ns);
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(mProfile, true)));
  // A null profile is safe because the safe-mode check precedes profile access.
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(nullptr, true)));
}

TEST_F(LarpfoxThemeTest, MissingPreferencesUseDefault) {
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(mProfile, false)));
}

TEST_F(LarpfoxThemeTest, MissingArchiveUsesDefault) {
  Prefs("user_pref('larpfox.current-theme', 'missing');"_ns);
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(mProfile, false)));
}

TEST_F(LarpfoxThemeTest, CorruptArchiveUsesDefault) {
  Write(Child(mThemes, u"broken.ja"_ns), "not a zip archive"_ns);
  Prefs("user_pref('larpfox.current-theme', 'broken');"_ns);
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(mProfile, false)));
}

TEST_F(LarpfoxThemeTest, CorruptRequiredEntryUsesDefault) {
  Archive(u"broken.ja"_ns);
  nsCOMPtr<nsIFile> file = Child(mThemes, u"broken.ja"_ns);
  RefPtr<nsZipArchive> zip = nsZipArchive::OpenArchive(file);
  ASSERT_TRUE(zip);
  auto* item = zip->GetItem("greprefs.js"_ns);
  ASSERT_TRUE(item);
  uint32_t offset = zip->GetDataOffset(item);
  ASSERT_NE(0u, offset);
  zip = nullptr;
  PRFileDesc* fd = nullptr;
  ASSERT_EQ(NS_OK, file->OpenNSPRFileDesc(PR_RDWR, 0600, &fd));
  EXPECT_EQ(static_cast<int32_t>(offset), PR_Seek(fd, offset, PR_SEEK_SET));
  EXPECT_EQ(1, PR_Write(fd, "!", 1));
  EXPECT_EQ(PR_SUCCESS, PR_Close(fd));
  Prefs("user_pref('larpfox.current-theme', 'broken');"_ns);
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(mProfile, false)));
}

class LarpfoxIncompleteThemeTest
    : public LarpfoxThemeTest,
      public ::testing::WithParamInterface<const char*> {};

TEST_P(LarpfoxIncompleteThemeTest, RequiredEntryMissingUsesDefault) {
  Archive(u"incomplete.ja"_ns, GetParam());
  Prefs("user_pref('larpfox.current-theme', 'incomplete');"_ns);
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(mProfile, false)));
}

INSTANTIATE_TEST_SUITE_P(RequiredEntries, LarpfoxIncompleteThemeTest,
                         ::testing::Values("chrome.manifest",
                                           "browser/chrome.manifest",
                                           "greprefs.js"));

class LarpfoxThemePathTest : public LarpfoxThemeTest,
                             public ::testing::WithParamInterface<const char*> {
};

TEST_P(LarpfoxThemePathTest, RejectsNonBasenames) {
  // A traversal target really exists, so accepting ../outside would select it.
  Archive(u"outside.ja"_ns);
  ASSERT_EQ(
      NS_OK,
      Child(mThemes, u"outside.ja"_ns)->MoveTo(mProfile, u"outside.ja"_ns));
  nsAutoCString prefs("user_pref('larpfox.current-theme', '");
  prefs.Append(GetParam());
  prefs.AppendLiteral("');");
  Prefs(prefs);
  EXPECT_FALSE(nsCOMPtr<nsIFile>(FindLarpfoxTheme(mProfile, false)));
}

INSTANTIATE_TEST_SUITE_P(PathBoundaries, LarpfoxThemePathTest,
                         ::testing::Values("../outside", "..\\\\outside",
                                           "/outside", "C:/outside",
                                           "outside:stream", "nested/outside",
                                           ".", ".."));
