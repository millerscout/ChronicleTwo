#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "mainloop_port.hpp"
#include "memcard.hpp"
#include "platform/files.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"
#include "save_extensions_mod.hpp"
#include "save_slots.hpp"

namespace fs = std::filesystem;

namespace {

class SaveExtensionsTest : public testing::Test {
protected:
    fs::path root;

    void SetUp() override {
        root = fs::temp_directory_path() / ("dc2_save_extensions_" + std::to_string(dc::test::ProcessId()));
        fs::remove_all(root);
        fs::create_directories(root / "saves");
        PathsSetSaveRoot(root);
        SaveExtensionsClear();
    }

    void TearDown() override {
        SaveExtensionsClear();
        PathsReset();
        std::error_code error;
        fs::remove_all(root, error);
    }
};

TEST_F(SaveExtensionsTest, SidecarPathUsesBasename) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl2";
    EXPECT_EQ(SaveExtensionsSidecarPath(image), SaveSlotsRoot() / "BESCES-51190dkcl2.extensions.json");
    EXPECT_EQ(SaveExtensionsSidecarPath(SaveSlotsRoot() / "autosave"),
              SaveSlotsRoot() / "autosave.extensions.json");
}

TEST_F(SaveExtensionsTest, MissingSidecarLoadsEmpty) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_EQ(SaveExtensionsMod("example.mod"), nullptr);
    EXPECT_TRUE(SaveExtensionsLoad(image, 5));
    EXPECT_TRUE(SaveExtensionsMods().empty());
    EXPECT_NE(SaveExtensionsMod("example.mod"), nullptr);
}

TEST_F(SaveExtensionsTest, BadSchemaVersionLoadsEmpty) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    const fs::path sidecar = SaveExtensionsSidecarPath(image);
    const std::string body = R"({
  "schema_version": 2,
  "save_file": "BESCES-51190dkcl0",
  "unique_counter": 1,
  "mods": { "a.b": { "x": 1 } }
})";
    ASSERT_EQ(FilesWrite(sidecar, body.data(), body.size(), true), FilesResult::kWritten);
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));
    EXPECT_TRUE(SaveExtensionsMods().empty());
}

TEST_F(SaveExtensionsTest, ValidParseAndRoundTrip) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    const std::string body = R"({
  "schema_version": 1,
  "save_file": "BESCES-51190dkcl0",
  "unique_counter": 9,
  "mods": {
    "example.mod_a": { "value": 3 },
    "example.mod_b": { "flag": true }
  }
})";
    ASSERT_EQ(FilesWrite(SaveExtensionsSidecarPath(image), body.data(), body.size(), true), FilesResult::kWritten);
    EXPECT_TRUE(SaveExtensionsLoad(image, 9));
    EXPECT_EQ(SaveExtensionsMods()["example.mod_a"]["value"], 3);
    EXPECT_EQ(SaveExtensionsMods()["example.mod_b"]["flag"], true);

    nlohmann::json *mod = SaveExtensionsMod("example.mod_a");
    ASSERT_NE(mod, nullptr);
    (*mod)["value"] = 4;
    EXPECT_TRUE(SaveExtensionsSave(image, 10));

    SaveExtensionsClear();
    EXPECT_TRUE(SaveExtensionsLoad(image, 10));
    EXPECT_EQ(SaveExtensionsMods()["example.mod_a"]["value"], 4);
    EXPECT_EQ(SaveExtensionsMods()["example.mod_b"]["flag"], true);
}

TEST_F(SaveExtensionsTest, CounterMismatchStillLoads) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl1";
    const std::string body = R"({
  "schema_version": 1,
  "save_file": "BESCES-51190dkcl1",
  "unique_counter": 1,
  "mods": { "example.mod": { "banked": 12.5 } }
})";
    ASSERT_EQ(FilesWrite(SaveExtensionsSidecarPath(image), body.data(), body.size(), true), FilesResult::kWritten);
    EXPECT_TRUE(SaveExtensionsLoad(image, 99));
    EXPECT_EQ(SaveExtensionsMods()["example.mod"]["banked"], 12.5);
}

