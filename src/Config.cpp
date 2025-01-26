#include "Config.h"
#include "SimpleIni.h"

namespace SensibleTempering {
    void Config::LoadConfig() noexcept
    {
        logger::info("Loading settings");

        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadFile(R"(.\Data\SKSE\Plugins\SensibleTempering.ini)") != 0) {
            logger::error("Failed to Load config file.");
            return;
        }

        additiveFallback = ini.GetBoolValue("General", "regenCoefficient");
        percentPerTierArmor = static_cast<float>(ini.GetDoubleValue("General", "percentPerTierArmor"));
        percentPerTierWeapon = static_cast<float>(ini.GetDoubleValue("General", "percentPerTierWeapon"));

        logger::info("Loaded config");
        logger::info("");
    }
}