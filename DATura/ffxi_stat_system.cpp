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
