using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

#include "Plugin.h"
#include "Config.h"

namespace SensibleTempering {
    std::optional<std::filesystem::path> getLogDirectory() {
        using namespace std::filesystem;
        PWSTR buf;
        SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &buf);
        std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> documentsPath{buf, CoTaskMemFree};
        path directory{documentsPath.get()};
        directory.append("My Games"sv);

        if (exists("steam_api64.dll"sv)) {
            if (exists("openvr_api.dll") || exists("Data/SkyrimVR.esm")) {
                directory.append("Skyrim VR"sv);
            } else {
                directory.append("Skyrim Special Edition"sv);
            }
        } else if (exists("Galaxy64.dll"sv)) {
            directory.append("Skyrim Special Edition GOG"sv);
        } else if (exists("eossdk-win64-shipping.dll"sv)) {
            directory.append("Skyrim Special Edition EPIC"sv);
        } else {
            return current_path().append("skselogs");
        }
        return directory.append("SKSE"sv).make_preferred();
    }

    void initializeLogging() {
        auto path = getLogDirectory();
        if (!path) {
            report_and_fail("Can't find SKSE log directory");
        }
        *path /= std::format("{}.log"sv, Plugin::Name);

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } else {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern(PLUGIN_LOGPATTERN_DEFAULT);
    }
}  // namespace plugin

using namespace SensibleTempering;

class TemperHooks {

    static float getArBonus(float temperTier, RE::TESObjectARMO* a) {
        float old = oldGetArBonus(temperTier, a);
        if (!a || Config::additiveFallback && a->GetArmorRating() * (temperTier - 1) < old)
            return old;
        return a->GetArmorRating() * (temperTier - 1) * Config::percentPerTierArmor * 0.1f;
    }

    static float getDmgBonus(float temperTier) { //original getDmgBonus only takes one argument, and doesn't receive the weapon ptr, so I had to take the long way...
        REL::Relocation<decltype(getDmgBonus)> oldGetDmgBonus(REL::ID(RELOCATION_ID(25915, 26498)));
        return oldGetDmgBonus(temperTier);
    }

    static float mysteryFunction(RE::ActorValueOwner *qwrd, uint32_t arg2) {
        REL::Relocation<decltype(mysteryFunction)> oldMysteryFunction(REL::ID(RELOCATION_ID(37517, 38462)));
        return oldMysteryFunction(qwrd, arg2);
    }

    static float getClampedActorValue(RE::ActorValueOwner* actorValueOwner, stl::enumeration<RE::ActorValue, std::uint32_t> q2) {
        REL::Relocation<decltype(getClampedActorValue)> oldGetClampedActorValue(REL::ID(RELOCATION_ID(26616, 27284)));
        return oldGetClampedActorValue(actorValueOwner, q2);
    }

