#include "stdafx.h"
#include "ffxi_nation_selection.h"

namespace
{
const FFXINationSelection::Info kNationInfo[] =
{
    {
        "The Republic of Bastok",
        "Industry, invention, and resolve",
        "bcre",
        "A nation of industry and invention, Bastok rose from the rugged lands of Gustaberg through mining, engineering, and sheer determination. Its citizens value progress, discipline, and hard work, though old tensions still linger beneath the smoke of its forges.",
        235,
        RGB(80, 96, 210)
    },
    {
        "The Federation of Windurst",
        "Magic, scholarship, and the stars",
        "wcre",
        "A lush and mystical nation guided by magic, scholarship, and the wisdom of the Star Sibyl. Windurst is home to brilliant Tarutaru mages and proud Mithra hunters, where ancient traditions and playful curiosity shape daily life.",
        241,
        RGB(90, 150, 48)
    },
    {
        "The Kingdom of San d'Oria",
        "Knighthood, faith, and honor",
        "scre",
        "A proud kingdom of knights, faith, and noble houses, San d'Oria stands beneath the forests of Ronfaure as a bastion of honor and tradition. Its people revere courage, loyalty, and duty, though pride can be as sharp as any blade.",
        230,
        RGB(185, 32, 36)
    },
};
}

namespace FFXINationSelection
{
int Count()
{
    return (int)(sizeof(kNationInfo) / sizeof(kNationInfo[0]));
}

const Info& InfoForIndex(const int index)
{
    return kNationInfo[index];
}
}
