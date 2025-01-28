namespace SensibleTempering {
    class Config {
    public:
        static void LoadConfig() noexcept;

        inline static bool vanillaPlusMode{};

        inline static long heavyArmorBonus;
        inline static long heavyArmorChestBonus;
        inline static long lightArmorBonus;
        inline static long lightArmorChestBonus;
        inline static long oneHandedBonus;
        inline static long twoHandedBonus;
        inline static long bowBonus;
        inline static long crossbowBonus;

        inline static bool vanillaPlusFallback{};
        inline static float percentPerTierArmor{};
        inline static float percentPerTierWeapon{};
    };
}  // namespace betteralttab