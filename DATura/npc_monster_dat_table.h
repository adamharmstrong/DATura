#pragma once

// Curated standalone NPC and monster model DATs from the same AltanaViewer
// model catalogs used to generate ffxi_internal_lists.h. Paths are relative to
// the configured FFXI installation; DATura does not redistribute game assets.

struct FFXIStandaloneModelEntry
{
    const char* label;
    const char* dat;
};

struct FFXIStandaloneModelGroup
{
    const char* name;
    const FFXIStandaloneModelEntry* entries;
    int count;
};

#define FFXI_STANDALONE_MODEL_COUNT(array) (int)(sizeof(array) / sizeof(array[0]))

static const FFXIStandaloneModelEntry kFFXINpcOriginal[] =
{
    { "Lion",                 "ROM/3/34.dat" },
    { "Eald'narche",          "ROM/3/35.dat" },
    { "Kam'lanaut",           "ROM/3/36.dat" },
    { "Cid",                  "ROM/3/37.dat" },
    { "Prince Trion",         "ROM/3/38.dat" },
    { "Prince Pieuje",        "ROM/3/39.dat" },
    { "Princess Claidie",     "ROM/3/40.dat" },
    { "King Destin",          "ROM/3/41.dat" },
    { "Shamonde",             "ROM/3/42.dat" },
    { "Curilla",              "ROM/3/43.dat" },
    { "Cornelia",             "ROM/3/44.dat" },
    { "President Karst",      "ROM/3/45.dat" },
    { "Gumbah",               "ROM/3/46.dat" },
    { "Ajido-Marujido",       "ROM/3/48.dat" },
    { "Star Sibyl",           "ROM/3/49.dat" },
    { "Perih Vashai",         "ROM/3/50.dat" },
    { "Aldo",                 "ROM/3/52.dat" },
    { "Verena",               "ROM/3/53.dat" },
    { "Volker",               "ROM/3/55.dat" },
    { "Moogle",               "ROM/3/56.dat" },
    { "Maat",                 "ROM/3/100.dat" },
    { "Tosuka-Porika",        "ROM/4/8.dat" },
    { "Zonpa-Zippa",          "ROM/4/9.dat" },
    { "Apururu",              "ROM/4/10.dat" },
    { "Rukususu",             "ROM/4/11.dat" },
    { "Nanaa Mihgo",          "ROM/4/96.dat" },
    { "Zeid",                 "ROM/7/111.dat" },
};

static const FFXIStandaloneModelEntry kFFXINpcZilart[] =
{
    { "Jakoh Wahcondalo",      "ROM2/0/52.dat" },
    { "Gilgamesh",             "ROM2/16/10.dat" },
    { "Yve'noile",             "ROM3/8/19.dat" },
    { "Illisory",              "ROM2/16/12.dat" },
    { "Grav'iton",             "ROM2/16/13.dat" },
    { "Shadow Lord (head)",    "ROM2/16/14.dat" },
    { "Semih Lafihna",         "ROM2/16/15.dat" },
    { "Ark Angel GK",          "ROM2/16/17.dat" },
    { "Ark Angel EV",          "ROM2/16/18.dat" },
    { "Ark Angel HM",          "ROM2/16/19.dat" },
    { "Ark Angel TT",          "ROM2/16/20.dat" },
    { "Ark Angel MR",          "ROM2/16/21.dat" },
    { "Nomad Moogle",          "ROM2/23/71.dat" },
};

