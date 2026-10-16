#include "stdafx.h"
#include "ffxi_stat_system.h"

#include <algorithm>
#include <cmath>

namespace FFXIStats
{
    namespace
    {
        struct RankAnchors
        {
            int level1;
            int level75;
            int level99;
        };

        const char* kAttributeNames[kAttr_Count] = { "STR", "DEX", "VIT", "AGI", "INT", "MND", "CHR" };
        const char* kRaceNames[kRace_Count] = { "Hume", "Elvaan", "Tarutaru", "Mithra", "Galka" };
        const char* kJobNames[kJob_Count] =
        {
            "None", "WAR", "MNK", "WHM", "BLM", "RDM", "THF", "PLD", "DRK", "BST", "BRD", "RNG",
            "SAM", "NIN", "DRG", "SMN", "BLU", "COR", "PUP", "DNC", "SCH", "GEO", "RUN"
        };
        const char* kElementNames[kElement_Count] =
            {"Fire", "Ice", "Wind", "Earth", "Lightning", "Water", "Light", "Dark"};
        const char* kSkillNames[kSkill_Count] =
        {
            "HandToHand", "Dagger", "Sword", "GreatSword", "Axe", "GreatAxe", "Scythe",
            "Polearm", "Katana", "GreatKatana", "Club", "Staff", "Archery", "Marksmanship",
            "Throwing", "Guard", "Evasion", "Shield", "Parrying", "DivineMagic",
            "HealingMagic", "EnhancingMagic", "EnfeeblingMagic", "ElementalMagic", "DarkMagic",
            "SummoningMagic", "Ninjutsu", "Singing", "StringInstrument", "WindInstrument",
            "BlueMagic", "Geomancy", "Handbell"
        };
        const char* kModifierNames[kMod_Count] =
        {
            "MaxHP", "MaxMP", "Attack", "RangedAttack", "Defense", "Accuracy",
            "RangedAccuracy", "Evasion", "MagicAttack", "MagicDefense", "MagicAccuracy",
            "MagicEvasion", "Haste", "DualWield", "MartialArts", "FastCast", "StoreTP",
            "SubtleBlow", "Regain", "Regen", "Refresh", "DoubleAttack", "TripleAttack",
            "QuadrupleAttack", "CriticalHitRate", "CriticalHitDamage", "DamageTaken",
            "PhysicalDamageTaken", "MagicDamageTaken", "BreathDamageTaken", "Enmity",
            "CurePotency", "SpellInterruptionRate", "MovementSpeed"
        };

        const RankAnchors kRaceCurve[7] =
        {
            { 5, 15, 20 }, { 4, 13, 18 }, { 4, 11, 16 }, { 4, 9, 14 },
            { 3, 7, 12 }, { 3, 5, 10 }, { 2, 3, 8 }
        };

        const RankAnchors kJobCurve[7] =
        {
            { 4, 55, 67 }, { 4, 52, 64 }, { 4, 49, 61 }, { 3, 46, 58 },
            { 3, 43, 55 }, { 2, 40, 52 }, { 2, 37, 49 }
        };

        const Rank kRaceRanks[kRace_Count][kAttr_Count] =
        {
            //            STR      DEX      VIT      AGI      INT      MND      CHR
            /* Hume    */ { kRank_C, kRank_C, kRank_D, kRank_D, kRank_C, kRank_C, kRank_C },
            /* Elvaan  */ { kRank_A, kRank_D, kRank_C, kRank_F, kRank_F, kRank_A, kRank_C },
            /* Taru    */ { kRank_E, kRank_C, kRank_E, kRank_C, kRank_A, kRank_E, kRank_C },
            /* Mithra  */ { kRank_D, kRank_A, kRank_E, kRank_B, kRank_D, kRank_E, kRank_F },
            /* Galka   */ { kRank_B, kRank_C, kRank_A, kRank_E, kRank_E, kRank_C, kRank_F },
        };

