#pragma once

namespace EffectModelLayout
{
// Retail CMo effect resource helpers 1003FD00/1003FD80. Counts are
// triangle-list and strip material groups; the group-count table precedes
// the 16-byte material records. It is NOT aligned to a 16-byte boundary.
inline int MaterialOffset6(unsigned int lists, unsigned int strips)
{
    const unsigned int groups = lists + strips;
    return 8 + (groups % 4 == 0 ? groups * 2 : (groups & ~3u) * 2 + 6);
}
inline int VertexOffset6(unsigned int lists, unsigned int strips)
{
    return MaterialOffset6(lists, strips) + lists * 16;
}
} // namespace EffectModelLayout