static const FFXIStandaloneModelEntry kFFXINpcPromathia[] =
{
    { "Mildaurion",            "ROM/261/56.dat" },
    { "Tenzen",                "ROM/151/126.dat" },
    { "Cherukiki",             "ROM3/0/63.dat" },
    { "Kukki-Chebukki",        "ROM3/0/64.dat" },
    { "Makki-Chebukki",        "ROM3/0/65.dat" },
    { "Ulmia",                 "ROM3/3/93.dat" },
    { "Prishe",                "ROM3/3/94.dat" },
    { "Enigmatic Youth",       "ROM3/3/95.dat" },
    { "Despachiaire",          "ROM3/3/96.dat" },
    { "Esha'ntarl",            "ROM3/5/106.dat" },
    { "Nag'molada",            "ROM3/5/107.dat" },
    { "Prishe (crown)",        "ROM3/5/112.dat" },
    { "Yve'noile",             "ROM3/8/19.dat" },
    { "Illisory",              "ROM3/8/20.dat" },
    { "Grav'iton",             "ROM3/8/21.dat" },
};

static const FFXIStandaloneModelEntry kFFXINpcAhtUrhgan[] =
{
    { "Raillefal",             "ROM4/2/5.dat" },
    { "Naja Salaheem",         "ROM4/2/6.dat" },
    { "Razfahd",               "ROM4/2/7.dat" },
    { "Gessho",                "ROM4/2/8.dat" },
    { "Wasuhd",                "ROM4/2/9.dat" },
    { "Ghatsad",               "ROM4/2/12.dat" },
    { "Qultada",               "ROM4/2/13.dat" },
    { "Aphmau",                "ROM4/2/14.dat" },
    { "Ovjang",                "ROM4/2/15.dat" },
    { "Prince Luzaf",          "ROM4/2/20.dat" },
    { "Mnejing",               "ROM4/2/21.dat" },
    { "Naja Salaheem (crown)", "ROM4/2/24.dat" },
    { "Naja Salaheem (shades)","ROM4/2/58.dat" },
    { "Razfahd (black)",       "ROM4/2/61.dat" },
};

static const FFXIStandaloneModelEntry kFFXINpcWings[] =
{
    { "Perih Vashai",          "ROM5/8/21.dat" },
    { "Ajido-Marujido",        "ROM5/8/22.dat" },
    { "Volker",                "ROM5/8/23.dat" },
    { "Gumbah",                "ROM5/8/24.dat" },
    { "Excenmille",            "ROM5/7/26.dat" },
    { "Cait Sith",             "ROM5/8/26.dat" },
    { "Rahal",                 "ROM5/8/30.dat" },
    { "Rholont",               "ROM5/8/31.dat" },
    { "Altennia",              "ROM5/8/33.dat" },
    { "Romaa Mihgo",           "ROM5/8/35.dat" },
    { "Lehko Habhoka",         "ROM5/7/32.dat" },
    { "Haudrale",              "ROM5/8/48.dat" },
    { "Aquila",                "ROM5/8/49.dat" },
    { "Lilisette",             "ROM5/8/50.dat" },
    { "Lady Lilith",           "ROM5/8/60.dat" },
    { "Noillurie",             "ROM5/8/67.dat" },
    { "Zeid",                  "ROM5/8/78.dat" },
};

static const FFXIStandaloneModelEntry kFFXINpcAddOns[] =
{
    { "Young Aldo",            "ROM6/0/11.dat" },
    { "Riko Kupenreich",       "ROM7/0/7.dat" },
    { "Mog",                   "ROM7/0/9.dat" },
    { "Kupiruru",              "ROM7/0/10.dat" },
    { "Toto Kupeliaure",       "ROM7/0/11.dat" },
    { "Shantotto Fomor",       "ROM8/0/0.dat" },
    { "Belle Shantotto",       "ROM8/0/1.dat" },
};

static const FFXIStandaloneModelEntry kFFXINpcSeekers[] =
{
    { "Arciela",               "ROM9/1/70.dat" },
    { "Arciela (alternate)",   "ROM9/1/100.dat" },
    { "Riana",                 "ROM9/1/101.dat" },
    { "Melvien de Malecroix",  "ROM9/1/103.dat" },
    { "Vortimere",             "ROM9/1/105.dat" },
    { "Svenja",                "ROM9/1/109.dat" },
    { "Teodor",                "ROM9/1/110.dat" },
    { "Berghent",              "ROM9/1/112.dat" },
    { "Greenith",              "ROM9/1/118.dat" },
};