        const Rank kJobRanks[kJob_Count][kAttr_Count] =
        {
            //              STR      DEX      VIT      AGI      INT      MND      CHR
            /* None */    { kRank_G, kRank_G, kRank_G, kRank_G, kRank_G, kRank_G, kRank_G },
            /* WAR  */    { kRank_A, kRank_C, kRank_C, kRank_C, kRank_F, kRank_E, kRank_E },
            /* MNK  */    { kRank_C, kRank_B, kRank_A, kRank_F, kRank_G, kRank_C, kRank_E },
            /* WHM  */    { kRank_D, kRank_F, kRank_C, kRank_E, kRank_E, kRank_A, kRank_C },
            /* BLM  */    { kRank_F, kRank_C, kRank_F, kRank_C, kRank_A, kRank_E, kRank_D },
            /* RDM  */    { kRank_D, kRank_D, kRank_E, kRank_E, kRank_C, kRank_B, kRank_D },
            /* THF  */    { kRank_D, kRank_A, kRank_C, kRank_B, kRank_C, kRank_G, kRank_G },
            /* PLD  */    { kRank_B, kRank_E, kRank_A, kRank_G, kRank_G, kRank_B, kRank_C },
            /* DRK  */    { kRank_A, kRank_C, kRank_B, kRank_D, kRank_C, kRank_G, kRank_G },
            /* BST  */    { kRank_D, kRank_C, kRank_C, kRank_F, kRank_E, kRank_D, kRank_A },
            /* BRD  */    { kRank_D, kRank_D, kRank_C, kRank_F, kRank_D, kRank_C, kRank_B },
            /* RNG  */    { kRank_E, kRank_D, kRank_C, kRank_A, kRank_E, kRank_C, kRank_E },
            /* SAM  */    { kRank_C, kRank_C, kRank_B, kRank_D, kRank_E, kRank_D, kRank_D },
            /* NIN  */    { kRank_C, kRank_B, kRank_B, kRank_B, kRank_D, kRank_G, kRank_F },
            /* DRG  */    { kRank_B, kRank_D, kRank_B, kRank_D, kRank_F, kRank_D, kRank_C },
            /* SMN  */    { kRank_G, kRank_E, kRank_G, kRank_D, kRank_B, kRank_B, kRank_B },
            /* BLU  */    { kRank_D, kRank_D, kRank_C, kRank_E, kRank_D, kRank_C, kRank_D },
            /* COR  */    { kRank_E, kRank_C, kRank_E, kRank_C, kRank_C, kRank_D, kRank_E },
            /* PUP  */    { kRank_E, kRank_B, kRank_C, kRank_C, kRank_E, kRank_E, kRank_C },
            /* DNC  */    { kRank_D, kRank_C, kRank_E, kRank_B, kRank_F, kRank_F, kRank_B },
            /* SCH  */    { kRank_F, kRank_D, kRank_E, kRank_D, kRank_C, kRank_C, kRank_C },
            /* GEO  */    { kRank_E, kRank_D, kRank_E, kRank_D, kRank_B, kRank_B, kRank_B },
            /* RUN  */    { kRank_B, kRank_D, kRank_C, kRank_B, kRank_D, kRank_B, kRank_F },
        };

        int InterpolateRankValue(const RankAnchors& anchors, int level)
        {
            level = ClampLevel(level);
            if (level <= 75)
            {
                const float t = (float)(level - 1) / 74.0f;
                return (int)std::floor((float)anchors.level1 + ((float)anchors.level75 - (float)anchors.level1) * t);
            }

            const float t = (float)(level - 75) / 24.0f;
            return (int)std::floor((float)anchors.level75 + ((float)anchors.level99 - (float)anchors.level75) * t);
        }

        AttributeBlock Add(const AttributeBlock& a, const AttributeBlock& b)
        {
            AttributeBlock out;
            for (int i = 0; i < kAttr_Count; ++i)
                out.value[(size_t)i] = a.value[(size_t)i] + b.value[(size_t)i];
            return out;
        }
    }

    const char* AttributeName(Attribute attr)
    {
        return (attr >= 0 && attr < kAttr_Count) ? kAttributeNames[attr] : "";
    }

    const char* RaceName(Race race)
    {
        return (race >= 0 && race < kRace_Count) ? kRaceNames[race] : "";
    }

    const char* JobName(Job job)
    {
        return (job >= 0 && job < kJob_Count) ? kJobNames[job] : "";
    }

    const char* ElementName(Element element)
    {
        return element >= 0 && element < kElement_Count ? kElementNames[element] : "";
    }

    const char* SkillName(Skill skill)
    {
        return skill >= 0 && skill < kSkill_Count ? kSkillNames[skill] : "";
    }

    const char* ModifierName(Modifier modifier)
    {
        return modifier >= 0 && modifier < kMod_Count ? kModifierNames[modifier] : "";
    }

    Rank RaceRank(Race race, Attribute attr)
    {
        if (race < 0 || race >= kRace_Count || attr < 0 || attr >= kAttr_Count)
            return kRank_G;
        return kRaceRanks[race][attr];
    }

    Rank JobRank(Job job, Attribute attr)
    {
        if (job < 0 || job >= kJob_Count || attr < 0 || attr >= kAttr_Count)
            return kRank_G;
        return kJobRanks[job][attr];
    }

    int ClampLevel(int level)
    {
        return std::clamp(level, 1, 99);
    }

