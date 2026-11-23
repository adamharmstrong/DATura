#pragma once

#include <array>
#include <cstddef>

namespace FFXIStats
{
    enum Attribute
    {
        kAttr_STR = 0,
        kAttr_DEX,
        kAttr_VIT,
        kAttr_AGI,
        kAttr_INT,
        kAttr_MND,
        kAttr_CHR,
        kAttr_Count
    };

    enum Race
    {
        kRace_Hume = 0,
        kRace_Elvaan,
        kRace_Tarutaru,
        kRace_Mithra,
        kRace_Galka,
        kRace_Count
    };

    enum Job
    {
        kJob_None = 0,
        kJob_WAR,
        kJob_MNK,
        kJob_WHM,
        kJob_BLM,
        kJob_RDM,
        kJob_THF,
        kJob_PLD,
        kJob_DRK,
        kJob_BST,
        kJob_BRD,
        kJob_RNG,
        kJob_SAM,
        kJob_NIN,
        kJob_DRG,
        kJob_SMN,
        kJob_BLU,
        kJob_COR,
        kJob_PUP,
        kJob_DNC,
        kJob_SCH,
        kJob_GEO,
        kJob_RUN,
        kJob_Count
    };

    enum Rank
    {
        kRank_A = 0,
        kRank_B,
        kRank_C,
        kRank_D,
        kRank_E,
        kRank_F,
        kRank_G
    };

    enum WeaponAttackProfile
    {
        kAttack_MainHand = 0,
        kAttack_OffHand,
        kAttack_HandToHand,
        kAttack_Ranged
    };

    enum Element { kElement_Fire, kElement_Ice, kElement_Wind, kElement_Earth,
        kElement_Lightning, kElement_Water, kElement_Light, kElement_Dark, kElement_Count };

    enum Skill
    {
        kSkill_HandToHand, kSkill_Dagger, kSkill_Sword, kSkill_GreatSword,
        kSkill_Axe, kSkill_GreatAxe, kSkill_Scythe, kSkill_Polearm, kSkill_Katana,
        kSkill_GreatKatana, kSkill_Club, kSkill_Staff, kSkill_Archery,
        kSkill_Marksmanship, kSkill_Throwing, kSkill_Guard, kSkill_Evasion,
        kSkill_Shield, kSkill_Parrying, kSkill_DivineMagic, kSkill_HealingMagic,
        kSkill_EnhancingMagic, kSkill_EnfeeblingMagic, kSkill_ElementalMagic,
        kSkill_DarkMagic, kSkill_SummoningMagic, kSkill_Ninjutsu, kSkill_Singing,
        kSkill_StringInstrument, kSkill_WindInstrument, kSkill_BlueMagic,
        kSkill_Geomancy, kSkill_Handbell, kSkill_Count
    };

    enum Modifier
    {
        kMod_MaxHP, kMod_MaxMP, kMod_Attack, kMod_RangedAttack, kMod_Defense,
        kMod_Accuracy, kMod_RangedAccuracy, kMod_Evasion, kMod_MagicAttack,
        kMod_MagicDefense, kMod_MagicAccuracy, kMod_MagicEvasion,
        kMod_Haste, kMod_DualWield, kMod_MartialArts, kMod_FastCast,
        kMod_StoreTP, kMod_SubtleBlow, kMod_Regain, kMod_Regen, kMod_Refresh,
        kMod_DoubleAttack, kMod_TripleAttack, kMod_QuadrupleAttack,
        kMod_CriticalHitRate, kMod_CriticalHitDamage, kMod_DamageTaken,
        kMod_PhysicalDamageTaken, kMod_MagicDamageTaken, kMod_BreathDamageTaken,
        kMod_Enmity, kMod_CurePotency, kMod_SpellInterruptionRate,
        kMod_MovementSpeed, kMod_Count
    };

    struct AttributeBlock
    {
        std::array<int, kAttr_Count> value{};

        int& operator[](Attribute attr) { return value[(size_t)attr]; }
        int operator[](Attribute attr) const { return value[(size_t)attr]; }
    };