static const FFXIStandaloneModelGroup kFFXINpcModelGroups[] =
{
    { "Original / Core",             kFFXINpcOriginal,  FFXI_STANDALONE_MODEL_COUNT(kFFXINpcOriginal) },
    { "Rise of the Zilart",          kFFXINpcZilart,    FFXI_STANDALONE_MODEL_COUNT(kFFXINpcZilart) },
    { "Chains of Promathia",         kFFXINpcPromathia, FFXI_STANDALONE_MODEL_COUNT(kFFXINpcPromathia) },
    { "Treasures of Aht Urhgan",     kFFXINpcAhtUrhgan, FFXI_STANDALONE_MODEL_COUNT(kFFXINpcAhtUrhgan) },
    { "Wings of the Goddess",        kFFXINpcWings,     FFXI_STANDALONE_MODEL_COUNT(kFFXINpcWings) },
    { "Scenario Add-ons",            kFFXINpcAddOns,    FFXI_STANDALONE_MODEL_COUNT(kFFXINpcAddOns) },
    { "Seekers of Adoulin",          kFFXINpcSeekers,   FFXI_STANDALONE_MODEL_COUNT(kFFXINpcSeekers) },
};
static const int kFFXINpcModelGroupCount = FFXI_STANDALONE_MODEL_COUNT(kFFXINpcModelGroups);

static const FFXIStandaloneModelEntry kFFXIMonsterOriginalWildlife[] =
{
    { "Ahriman",              "ROM/4/106.dat" },
    { "Rabbit",               "ROM/4/108.dat" },
    { "Bee",                  "ROM/4/111.dat" },
    { "Leech",                "ROM/4/113.dat" },
    { "Scorpion",             "ROM/4/117.dat" },
    { "Cockatrice",           "ROM/4/121.dat" },
    { "Slime",                "ROM/4/123.dat" },
    { "Goobbue",              "ROM/4/125.dat" },
    { "Mandragora",           "ROM/4/127.dat" },
    { "Tiger",                "ROM/5/3.dat" },
    { "Raptor",               "ROM/5/7.dat" },
    { "Doomed",               "ROM/5/9.dat" },
    { "Lizard",               "ROM/5/13.dat" },
    { "Dhalmel",              "ROM/5/15.dat" },
    { "Roc",                  "ROM/5/17.dat" },
    { "Sheep",                "ROM/5/19.dat" },
    { "Ram",                  "ROM/5/21.dat" },
    { "Pugil",                "ROM/5/23.dat" },
    { "Sea Monk",             "ROM/5/25.dat" },
    { "Crab",                 "ROM/5/27.dat" },
    { "Hound",                "ROM/5/33.dat" },
    { "Coeurl",               "ROM/5/35.dat" },
    { "Cactuar",              "ROM/5/38.dat" },
    { "Funguar",              "ROM/5/40.dat" },
    { "Morbol",               "ROM/5/42.dat" },
    { "Treant",               "ROM/5/46.dat" },
    { "Sapling",              "ROM/5/48.dat" },
    { "Crawler",              "ROM/5/50.dat" },
    { "Behemoth",             "ROM/5/52.dat" },
    { "Beetle",               "ROM/5/54.dat" },
    { "Opo-opo",              "ROM/5/60.dat" },
    { "Worm",                 "ROM/5/64.dat" },
    { "Bird",                 "ROM/5/79.dat" },
    { "Fly",                  "ROM/5/81.dat" },
};