    int CombatSkillContribution(int combatSkill)
    {
        combatSkill = std::max(0, combatSkill);
        if (combatSkill <= 200)
            return combatSkill;
        if (combatSkill <= 400)
            return 200 + (int)std::floor((combatSkill - 200) * 0.9f);
        if (combatSkill <= 600)
            return 380 + (int)std::floor((combatSkill - 400) * 0.8f);
        return 540 + (int)std::floor((combatSkill - 600) * 0.9f);
    }

    AttributeBlock BaseAttributes(Race race, Job mainJob, int mainLevel)
    {
        AttributeBlock out;
        for (int i = 0; i < kAttr_Count; ++i)
        {
            const Attribute attr = (Attribute)i;
            out[attr] =
                InterpolateRankValue(kRaceCurve[RaceRank(race, attr)], mainLevel) +
                InterpolateRankValue(kJobCurve[JobRank(mainJob, attr)], mainLevel);
        }
        return out;
    }

    AttributeBlock SupportJobAttributes(Job supportJob, int supportLevel)
    {
        AttributeBlock out;
        if (supportJob == kJob_None || supportLevel <= 0)
            return out;

        for (int i = 0; i < kAttr_Count; ++i)
        {
            const Attribute attr = (Attribute)i;
            out[attr] = InterpolateRankValue(kJobCurve[JobRank(supportJob, attr)], supportLevel) / 2;
        }
        return out;
    }

    AttributeBlock TotalAttributes(const CharacterBuild& build)
    {
        AttributeBlock out = Add(BaseAttributes(build.race, build.mainJob, build.mainLevel),
                                 SupportJobAttributes(build.supportJob, build.supportLevel));
        out = Add(out, build.merits);
        out = Add(out, build.gear);
        out = Add(out, build.buffs);
        return out;
    }

    ModifierBlock TotalModifiers(const CharacterBuild& build)
    {
        ModifierBlock out;
        for (int index = 0; index < kMod_Count; ++index)
            out.value[index] = build.gearModifiers.value[index] +
                build.traitModifiers.value[index] + build.effectModifiers.value[index];
        return out;
    }

    namespace
    {
        int ResourceByRank(const int rank, const int level, const bool mp)
        {
            static const float hp[7][5] = {{19,9,1,3,3},{17,8,1,3,3},{16,7,1,3,3},
                {14,6,0,3,3},{13,5,0,2,2},{11,4,0,2,2},{10,3,0,2,2}};
            static const float mana[7][3] = {{16,6,4},{14,5,4},{12,4,4},{10,3,4},
                {8,2,3},{6,1,2},{4,0.5f,1}};
            const int clamped = ClampLevel(level);
            if (mp)
                return static_cast<int>(mana[rank][0] + mana[rank][1] * (std::min)(59, clamped - 1) +
                    mana[rank][2] * (std::max)(0, clamped - 60));
            return static_cast<int>(hp[rank][0] + hp[rank][1] * (std::min)(29, clamped - 1) +
                hp[rank][2] * (std::max)(0, (std::min)(59, clamped - 1) - 29) +
                hp[rank][3] * (std::max)(0, (std::min)(74, clamped - 1) - 59) +
                hp[rank][4] * (std::max)(0, clamped - 75));
        }

        int HpJobRank(Job job)
        {
            static const int ranks[kJob_Count] = {6,0,0,4,5,3,3,1,1,2,4,3,2,3,2,6,2,3,3,3,5,4,1};
            return job >= 0 && job < kJob_Count ? ranks[job] : 6;
        }
        int MpJobRank(Job job)
        {
            static const int ranks[kJob_Count] = {6,6,6,0,0,1,6,4,5,6,2,6,6,6,6,0,2,6,4,6,1,1,3};
            return job >= 0 && job < kJob_Count ? ranks[job] : 6;
        }
    }

    int BaseMaxHP(Race race, Job mainJob, int mainLevel, Job supportJob, int supportLevel)
    {
        static const int raceRank[kRace_Count] = {2,1,4,3,0};
        int hp = ResourceByRank(raceRank[race], mainLevel, false) +
            ResourceByRank(HpJobRank(mainJob), mainLevel, false);
        if (supportJob != kJob_None && supportLevel > 0)
            hp += ResourceByRank(HpJobRank(supportJob), supportLevel, false) / 2;
        return (std::max)(1, hp);
    }

