/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/Omnijar.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gtest/MozAssertions.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIInputStream.h"
#include "nsIZipWriter.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "prio.h"
#include "prtime.h"

using mozilla::Omnijar;

namespace {

class OmnijarTest : public testing::Test {
 protected:
  void SetUp() override {
    mWasInitialized = Omnijar::IsInitialized();
    if (mWasInitialized) {
      mSavedGRE = Omnijar::GetPath(Omnijar::GRE);
      mSavedAPP = Omnijar::GetPath(Omnijar::APP);
    }
    ASSERT_NS_SUCCEEDED(
        NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mDirectory)));
    ASSERT_NS_SUCCEEDED(mDirectory->AppendNative("TestOmnijar"_ns));
    ASSERT_NS_SUCCEEDED(
        mDirectory->CreateUnique(nsIFile::DIRECTORY_TYPE, 0700));
    mDirectoryCreated = true;
    ASSERT_NS_SUCCEEDED(mDirectory->Clone(getter_AddRefs(mGRE)));
    ASSERT_NS_SUCCEEDED(mGRE->AppendNative("omni.ja"_ns));
    ASSERT_NS_SUCCEEDED(mDirectory->Clone(getter_AddRefs(mAPP)));
    ASSERT_NS_SUCCEEDED(mAPP->AppendNative("app.ja"_ns));
    Omnijar::CleanUp();
  }

  void TearDown() override {
    Omnijar::CleanUp();
    if (mWasInitialized) {
      Omnijar::Init(mSavedGRE, mSavedAPP);
    }
    if (mDirectoryCreated) {
      EXPECT_NS_SUCCEEDED(mDirectory->Remove(true));
    }
  }

  void CreateArchive(nsIFile* aFile, bool aBrowserSubtree) {
    nsCOMPtr<nsIZipWriter> writer =
        do_CreateInstance("@mozilla.org/zipwriter;1");
    ASSERT_TRUE(writer);
    ASSERT_NS_SUCCEEDED(
        writer->Open(aFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
    AddEntry(writer, "chrome.manifest"_ns, "# Toolkit manifest\n"_ns);
    if (aBrowserSubtree) {
      AddEntry(writer, "browser/chrome.manifest"_ns, "# Browser manifest\n"_ns);
      AddEntry(writer, "browser/defaults/preferences/browser.js"_ns,
               "pref(\"test.omnijar.browser\", true);\n"_ns);
      AddEntry(writer, "browser/localization/en-US/browser/test.ftl"_ns,
               "test = Browser\n"_ns);
    }
    ASSERT_NS_SUCCEEDED(writer->Close());
  }

  void AddEntry(nsIZipWriter* aWriter, const nsACString& aPath,
                const nsACString& aData) {
    nsCOMPtr<nsIInputStream> stream;
    ASSERT_NS_SUCCEEDED(
        NS_NewCStringInputStream(getter_AddRefs(stream), aData));
    ASSERT_NS_SUCCEEDED(aWriter->AddEntryStream(
        aPath, PR_Now(), nsIZipWriter::COMPRESSION_NONE, stream, false));
  }

  void CheckBrowserSubtree() {
    EXPECT_TRUE(Omnijar::HasOmnijar(Omnijar::GRE));
    EXPECT_TRUE(Omnijar::HasOmnijar(Omnijar::APP));
    nsCOMPtr<nsIFile> appPath = Omnijar::GetPath(Omnijar::APP);
    EXPECT_FALSE(appPath);
    EXPECT_TRUE(Omnijar::GetEntryPrefix(Omnijar::GRE).IsEmpty());
    EXPECT_EQ(Omnijar::GetEntryPrefix(Omnijar::APP), "browser/"_ns);

    RefPtr<nsZipArchive> gre = Omnijar::GetReader(Omnijar::GRE);
    RefPtr<nsZipArchive> app = Omnijar::GetReader(Omnijar::APP);
    ASSERT_TRUE(gre);
    ASSERT_EQ(gre, app);
    EXPECT_TRUE(app->GetItem(Omnijar::GetEntryPrefix(Omnijar::APP) +
                             "chrome.manifest"_ns));
    EXPECT_TRUE(app->GetItem(Omnijar::GetEntryPrefix(Omnijar::APP) +
                             "localization/en-US/browser/test.ftl"_ns));

    nsAutoCString pattern(Omnijar::GetEntryPrefix(Omnijar::APP));
    pattern.AppendLiteral("defaults/preferences/*.js$");
    mozilla::UniquePtr<nsZipFind> find;
    ASSERT_NS_SUCCEEDED(app->FindInit(pattern.get(), getter_Transfers(find)));
    const char* entry;
    uint16_t length;
    ASSERT_NS_SUCCEEDED(find->FindNext(&entry, &length));
    EXPECT_EQ(Substring(entry, length),
              "browser/defaults/preferences/browser.js"_ns);

    nsAutoCString greURI, appURI, fileURI;
    ASSERT_NS_SUCCEEDED(NS_GetURLSpecFromActualFile(mGRE, fileURI));
    ASSERT_NS_SUCCEEDED(Omnijar::GetURIString(Omnijar::GRE, greURI));
    ASSERT_NS_SUCCEEDED(Omnijar::GetURIString(Omnijar::APP, appURI));
    EXPECT_TRUE(greURI.Equals("jar:"_ns + fileURI + "!/"_ns));
    EXPECT_TRUE(appURI.Equals(greURI + "browser/"_ns));
  }

  bool mWasInitialized = false;
  bool mDirectoryCreated = false;
  nsCOMPtr<nsIFile> mSavedGRE, mSavedAPP;
  nsCOMPtr<nsIFile> mDirectory, mGRE, mAPP;
};

TEST_F(OmnijarTest, BrowserSubtree) {
  CreateArchive(mGRE, true);
  // No application archive exists, and autodetection must not need one.
  ASSERT_NS_SUCCEEDED(Omnijar::FallibleInit(mGRE));
  CheckBrowserSubtree();

  // Child processes receive only the GRE archive, not an APP archive flag.
  Omnijar::CleanUp();
  nsAutoString filePath;
  ASSERT_NS_SUCCEEDED(mGRE->GetPath(filePath));
  NS_ConvertUTF16toUTF8 path(filePath);
  char program[] = "test";
  char flag[] = "-greomni";
  char* argv[] = {program, flag, path.BeginWriting(), nullptr};
  int argc = 3;
  Omnijar::ChildProcessInit(argc, argv);
  ASSERT_TRUE(Omnijar::IsInitialized());
  CheckBrowserSubtree();
}

TEST_F(OmnijarTest, ExistingApplicationLayouts) {
  CreateArchive(mGRE, false);
  CreateArchive(mAPP, false);
  ASSERT_NS_SUCCEEDED(Omnijar::FallibleInit(mGRE, mAPP));
  EXPECT_TRUE(Omnijar::HasOmnijar(Omnijar::APP));
  EXPECT_TRUE(Omnijar::GetEntryPrefix(Omnijar::APP).IsEmpty());
  RefPtr<nsZipArchive> gre = Omnijar::GetReader(Omnijar::GRE);
  RefPtr<nsZipArchive> app = Omnijar::GetReader(Omnijar::APP);
  EXPECT_NE(gre, app);

  Omnijar::CleanUp();
  ASSERT_NS_SUCCEEDED(Omnijar::FallibleInit(mGRE, mGRE));
  EXPECT_FALSE(Omnijar::HasOmnijar(Omnijar::APP));
  EXPECT_TRUE(Omnijar::GetEntryPrefix(Omnijar::APP).IsEmpty());
  nsAutoCString uri;
  ASSERT_NS_SUCCEEDED(Omnijar::GetURIString(Omnijar::APP, uri));
  EXPECT_TRUE(uri.IsEmpty());
}

}  // namespace