static const FFXIStandaloneModelEntry kFFXIMonsterOriginalArcana[] =
{
    { "Bomb",                 "ROM/4/115.dat" },
    { "Doll",                 "ROM/5/1.dat" },
    { "Skeleton",             "ROM/5/29.dat" },
    { "Ghost",                "ROM/5/36.dat" },
    { "Hecteyes",             "ROM/5/44.dat" },
    { "Magic Pot",            "ROM/5/56.dat" },
    { "Dragon",               "ROM/5/62.dat" },
    { "Orcish Machine",       "ROM/5/66.dat" },
    { "Cardian",              "ROM/5/68.dat" },
    { "Golem",                "ROM/5/69.dat" },
    { "Earth Elemental",      "ROM/5/74.dat" },
    { "Evil Weapon",          "ROM/5/83.dat" },
    { "Shadow",               "ROM/6/32.dat" },
    { "Demon",                "ROM/7/44.dat" },
};

static const FFXIStandaloneModelEntry kFFXIMonsterOriginalBeastmen[] =
{
    { "Orc",                  "ROM/3/2.dat" },
    { "Goblin",               "ROM/3/59.dat" },
    { "Yagudo",               "ROM/6/70.dat" },
    { "Orc (armored)",        "ROM/6/94.dat" },
    { "Quadav",               "ROM/6/119.dat" },
    { "Gigas",                "ROM/7/50.dat" },
    { "Byakko",               "ROM/5/4.dat" },
    { "Suzaku",               "ROM/5/18.dat" },
    { "Elasmoth",             "ROM/5/53.dat" },
    { "Shadow Lord",          "ROM/7/80.dat" },
    { "Dynamis Lord",         "ROM/7/81.dat" },
    { "Overlord Bakgodek",    "ROM/9/14.dat" },
};

static const FFXIStandaloneModelEntry kFFXIMonsterZilart[] =
{
    { "Mandragora",           "ROM/95/68.dat" },
    { "Wyvern",               "ROM/97/74.dat" },
    { "Spheroid",             "ROM/97/80.dat" },
    { "Ark Angel HM",         "ROM2/16/19.dat" },
    { "Ark Angel EV",         "ROM2/16/18.dat" },
    { "Ark Angel MR",         "ROM2/16/21.dat" },
    { "Ark Angel TT",         "ROM2/16/20.dat" },
    { "Ark Angel GK",         "ROM2/16/17.dat" },
    { "Fenrir",               "ROM/97/67.dat" },
    { "Kam'lanaut",           "ROM/97/75.dat" },
    { "Tonberry",             "ROM2/22/23.dat" },
    { "Tonberry King",        "ROM2/22/85.dat" },
    { "Antica",               "ROM2/22/100.dat" },
    { "Sahagin",              "ROM/157/123.dat" },
    { "Dawnmaiden",           "ROM2/23/60.dat" },
};

static const FFXIStandaloneModelEntry kFFXIMonsterPromathia[] =
{
    { "Antlion",              "ROM/302/88.dat" },
    { "Snoll",                "ROM/224/22.dat" },
    { "Mammet",               "ROM3/5/23.dat" },
    { "Wanderer",             "ROM3/5/35.dat" },
    { "Weeper",               "ROM3/5/39.dat" },
    { "Gorger",               "ROM3/5/53.dat" },
    { "Diabolos",             "ROM/159/51.dat" },
    { "Diremite",             "ROM/156/17.dat" },
    { "Bugbear",              "ROM3/5/97.dat" },
    { "Tauri",                "ROM/185/4.dat" },
    { "Uragnite",             "ROM/259/15.dat" },
    { "Corse",                "ROM/184/119.dat" },
    { "Hippogryph",           "ROM/266/122.dat" },
    { "Bahamut",              "ROM3/7/36.dat" },
    { "Moblin",               "ROM3/7/71.dat" },
    { "Craver",               "ROM3/8/14.dat" },
    { "Ultima",               "ROM3/9/81.dat" },
    { "Omega",                "ROM3/9/83.dat" },
    { "Promathia",            "ROM3/10/29.dat" },
    { "Hpemde",               "ROM3/10/36.dat" },
    { "Phuabo",               "ROM3/10/39.dat" },
    { "Ghrah",                "ROM3/10/45.dat" },
    { "Aern",                 "ROM3/10/66.dat" },
};