    int BaseMaxMP(Race race, Job mainJob, int mainLevel, Job supportJob, int supportLevel)
    {
        static const int raceRank[kRace_Count] = {2,4,0,2,6};
        int mp = ResourceByRank(raceRank[race], mainLevel, true) +
            ResourceByRank(MpJobRank(mainJob), mainLevel, true);
        if (supportJob != kJob_None && supportLevel > 0)
            mp += ResourceByRank(MpJobRank(supportJob), supportLevel, true) / 2;
        return (std::max)(0, mp);
    }

    DerivedStats Derive(const CharacterBuild& build, Skill attackSkill, Skill rangedSkill, Skill magicSkill)
    {
        DerivedStats out;
        out.attributes = TotalAttributes(build);
        out.elementalResistance = build.elementalResistance;
        const ModifierBlock mods = TotalModifiers(build);
        out.maxHp = (std::max)(1, BaseMaxHP(build.race, build.mainJob, build.mainLevel,
            build.supportJob, build.supportLevel) + mods[kMod_MaxHP]);
        out.maxMp = (std::max)(0, BaseMaxMP(build.race, build.mainJob, build.mainLevel,
            build.supportJob, build.supportLevel) + mods[kMod_MaxMP]);
        out.attack = Attack({build.skills[attackSkill], out.attributes[kAttr_STR], mods[kMod_Attack]});
        out.rangedAttack = Attack({build.skills[rangedSkill], out.attributes[kAttr_STR],
            mods[kMod_RangedAttack], 0, 1.0f, 1.0f, kAttack_Ranged});
        out.defense = Defense(0, out.attributes[kAttr_VIT], mods[kMod_Defense]);
        out.accuracy = Accuracy({build.skills[attackSkill], out.attributes[kAttr_DEX], 0,
            mods[kMod_Accuracy]});
        out.rangedAccuracy = RangedAccuracy(build.skills[rangedSkill], out.attributes[kAttr_AGI],
            mods[kMod_RangedAccuracy]);
        out.evasion = Evasion(build.skills[kSkill_Evasion], out.attributes[kAttr_AGI], mods[kMod_Evasion]);
        out.magicAttack = 100 + mods[kMod_MagicAttack];
        out.magicDefense = 100 + mods[kMod_MagicDefense];
        out.magicAccuracy = build.skills[magicSkill] + out.attributes[kAttr_INT] / 2 + mods[kMod_MagicAccuracy];
        out.magicEvasion = out.attributes[kAttr_MND] / 2 + mods[kMod_MagicEvasion];
        return out;
    }

    void ClampResources(CharacterBuild& build, const DerivedStats& derived)
    {
        build.resources.currentHp = std::clamp(build.resources.currentHp, 0, derived.maxHp);
        build.resources.currentMp = std::clamp(build.resources.currentMp, 0, derived.maxMp);
        build.resources.tp = std::clamp(build.resources.tp, 0, 3000);
    }

    int Accuracy(const AccuracyInput& input)
    {
        return CombatSkillContribution(input.combatSkill) +
               (int)std::floor(input.dexterity * 0.75f) +
               input.traitBonus +
               input.equipmentBonus +
               input.effectBonus +
               input.foodBonus;
    }

    int RangedAccuracy(int combatSkill, int agility, int bonuses)
    {
        return CombatSkillContribution(combatSkill) + (int)std::floor(agility * 0.75f) + bonuses;
    }

    int Evasion(int evasionSkill, int agility, int bonuses)
    {
        return CombatSkillContribution(evasionSkill) + (agility / 2) + bonuses;
    }

    int Defense(int defenseSkillOrBase, int vitality, int bonuses)
    {
        return defenseSkillOrBase + (int)std::floor(vitality * 1.5f) + bonuses;
    }

    int Attack(const AttackInput& input)
    {
        float strContribution = 0.75f;
        if (input.profile == kAttack_OffHand)
            strContribution = 0.5f;
        else if (input.profile == kAttack_HandToHand)
            strContribution = 0.625f;
        else if (input.profile == kAttack_Ranged)
            strContribution = 1.0f;

        const float base =
            8.0f +
            (float)input.combatSkill +
            (float)input.strength * strContribution +
            (float)input.equipmentBonus +
            (float)input.traitAbilityBonus;

        return (int)std::floor(base * input.abilityMultiplier * input.foodMultiplier);
    }

    float HitRateFromAccuracyDelta(int attackerAccuracy, int defenderEvasion)
    {
        const float rate = 75.0f + (float)(attackerAccuracy - defenderEvasion) * 0.5f;
        return std::clamp(rate, 20.0f, 95.0f);
    }

    int FStrDelta(int attackerStrength, int defenderVitality)
    {
        return attackerStrength - defenderVitality;
    }

    int CriticalDelta(int attackerDexterity, int defenderAgility)
    {
        return attackerDexterity - defenderAgility;
    }
}
