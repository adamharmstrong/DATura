#include "../DATura/ffxi_stat_system.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main()
{
    using namespace FFXIStats;
    CharacterBuild hume;
    hume.mainJob = kJob_WAR;
    hume.mainLevel = 50;
    hume.skills[kSkill_Sword] = 150;
    hume.skills[kSkill_Evasion] = 120;
    hume.gearModifiers[kMod_MaxHP] = 25;
    hume.gearModifiers[kMod_Accuracy] = 10;
    hume.elementalResistance[kElement_Fire] = 12;
    const DerivedStats base = Derive(hume, kSkill_Sword);
    Check(base.maxHp > 25 && base.maxMp >= 0, "HP/MP derivation failed");
    Check(base.accuracy > 10 && base.attack > 0 && base.evasion > 0,
        "physical derived stats failed");
    Check(base.elementalResistance[kElement_Fire] == 12,
        "elemental resistance derivation failed");

    CharacterBuild taru = hume;
    taru.race = kRace_Tarutaru;
    Check(BaseMaxMP(taru.race, taru.mainJob, taru.mainLevel, kJob_None, 0) >
        BaseMaxMP(kRace_Galka, taru.mainJob, taru.mainLevel, kJob_None, 0),
        "race-specific MP growth failed");
    Check(BaseMaxHP(kRace_Galka, hume.mainJob, hume.mainLevel, kJob_None, 0) >
        BaseMaxHP(kRace_Tarutaru, hume.mainJob, hume.mainLevel, kJob_None, 0),
        "race-specific HP growth failed");

    hume.resources = {99999, 99999, 99999};
    ClampResources(hume, base);
    Check(hume.resources.currentHp == base.maxHp && hume.resources.currentMp == base.maxMp &&
        hume.resources.tp == 3000, "resource clamping failed");
    std::cout << "All stat-system checks passed.\n";
}