static const FFXIStandaloneModelEntry kFFXIMonsterAhtUrhgan[] =
{
    { "Qiqirn",               "ROM4/2/53.dat" },
    { "Jnun",                 "ROM4/7/116.dat" },
    { "Karakul",              "ROM/249/50.dat" },
    { "Bhoot",                "ROM/249/51.dat" },
    { "Ameretat",             "ROM4/7/121.dat" },
    { "Eruca",                "ROM/258/51.dat" },
    { "Dahak",                "ROM/224/23.dat" },
    { "Mamool Ja",            "ROM4/7/124.dat" },
    { "Lamia",                "ROM4/8/36.dat" },
    { "Merrow",               "ROM4/8/61.dat" },
    { "Troll",                "ROM4/8/76.dat" },
    { "Colibri",              "ROM4/8/116.dat" },
    { "Yalungur",             "ROM4/8/117.dat" },
    { "Apkallu",              "ROM/258/87.dat" },
    { "Imp",                  "ROM4/8/122.dat" },
    { "Vanasarvik",           "ROM4/8/124.dat" },
    { "Qutrub",               "ROM4/9/42.dat" },
    { "Marid",                "ROM/258/93.dat" },
    { "Puk",                  "ROM/249/64.dat" },
    { "Chigoe",               "ROM/216/1.dat" },
    { "Draugar",              "ROM4/9/18.dat" },
    { "Soulflayer",           "ROM4/9/44.dat" },
    { "Acrolith",             "ROM4/9/47.dat" },
    { "Wivre",                "ROM4/9/55.dat" },
    { "Cerberus",             "ROM/250/62.dat" },
    { "Flan",                 "ROM4/9/65.dat" },
    { "Khimaira",             "ROM4/9/72.dat" },
    { "Poroggo",              "ROM4/9/77.dat" },
    { "Alexander",            "ROM4/9/100.dat" },
    { "Gulool Ja Ja",         "ROM4/10/1.dat" },
    { "Medusa",               "ROM4/10/3.dat" },
    { "Gurfurlur",            "ROM4/10/5.dat" },
};

static const FFXIStandaloneModelEntry kFFXIMonsterWings[] =
{
    { "Gnat",                 "ROM5/5/83.dat" },
    { "Ladybug",              "ROM/207/7.dat" },
    { "Slug",                 "ROM/207/8.dat" },
    { "Rafflesia",            "ROM/207/9.dat" },
    { "Pixie",                "ROM/197/76.dat" },
    { "Dark Pixie",           "ROM5/6/5.dat" },
    { "Gnole",                "ROM5/6/6.dat" },
    { "Orc",                  "ROM5/6/9.dat" },
    { "Yagudo",               "ROM5/6/49.dat" },
    { "Quadav",               "ROM5/6/89.dat" },
    { "Atomos",               "ROM5/7/1.dat" },
    { "Cait Sith",            "ROM5/7/4.dat" },
    { "Shadow Lord",          "ROM5/7/16.dat" },
    { "Lady Lilith",          "ROM5/7/34.dat" },
    { "Lilith Ascendant",     "ROM5/7/35.dat" },
    { "Lycopodium",           "ROM/250/2.dat" },
    { "Smilodon",             "ROM/267/88.dat" },
    { "Funguar",              "ROM5/8/20.dat" },
    { "Colibri",              "ROM5/9/35.dat" },
    { "Malboro",              "ROM5/9/36.dat" },
    { "Wivre",                "ROM4/9/55.dat" },
    { "Hippogryph",           "ROM5/9/47.dat" },
    { "Djinn",                "ROM5/9/50.dat" },
    { "Lynx",                 "ROM/258/50.dat" },
};