    static float getDamage(RE::ActorValueOwner* actorValueOwner, RE::TESObjectWEAP* weapon, RE::TESAmmo *munition, float temperTier, float idkFloat, char idkChar) { // And substitute this long function
        float oldReturn = oldGetDamage(actorValueOwner, weapon, munition, temperTier, idkFloat, idkChar);
        if (!actorValueOwner || !weapon || weapon->GetWeaponType() == 0 || weapon->GetWeaponType() == 8)
            return oldReturn;
        RE::TESAmmo *ammoType = munition;
        float ammoDamage = 0;
        if (!munition && idkChar && actorValueOwner->GetIsPlayerOwner()) {
            ammoType = RE::PlayerCharacter::GetSingleton()->GetCurrentAmmo();
        }
        if (ammoType) {
            ammoDamage = ammoType->data.damage;
        }
        float damage = (ammoDamage + weapon->GetAttackDamage()) * RE::GameSettingCollection::GetSingleton()->GetSetting("fDamageWeaponMult")->GetFloat();
        stl::enumeration<RE::ActorValue, std::uint32_t> weaponSkill = weapon->weaponData.skill;
        float float3 = 0;

        float additiveDamageBonus = getDmgBonus(temperTier); // part that matters
        float temperDamageBonus = weapon->GetAttackDamage() * (temperTier - 1) * Config::percentPerTierWeapon * 0.1f;
        if (Config::additiveFallback && temperDamageBonus < additiveDamageBonus) {
            temperDamageBonus = additiveDamageBonus;
        }

        if (weapon->GetWeaponType() >= 1 and weapon->GetWeaponType() <= 6) { //if is mellee
            float3 = mysteryFunction(actorValueOwner, 34); // dont know, dont care, value is correct
        }
        float clampedActorValue;
        float damageSkill;
        if (weaponSkill.underlying() - 6 > 17) /*???*/ {
            clampedActorValue = 100;
            damageSkill = 1.0f;
        }
        else {
            clampedActorValue = getClampedActorValue(actorValueOwner, weaponSkill);
            if (actorValueOwner->GetIsPlayerOwner()) {
                damageSkill = (RE::GameSettingCollection::GetSingleton()->GetSetting("fDamagePCSkillMax")->GetFloat() - RE::GameSettingCollection::GetSingleton()->GetSetting("fDamagePCSkillMin")->GetFloat())
                    * clampedActorValue * 0.01f + RE::GameSettingCollection::GetSingleton()->GetSetting("fDamagePCSkillMin")->GetFloat();
            }
            else {
                damageSkill = (RE::GameSettingCollection::GetSingleton()->GetSetting("fDamageSkillMax")->GetFloat() - RE::GameSettingCollection::GetSingleton()->GetSetting("fDamageSkillMin")->GetFloat())
                    * clampedActorValue * 0.01f + RE::GameSettingCollection::GetSingleton()->GetSetting("fDamageSkillMin")->GetFloat();
            }
        }
        float time = 1.0f;
        if (weapon /*&& weapon->data->flags.underlying() != 2*/ && weapon->GetWeaponType() < 7 && RE::VATS::GetSingleton()->VATSMode != RE::VATS::VATS_MODE::kNone) { // dont know why all this, also commented condition is vestigial from fo3
            time = RE::GetSecondsSinceLastFrame();
        }
        float float2 = damageSkill * idkFloat;
        float returnValue = (((damage + temperDamageBonus) * float2) + float3) * time;
        float check = (damage * float2 + float3) * time;
        return temperDamageBonus != 0 && std::roundf(returnValue) == std::roundf(check) ? std::roundf(returnValue) + 0.55f : returnValue;
    }

    static inline REL::Relocation<decltype(getArBonus)> oldGetArBonus;
    static inline REL::Relocation<decltype(getDamage)> oldGetDamage;

public:
    static void hook() {
        oldGetArBonus = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(50455, 51360)).address() + 0x471, getArBonus);
        SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(50455, 51360)).address() + 0x45d, getArBonus);
        SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(15779, 16017)).address() + 0x2f, getArBonus);

        oldGetDamage = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(25846, 26409)).address() + 0x19, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(25848, 26411)).address() + 0x2a, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(39215, 40291)).address() + 0x37, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(42920, 44100)).address() + 0x2f8, getDamage);
    }
    static void hookSE() {
        oldGetArBonus = SKSE::GetTrampoline().write_call<5>(REL::ID(50531).address() + 0x60, getArBonus);
        SKSE::GetTrampoline().write_call<5>(REL::ID(50531).address() + 0x72, getArBonus);
        SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(15779, 16017)).address() + 0x2f, getArBonus);

        oldGetDamage = SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(25846, 26409)).address() + 0x19, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(RELOCATION_ID(25848, 26411)).address() + 0x2a, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(39215).address() + 0x2f, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(42920).address() + 0x2f4, getDamage);
    }
};

extern "C" DLLEXPORT bool SKSEPlugin_Load(const LoadInterface* skse) {
    initializeLogging();
    logger::info("'{} {}' is loading, game version '{}'...", Plugin::Name, Plugin::VersionString, REL::Module::get().version().string());
    Init(skse);
    SKSE::AllocTrampoline(1 << 10);
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            Config::LoadConfig();
            Config::LoadConfig();
            if (REL::Module::IsSE()) {
                TemperHooks::hookSE();
            }
            else if (REL::Module::IsAE()) {
                TemperHooks::hook();
            }
        }
        });

    logger::info("{} has finished loading.", Plugin::Name);
    return true;
}