TEST_F(SaveExtensionsTest, NamespaceIsolation) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));
    (*SaveExtensionsMod("one.mod"))["a"] = 1;
    (*SaveExtensionsMod("two.mod"))["b"] = 2;
    EXPECT_EQ(SaveExtensionsMods()["one.mod"]["a"], 1);
    EXPECT_EQ(SaveExtensionsMods()["two.mod"]["b"], 2);
    EXPECT_FALSE(SaveExtensionsMods()["one.mod"].contains("b"));
}

TEST_F(SaveExtensionsTest, SkipWriteWhenCleanAndEmpty) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));
    EXPECT_TRUE(SaveExtensionsSave(image, 2));
    EXPECT_FALSE(fs::exists(SaveExtensionsSidecarPath(image)));
}

class SaveExtensionsMemcardTest : public testing::Test {
protected:
    fs::path                         root;
    std::unique_ptr<SAVEDATA_FORMAT> image = std::make_unique<SAVEDATA_FORMAT>();
    std::unique_ptr<SAVEDATA_FORMAT> buffer = std::make_unique<SAVEDATA_FORMAT>();
    std::unique_ptr<CSaveData>       live = std::make_unique<CSaveData>();
    CSaveData                       *previous_save = PortActiveSaveData;
    CMemoryCardManager               manager;

    void SetUp() override {
        root = fs::temp_directory_path() / ("dc2_save_ext_mc_" + std::to_string(dc::test::ProcessId()));
        fs::remove_all(root);
        fs::create_directories(root / "saves");
        PathsSetSaveRoot(root);
        std::memset(image.get(), 0, sizeof(*image));
        std::strcpy(image->version, "dc2Ver4");
        image->unique_counter = 42;
        PortActiveSaveData = live.get();
        manager.save_buffer = buffer.get();
        manager.InitForMC();
        SaveSlots = {};
        SaveExtensionsClear();
    }

    void TearDown() override {
        manager.FinishForMC();
        PortActiveSaveData = previous_save;
        SaveExtensionsClear();
        SaveSlots = {};
        PathsReset();
        std::error_code error;
        fs::remove_all(root, error);
    }
};

TEST_F(SaveExtensionsMemcardTest, LoadFromMcReadsSidecar) {
    const fs::path path = SaveSlotsRoot() / "BESCES-51190dkcl0";
    ASSERT_EQ(FilesWrite(path, image.get(), sizeof(*image), false), FilesResult::kWritten);
    const std::string sidecar = R"({
  "schema_version": 1,
  "save_file": "BESCES-51190dkcl0",
  "unique_counter": 42,
  "mods": { "integration.test": { "ok": true } }
})";
    ASSERT_EQ(FilesWrite(SaveExtensionsSidecarPath(path), sidecar.data(), sidecar.size(), true),
              FilesResult::kWritten);

    SaveSlots = SaveSlotScan(false);
    ASSERT_EQ(SaveSlots.saves.size(), 1u);
    EXPECT_EQ(manager.LoadFromMc(0), MC_STEP_DONE);
    EXPECT_EQ(SaveExtensionsMods()["integration.test"]["ok"], true);
}

TEST_F(SaveExtensionsMemcardTest, SaveToMcRoundTrip) {
    const fs::path path = SaveSlotsRoot() / "BESCES-51190dkcl0";
    ASSERT_EQ(FilesWrite(path, image.get(), sizeof(*image), false), FilesResult::kWritten);
    SaveSlots = SaveSlotScan(false);
    ASSERT_EQ(SaveSlots.saves.size(), 1u);
    ASSERT_EQ(manager.LoadFromMc(0), MC_STEP_DONE);
    (*SaveExtensionsMod("round.trip"))["count"] = 7;

    ASSERT_EQ(manager.SaveToMc(0), MC_STEP_DONE);
    SAVEDATA_FORMAT saved{};
    ASSERT_EQ(SaveSlotRead(path, saved), SaveSlotState::Ready);
    SaveExtensionsClear();
    EXPECT_TRUE(SaveExtensionsLoad(path, saved.unique_counter));
    EXPECT_EQ(SaveExtensionsMods()["round.trip"]["count"], 7);
}

} // namespace