static const FFXIStandaloneModelEntry kFFXIMonsterAddOns[] =
{
    { "Seed Mandragora",      "ROM6/0/13.dat" },
    { "Seed Goblin",          "ROM6/0/14.dat" },
    { "Seed Orc",             "ROM6/0/15.dat" },
    { "Seed Quadav",          "ROM6/0/16.dat" },
    { "Seed Yagudo",          "ROM6/0/17.dat" },
    { "Henchman Moogle",      "ROM7/0/13.dat" },
    { "Riko Kupenreich",      "ROM7/0/14.dat" },
    { "Shantotto Fomor",      "ROM8/0/63.dat" },
};

static const FFXIStandaloneModelEntry kFFXIMonsterSeekers[] =
{
    { "Orc",                  "ROM9/0/1.dat" },
    { "Bztavian",             "ROM9/0/28.dat" },
    { "Rockfin",              "ROM9/0/33.dat" },
    { "Yggdreant",            "ROM9/0/38.dat" },
    { "Gabbrath",             "ROM9/0/43.dat" },
    { "Velkk (sword)",        "ROM9/0/48.dat" },
    { "Velkk (wand)",         "ROM9/0/53.dat" },
    { "Leafkin",              "ROM9/0/58.dat" },
    { "Twitherym",            "ROM9/0/63.dat" },
    { "Chapuli",              "ROM9/0/73.dat" },
    { "Pitcherplant",         "ROM9/0/78.dat" },
    { "Craklaw",              "ROM9/0/83.dat" },
    { "Matamata",             "ROM9/0/88.dat" },
    { "Acuex",                "ROM9/0/98.dat" },
    { "Heartwing",            "ROM9/0/103.dat" },
    { "Cehuetzi",             "ROM9/0/113.dat" },
    { "Ram",                  "ROM9/1/64.dat" },
    { "Poroggo",              "ROM9/1/66.dat" },
    { "Umbril",               "ROM9/7/77.dat" },
};

static const FFXIStandaloneModelGroup kFFXIMonsterModelGroups[] =
{
    { "Original - Wildlife",           kFFXIMonsterOriginalWildlife, FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterOriginalWildlife) },
    { "Original - Arcana / Undead",    kFFXIMonsterOriginalArcana,   FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterOriginalArcana) },
    { "Original - Beastmen / Bosses",  kFFXIMonsterOriginalBeastmen, FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterOriginalBeastmen) },
    { "Rise of the Zilart",            kFFXIMonsterZilart,           FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterZilart) },
    { "Chains of Promathia",           kFFXIMonsterPromathia,        FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterPromathia) },
    { "Treasures of Aht Urhgan",       kFFXIMonsterAhtUrhgan,        FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterAhtUrhgan) },
    { "Wings of the Goddess",          kFFXIMonsterWings,            FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterWings) },
    { "Scenario Add-ons",              kFFXIMonsterAddOns,           FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterAddOns) },
    { "Seekers of Adoulin",            kFFXIMonsterSeekers,          FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterSeekers) },
};
static const int kFFXIMonsterModelGroupCount = FFXI_STANDALONE_MODEL_COUNT(kFFXIMonsterModelGroups);

static inline int FFXIStandaloneModel_TotalEntries(const FFXIStandaloneModelGroup* groups, int groupCount)
{
    int total = 0;
    for (int i = 0; i < groupCount; ++i)
        total += groups[i].count;
    return total;
}

static inline const FFXIStandaloneModelEntry* FFXIStandaloneModel_GetEntry(
    const FFXIStandaloneModelGroup* groups, int groupCount, int flatIndex)
{
    if (!groups || flatIndex < 0)
        return nullptr;
    for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex)
    {
        if (flatIndex < groups[groupIndex].count)
            return &groups[groupIndex].entries[flatIndex];
        flatIndex -= groups[groupIndex].count;
    }
    return nullptr;
}
