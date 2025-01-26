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

    struct MySmithingItemEntry { // The struct in CommonLib is wrong, this is a replacement until they accept my PR
        // members
        RE::InventoryEntryData* item;                     // 00
        RE::BGSConstructibleObject* unk08;                // 08
        RE::BGSConstructibleObject* constructibleObject;  // 10
        float unk18;                                      // 18 - Health of item?
        float unk1C;                                      // 1C - kSmithing actor value?
        float unk20;                                      // 20
        float unk24;                                      // 24
        uint8_t unk28;                                    // 28
        uint8_t unk29;                                    // 29
        uint8_t unk2A;                                    // 2A
        uint8_t pad2B;                                    // 2B
        uint32_t pad2C;                                   // 2C
    };

    static float getArBonus(float temperTier, RE::TESObjectARMO* a) {
        float old = oldGetArBonus(temperTier, a);
        if (!a || Config::additiveFallback && a->GetArmorRating() * (temperTier - 1) < old)
            return old;
        return a->GetArmorRating() * (temperTier - 1) * Config::percentPerTierArmor * 0.1f;
    }

    static float getDmgBonus(float temperTier) { //original getDmgBonus only takes one argument, and doesn't receive the weapon ptr, so I had to take the long way...
        REL::Relocation<decltype(getDmgBonus)> oldGetDmgBonus(REL::ID(26498));
        return oldGetDmgBonus(temperTier);
    }

    static float getTemperTier(RE::InventoryEntryData *arg) { // and
        REL::Relocation<decltype(getTemperTier)> oldTemperTier(REL::ID(15990));
        return oldTemperTier(arg);
    }

    static float getPotentialTier(float arg) { // hook
        REL::Relocation<decltype(getPotentialTier)> oldPotentialtemper(REL::ID(26497));
        return oldPotentialtemper(arg);
    }

    static void functionA(uintptr_t array, uintptr_t innerFuncA, uint32_t i, uint32_t i2) { // all
        REL::Relocation<decltype(functionA)> oldFunctionA(REL::ID(51215).address());
        return oldFunctionA(array, innerFuncA, i, i2);
    }

    static int innerFunctionA(uintptr_t q1, uintptr_t q2) { // of
        REL::Relocation<decltype(innerFunctionA)> oldInnerFunctionA(REL::ID(51444).address());
        return oldInnerFunctionA(q1, q2);
    }

    static float functionB(RE::CraftingSubMenus::SmithingMenu* menu, RE::BSTArray<MySmithingItemEntry> *array) { //these
        REL::Relocation<decltype(functionB)> oldFunctionB(REL::ID(51225).address());
        return oldFunctionB(menu, array);
    }

    static float functionC(RE::CraftingSubMenus::SmithingMenu* menu, RE::BSTArray<MySmithingItemEntry>* array, int i) {
        REL::Relocation<decltype(functionC)> oldFunctionC(REL::ID(51233).address());
        return oldFunctionC(menu, array, i);
    }

    static uint64_t innerFunction(RE::CraftingSubMenus::SmithingMenu *arg) {
        REL::Relocation<decltype(innerFunction)> oldInnerFunction(REL::ID(51460).address());
        return oldInnerFunction(arg);
    }

    static float mysteryFunction(RE::ActorValueOwner *qwrd, uint32_t arg2) {
        REL::Relocation<decltype(mysteryFunction)> oldMysteryFunction(REL::ID(38462).address());
        return oldMysteryFunction(qwrd, arg2);
    }

    static float getClampedActorValue(RE::ActorValueOwner* actorValueOwner, stl::enumeration<RE::ActorValue, std::uint32_t> q2) {
        REL::Relocation<decltype(getClampedActorValue)> oldGetClampedActorValue(REL::ID(27284).address());
        return oldGetClampedActorValue(actorValueOwner, q2);
    }

    static uint64_t temperUIFunction(RE::CraftingSubMenus::SmithingMenu *menu) {
        uint64_t old = oldTemperUIFunction(menu);
        if (menu->smithingType == RE::FormType::Weapon) {
            auto itemArray = reinterpret_cast<RE::BSTArray<MySmithingItemEntry>*>(&menu->unk100);
            for (MySmithingItemEntry &i : *itemArray) {
                //logger::info("unk18 {}; unk1C {}; unk20 {}; unk24 {}; unk28 {}; unk29 {}; unk2A {}; pad2B {}; pad2C {};", i.unk18, i.unk1C, i.unk20, i.unk24, i.unk28, i.unk29, i.unk2A, i.pad2B, i.pad2C);
                float addDmg = getDmgBonus(getTemperTier(i.item));
                float multDmg = (getTemperTier(i.item) - 1.0f) * i.item->object->As<RE::TESObjectWEAP>()->GetAttackDamage() * Config::percentPerTierWeapon * 0.1f;
                float addPotentialDmg = getDmgBonus(getPotentialTier(RE::PlayerCharacter::GetSingleton()->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSmithing)));
                float multPotentialDmg = std::roundf(getPotentialTier(RE::PlayerCharacter::GetSingleton()->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSmithing))* 10) / 10 * i.item->object->As<RE::TESObjectWEAP>()->GetAttackDamage() * Config::percentPerTierWeapon * 0.1f;
                //logger::info("damage {}; potential {}; coeffiecint {}; result {}", i.item->object->As<RE::TESObjectWEAP>()->GetAttackDamage(),
                //    getPotentialTier((RE::PlayerCharacter::GetSingleton()->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSmithing))),
                //    (std::roundf((getPotentialTier(RE::PlayerCharacter::GetSingleton()->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSmithing)) - 1.0f) * 10.0f) / 10),
                //    newPotentialDmg);
                i.unk20 = Config::additiveFallback && addDmg > multDmg ? addDmg : multDmg;
                i.unk24 = Config::additiveFallback && addPotentialDmg > multPotentialDmg ? addPotentialDmg : multPotentialDmg;
                //logger::info("unk18 {}; unk1C {}; unk20 {};  unk24 {}; unk28 {}; unk29 {}; unk2A {}; pad2B {}; pad2C {};", i.unk18, i.unk1C, i.unk20, i.unk24, i.unk28, i.unk29, i.unk2A, i.pad2B, i.pad2C);
            }
            if (itemArray->size() > 1) {
                functionA(reinterpret_cast<std::uintptr_t>(itemArray), reinterpret_cast<std::uintptr_t>(&innerFunctionA), 0, itemArray->size() - 1);
            }
            functionB(menu, itemArray);
            functionC(menu, itemArray, 1);
            return innerFunction(menu);
        }
        return old;
    }

    static float getDamage(RE::ActorValueOwner* actorValueOwner, RE::TESObjectWEAP* weapon, RE::TESAmmo *munition, float temperTier, float idkFloat, char idkChar) {
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
        if (weapon /*&& weapon->data->flags.underlying() != 2*/ && weapon->GetWeaponType() < 7 && RE::VATS::GetSingleton()->VATSMode != RE::VATS::VATS_MODE::kNone) { // dont know why all this, also comments cond is vestigial from fo3
            time = RE::GetSecondsSinceLastFrame();
        }
        float float2 = damageSkill * idkFloat;
        float returnValue = (((damage + temperDamageBonus) * float2) + float3) * time;
        float check = (damage * float2 + float3) * time;
        return temperDamageBonus != 0 && std::roundf(returnValue) == std::roundf(check) ? std::roundf(returnValue) + 0.55f : returnValue;
    }

    static inline REL::Relocation<decltype(getArBonus)> oldGetArBonus;
    static inline REL::Relocation<decltype(temperUIFunction)> oldTemperUIFunction;
    static inline REL::Relocation<decltype(getDamage)> oldGetDamage;

