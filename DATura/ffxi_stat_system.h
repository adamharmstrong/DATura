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

    struct AttributeBlock
    {
        std::array<int, kAttr_Count> value{};

        int& operator[](Attribute attr) { return value[(size_t)attr]; }
        int operator[](Attribute attr) const { return value[(size_t)attr]; }
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

    Rank RaceRank(Race race, Attribute attr);
    Rank JobRank(Job job, Attribute attr);

    int ClampLevel(int level);
    int CombatSkillContribution(int combatSkill);

    AttributeBlock BaseAttributes(Race race, Job mainJob, int mainLevel);
    AttributeBlock SupportJobAttributes(Job supportJob, int supportLevel);
    AttributeBlock TotalAttributes(const CharacterBuild& build);

    int Accuracy(const AccuracyInput& input);
    int RangedAccuracy(int combatSkill, int agility, int bonuses);
    int Evasion(int evasionSkill, int agility, int bonuses);
    int Defense(int defenseSkillOrBase, int vitality, int bonuses);
    int Attack(const AttackInput& input);

    float HitRateFromAccuracyDelta(int attackerAccuracy, int defenderEvasion);
    int FStrDelta(int attackerStrength, int defenderVitality);
    int CriticalDelta(int attackerDexterity, int defenderAgility);
}
