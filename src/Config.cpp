#include "Config.h"
#include "SimpleIni.h"

namespace SensibleTempering {
    void Config::LoadConfig() noexcept
    {
        logger::info("Loading config");

        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadFile(R"(.\Data\SKSE\Plugins\SensibleTempering.ini)") != 0) {
            logger::error("Failed to Load config file.");
            return;
        }

        vanillaPlusMode = ini.GetBoolValue("General", "vanillaPlusMode");

        heavyArmorBonus = ini.GetLongValue("VanillaPlusMode", "heavyArmorBonus");
        heavyArmorChestBonus = ini.GetLongValue("VanillaPlusMode", "heavyArmorChestBonus");
        heavyArmorBonus = ini.GetLongValue("VanillaPlusMode", "lightArmorBonus");
        heavyArmorChestBonus = ini.GetLongValue("VanillaPlusMode", "lightArmorChestBonus");
        oneHandedBonus = ini.GetLongValue("VanillaPlusMode", "oneHandedBonus");
        twoHandedBonus = ini.GetLongValue("VanillaPlusMode", "twoHandedBonus");
        bowBonus = ini.GetLongValue("VanillaPlusMode", "bowBonus");
        crossbowBonus = ini.GetLongValue("VanillaPlusMode", "crossbowBonus");

        vanillaPlusFallback = ini.GetBoolValue("PercentMode", "vanillaPlusFallback");
        percentPerTierArmor = (float) ini.GetDoubleValue("PercentMode", "percentPerTierArmor");
        percentPerTierWeapon = (float) ini.GetDoubleValue("PercentMode", "percentPerTierWeapon");

        logger::info("Loaded config");
        logger::info("");
    }
}