public:
    static void hook() {
        oldGetArBonus = SKSE::GetTrampoline().write_call<5>(REL::ID(51360).address() + 0x471, getArBonus);
        SKSE::GetTrampoline().write_call<5>(REL::ID(51360).address() + 0x45d, getArBonus);
        SKSE::GetTrampoline().write_call<5>(REL::ID(16017).address() + 0x2f, getArBonus);

        oldTemperUIFunction = SKSE::GetTrampoline().write_call<5>(REL::ID(51370).address() + 0x1f9, temperUIFunction);
        SKSE::GetTrampoline().write_call<5>(REL::ID(51386).address() + 0x196, temperUIFunction);

        oldGetDamage = SKSE::GetTrampoline().write_call<5>(REL::ID(26409).address() + 0x19, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(26411).address() + 0x2a, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(40291).address() + 0x37, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(44100).address() + 0x2f8, getDamage);
    }
    static void hookSE() { //FIXME correct addresses
        oldGetArBonus = SKSE::GetTrampoline().write_call<5>(REL::ID(50455).address() + 0x471, getArBonus);
        SKSE::GetTrampoline().write_call<5>(REL::ID(50455).address() + 0x45d, getArBonus);
        SKSE::GetTrampoline().write_call<5>(REL::ID(15779).address() + 0x2f, getArBonus);

        oldTemperUIFunction = SKSE::GetTrampoline().write_call<5>(REL::ID(51370).address() + 0x1f9, temperUIFunction);
        SKSE::GetTrampoline().write_call<5>(REL::ID(50494).address() + 0x196, temperUIFunction);

        oldGetDamage = SKSE::GetTrampoline().write_call<5>(REL::ID(25846).address() + 0x19, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(25848).address() + 0x2a, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(39215).address() + 0x37, getDamage);
        SKSE::GetTrampoline().write_call<5>(REL::ID(42920).address() + 0x2f8, getDamage);
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
            if (REL::Module::IsSE()) {
                TemperHooks::hookSE();
            }
            else if (REL::Module::IsAE()){
                TemperHooks::hook();
            }
        }
        });

    logger::info("{} has finished loading.", Plugin::Name);
    return true;
}