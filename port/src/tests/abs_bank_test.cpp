#include <gtest/gtest.h>

#include <cstring>

#include "abs_bank.hpp"
#include "platform/files.hpp"
#include "gamedata.hpp"
#include "maintex.hpp"
#include "platform/config.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"
#include "mainloop_port.hpp"
#include "memcard.hpp"
#include "save_extensions.hpp"
#include "savedata.hpp"
#include "save_slots.hpp"

namespace fs = std::filesystem;

namespace {

class AbsBankTest : public testing::Test {
protected:
    fs::path root;
    Config   prior;

    void SetUp() override {
        prior = ConfigGet();
        root = fs::temp_directory_path() / ("dc2_abs_bank_" + std::to_string(dc::test::ProcessId()));
        fs::remove_all(root);
        fs::create_directories(root / "saves");
        PathsSetSaveRoot(root);
        SaveExtensionsClear();
    }

    void TearDown() override {
        ConfigChange(prior);
        SaveExtensionsClear();
        PathsReset();
        std::error_code error;
        fs::remove_all(root, error);
    }

    void EnableBanking() {
        Config config = ConfigGet();
        config.abs_bank = true;
        ConfigChange(config);
    }
};

TEST_F(AbsBankTest, CreditDebitAndBalanceRoundTrip) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));
    EXPECT_FLOAT_EQ(AbsBankBalance(), 0.0f);
    AbsBankCredit(25.5f);
    EXPECT_FLOAT_EQ(AbsBankBalance(), 25.5f);
    EXPECT_TRUE(AbsBankDebit(10.0f));
    EXPECT_FLOAT_EQ(AbsBankBalance(), 15.5f);
    EXPECT_FALSE(AbsBankDebit(100.0f));
    EXPECT_FLOAT_EQ(AbsBankBalance(), 15.5f);
}

TEST_F(AbsBankTest, ApplyRespectsWeaponCapacityAndDebitsBank) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));
    AbsBankCredit(100.0f);

    CGameDataUsed weapon{};
    weapon.used_type = USED_ITEM_TYPE_WEAPON;
    weapon.data.weapon.abs.max = 200.0f;
    weapon.data.weapon.abs.now = 170.0f;

    AbsBankSetApplyAmount(25.0f);
    EXPECT_TRUE(AbsBankApplyToWeapon(&weapon));
    EXPECT_FLOAT_EQ(weapon.data.weapon.abs.now, 195.0f);
    EXPECT_FLOAT_EQ(AbsBankBalance(), 75.0f);
    EXPECT_FLOAT_EQ(AbsBankApplyAmount(), 0.0f);
}

TEST_F(AbsBankTest, RouteOrbCreditsWhenEnabled) {
    EnableBanking();
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));
    EXPECT_TRUE(AbsBankRouteOrbExperience(12.0f));
    EXPECT_FLOAT_EQ(AbsBankBalance(), 12.0f);
}

TEST_F(AbsBankTest, AddExpWeaponParamBanksInsteadOfBattleWhenEnabled) {
    EnableBanking();
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));
    AddExpWeaponParam(7.0f, 0, 0);
    EXPECT_FLOAT_EQ(AbsBankBalance(), 7.0f);
}

TEST_F(AbsBankTest, OrbCreditIgnoredUntilSaveSlotLoad) {
    EnableBanking();
    EXPECT_FALSE(SaveExtensionsActive());
    AddExpWeaponParam(9.0f, 0, 0);
    EXPECT_FLOAT_EQ(AbsBankBalance(), 0.0f);

    CSaveData                live{};
    CSaveData               *previous = PortActiveSaveData;
    PortActiveSaveData = &live;

    const fs::path path = SaveSlotsRoot() / "BESCES-51190dkcl0";
    SAVEDATA_FORMAT image{};
    std::strcpy(image.version, "dc2Ver4");
    image.unique_counter = 3;
    ASSERT_EQ(FilesWrite(path, &image, sizeof(image), false), FilesResult::kWritten);
    SaveSlots = SaveSlotScan(false);
    CMemoryCardManager manager;
    manager.InitForMC();
    ASSERT_EQ(manager.LoadFromMc(0), MC_STEP_DONE);
    EXPECT_TRUE(SaveExtensionsActive());

    AddExpWeaponParam(4.0f, 0, 0);
    EXPECT_FLOAT_EQ(AbsBankBalance(), 4.0f);
    manager.FinishForMC();
    PortActiveSaveData = previous;
    SaveSlots = {};
}

TEST_F(AbsBankTest, TransferToWeaponCapsByGaugeAndBank) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));
    AbsBankCredit(100.0f);

    CGameDataUsed weapon{};
    weapon.used_type = USED_ITEM_TYPE_WEAPON;
    weapon.data.weapon.abs.max = 200.0f;
    weapon.data.weapon.abs.now = 180.0f;

    EXPECT_FLOAT_EQ(AbsBankTransferToWeapon(&weapon, 50.0f), 20.0f);
    EXPECT_FLOAT_EQ(weapon.data.weapon.abs.now, 200.0f);
    EXPECT_FLOAT_EQ(AbsBankBalance(), 80.0f);
}

TEST_F(AbsBankTest, TransferFromWeaponRefundsWithoutLoweringLevel) {
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 1));

    CGameDataUsed weapon{};
    weapon.used_type = USED_ITEM_TYPE_WEAPON;
    weapon.data.weapon.level = 3;
    weapon.data.weapon.abs.max = 200.0f;
    weapon.data.weapon.abs.now = 40.0f;

    EXPECT_FLOAT_EQ(AbsBankTransferFromWeapon(&weapon, 25.0f), 25.0f);
    EXPECT_FLOAT_EQ(weapon.data.weapon.abs.now, 15.0f);
    EXPECT_EQ(weapon.data.weapon.level, 3);
    EXPECT_FLOAT_EQ(AbsBankBalance(), 25.0f);
    EXPECT_FLOAT_EQ(AbsBankTransferFromWeapon(&weapon, 100.0f), 15.0f);
    EXPECT_FLOAT_EQ(weapon.data.weapon.abs.now, 0.0f);
    EXPECT_EQ(weapon.data.weapon.level, 3);
}

TEST_F(AbsBankTest, SaveExtensionsRoundTripAfterSpend) {
    EnableBanking();
    const fs::path image = SaveSlotsRoot() / "BESCES-51190dkcl0";
    EXPECT_TRUE(SaveExtensionsLoad(image, 3));
    AbsBankCredit(80.0f);

    CGameDataUsed weapon{};
    weapon.used_type = USED_ITEM_TYPE_WEAPON;
    weapon.data.weapon.abs.max = 100.0f;
    weapon.data.weapon.abs.now = 0.0f;
    AbsBankSetApplyAmount(40.0f);
    EXPECT_TRUE(AbsBankApplyToWeapon(&weapon));
    EXPECT_TRUE(SaveExtensionsSave(image, 4));

    SaveExtensionsClear();
    EXPECT_TRUE(SaveExtensionsLoad(image, 4));
    EXPECT_FLOAT_EQ(AbsBankBalance(), 40.0f);
}

} // namespace
