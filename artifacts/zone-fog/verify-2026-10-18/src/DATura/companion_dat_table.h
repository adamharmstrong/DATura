#pragma once

// Curated pet, mount, and summon models. The entry/group types and count macro
// are declared by npc_monster_dat_table.h, which is included immediately before
// this header by main.cpp.

static const FFXIStandaloneModelEntry kFFXICompanionChocobos[] =
{
    { "Chocobo 1",                  "ROM/3/60.dat" },
    { "Chocobo 2",                  "ROM/3/61.dat" },
    { "Chocobo 3",                  "ROM/3/62.dat" },
    { "Chocobo 4",                  "ROM/3/63.dat" },
    { "Chocobo (alternate)",        "ROM/3/103.dat" },
    { "Rental Chocobo",             "ROM/90/56.dat" },
    { "Chocobo Egg",                "ROM/168/21.dat" },
    { "Chocobo Chick",              "ROM/168/22.dat" },
    { "Young Chocobo (yellow)",     "ROM/168/23.dat" },
    { "Young Chocobo (black)",      "ROM/169/42.dat" },
    { "Young Chocobo (blue)",       "ROM/169/43.dat" },
    { "Young Chocobo (red)",        "ROM/169/44.dat" },
    { "Young Chocobo (green)",      "ROM/169/45.dat" },
    { "White Chocobo",              "ROM/204/1.dat" },
    { "Armored Black Chocobo",      "ROM/211/25.dat" },
};

static const FFXIStandaloneModelEntry kFFXICompanionAvatars[] =
{
    { "Carbuncle",                  "ROM/97/66.dat" },
    { "Fenrir",                     "ROM/97/67.dat" },
    { "Ifrit",                      "ROM/97/68.dat" },
    { "Titan",                      "ROM/97/69.dat" },
    { "Leviathan",                  "ROM/97/70.dat" },
    { "Garuda",                     "ROM/97/71.dat" },
    { "Shiva",                      "ROM/97/72.dat" },
    { "Ramuh",                      "ROM/97/73.dat" },
    { "Carbuncle Prime",            "ROM/97/86.dat" },
    { "Ifrit Prime",                "ROM/119/58.dat" },
    { "Titan Prime",                "ROM/119/59.dat" },
    { "Leviathan Prime",            "ROM/119/60.dat" },
    { "Garuda Prime",               "ROM/119/61.dat" },
    { "Shiva Prime",                "ROM/119/62.dat" },
    { "Ramuh Prime",                "ROM/119/63.dat" },
    { "Diabolos",                   "ROM/159/51.dat" },
    { "Cait Sith",                  "ROM/263/5.dat" },
    { "Atomos",                     "ROM5/7/1.dat" },
    { "Alexander",                  "ROM4/9/100.dat" },
    { "Odin 1",                     "ROM/239/127.dat" },
    { "Odin 2",                     "ROM/240/0.dat" },
};

static const FFXIStandaloneModelEntry kFFXICompanionAutomatons[] =
{
    { "Automaton",                  "ROM/165/88.dat" },
    { "Automaton (Aht Urhgan 1)",   "ROM4/2/10.dat" },
    { "Automaton (Aht Urhgan 2)",   "ROM4/2/11.dat" },
    { "Ovjang",                     "ROM4/2/15.dat" },
    { "Mnejing",                    "ROM4/2/21.dat" },
};

static const FFXIStandaloneModelEntry kFFXICompanionWyverns[] =
{
    { "Pet Wyvern",                 "ROM/97/74.dat" },
    { "Wyvern",                     "ROM/97/81.dat" },
    { "Ark Angel Wyvern",           "ROM/119/76.dat" },
    { "Dynamis Wyvern",             "ROM/130/108.dat" },
    { "Blue Wyvern",                "ROM/249/71.dat" },
    { "Abyssea Wyvern",             "ROM/272/90.dat" },
    { "Tiamat Wyvern",              "ROM/286/80.dat" },
};

static const FFXIStandaloneModelEntry kFFXICompanionFamiliars[] =
{
    { "Rabbit",                     "ROM/4/108.dat" },
    { "Bee",                        "ROM/4/111.dat" },
    { "Crab",                       "ROM/5/27.dat" },
    { "Sheep",                      "ROM/5/19.dat" },
    { "Crawler",                    "ROM/5/50.dat" },
    { "Funguar",                    "ROM/5/40.dat" },
    { "Tiger",                      "ROM/5/3.dat" },
    { "Mandragora",                 "ROM/95/68.dat" },
    { "Ladybug",                    "ROM/207/7.dat" },
    { "Slug",                       "ROM/207/8.dat" },
    { "Rafflesia",                  "ROM/207/9.dat" },
    { "Lynx",                       "ROM/258/50.dat" },
    { "Pixie",                      "ROM/197/76.dat" },
};

static const FFXIStandaloneModelGroup kFFXICompanionGroups[] =
{
    { "Chocobos",              kFFXICompanionChocobos,   FFXI_STANDALONE_MODEL_COUNT(kFFXICompanionChocobos) },
    { "Avatars / Summons",     kFFXICompanionAvatars,    FFXI_STANDALONE_MODEL_COUNT(kFFXICompanionAvatars) },
    { "Automatons",            kFFXICompanionAutomatons, FFXI_STANDALONE_MODEL_COUNT(kFFXICompanionAutomatons) },
    { "Wyverns",               kFFXICompanionWyverns,    FFXI_STANDALONE_MODEL_COUNT(kFFXICompanionWyverns) },
    { "Familiar / Jug Pets",   kFFXICompanionFamiliars,  FFXI_STANDALONE_MODEL_COUNT(kFFXICompanionFamiliars) },
};
static const int kFFXICompanionGroupCount = FFXI_STANDALONE_MODEL_COUNT(kFFXICompanionGroups);

