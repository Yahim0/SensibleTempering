namespace SensibleTempering {
    class Config {
    public:
        static void LoadConfig() noexcept;

        inline static bool additiveFallback{};

        inline static float percentPerTierArmor{};

        inline static float percentPerTierWeapon{};
    };
}  // namespace betteralttab