    template <typename Index, std::size_t Count>
    struct StatBlock
    {
        std::array<int, Count> value{};
        int& operator[](Index index) { return value[static_cast<std::size_t>(index)]; }
        int operator[](Index index) const { return value[static_cast<std::size_t>(index)]; }
    };

    using ElementBlock = StatBlock<Element, kElement_Count>;
    using SkillBlock = StatBlock<Skill, kSkill_Count>;
    using ModifierBlock = StatBlock<Modifier, kMod_Count>;

    struct ResourceState { int currentHp = 1; int currentMp = 0; int tp = 0; };
    struct Progression
    {
        int experience = 0;
        int limitPoints = 0;
        int meritPoints = 0;
        int jobPoints = 0;
        int capacityPoints = 0;
        int masterLevel = 0;
    };

    struct CharacterBuild
    {
        Race race = kRace_Hume;
        Job mainJob = kJob_WAR;
        int mainLevel = 1;
        Job supportJob = kJob_None;
        int supportLevel = 0;

        AttributeBlock merits;
        AttributeBlock gear;
        AttributeBlock buffs;
        ResourceState resources;
        Progression progression;
        SkillBlock skills;
        ElementBlock elementalResistance;
        ModifierBlock gearModifiers;
        ModifierBlock traitModifiers;
        ModifierBlock effectModifiers;
    };

    struct DerivedStats
    {
        AttributeBlock attributes;
        ElementBlock elementalResistance;
        int maxHp = 1, maxMp = 0;
        int attack = 0, rangedAttack = 0, defense = 0;
        int accuracy = 0, rangedAccuracy = 0, evasion = 0;
        int magicAttack = 0, magicDefense = 0, magicAccuracy = 0, magicEvasion = 0;
    };

    struct AccuracyInput
    {
        int combatSkill = 0;
        int dexterity = 0;
        int traitBonus = 0;
        int equipmentBonus = 0;
        int effectBonus = 0;
        int foodBonus = 0;
    };

    struct AttackInput
    {
        int combatSkill = 0;
        int strength = 0;
        int equipmentBonus = 0;
        int traitAbilityBonus = 0;
        float abilityMultiplier = 1.0f;
        float foodMultiplier = 1.0f;
        WeaponAttackProfile profile = kAttack_MainHand;
    };

    const char* AttributeName(Attribute attr);
    const char* RaceName(Race race);
    const char* JobName(Job job);
    const char* ElementName(Element element);
    const char* SkillName(Skill skill);
    const char* ModifierName(Modifier modifier);

    Rank RaceRank(Race race, Attribute attr);
    Rank JobRank(Job job, Attribute attr);

    int ClampLevel(int level);
    int CombatSkillContribution(int combatSkill);

    AttributeBlock BaseAttributes(Race race, Job mainJob, int mainLevel);
    AttributeBlock SupportJobAttributes(Job supportJob, int supportLevel);
    AttributeBlock TotalAttributes(const CharacterBuild& build);
    ModifierBlock TotalModifiers(const CharacterBuild& build);
    int BaseMaxHP(Race race, Job mainJob, int mainLevel, Job supportJob, int supportLevel);
    int BaseMaxMP(Race race, Job mainJob, int mainLevel, Job supportJob, int supportLevel);
    DerivedStats Derive(const CharacterBuild& build, Skill attackSkill = kSkill_HandToHand,
        Skill rangedSkill = kSkill_Archery, Skill magicSkill = kSkill_ElementalMagic);
    void ClampResources(CharacterBuild& build, const DerivedStats& derived);

    int Accuracy(const AccuracyInput& input);
    int RangedAccuracy(int combatSkill, int agility, int bonuses);
    int Evasion(int evasionSkill, int agility, int bonuses);
    int Defense(int defenseSkillOrBase, int vitality, int bonuses);
    int Attack(const AttackInput& input);

    float HitRateFromAccuracyDelta(int attackerAccuracy, int defenderEvasion);
    int FStrDelta(int attackerStrength, int defenderVitality);
    int CriticalDelta(int attackerDexterity, int defenderAgility);
}
