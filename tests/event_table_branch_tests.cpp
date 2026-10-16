#include "../DATura/ffxi_event_table.h"
#include <cassert>
#include <iostream>
int main()
{
    std::vector<unsigned char> bytes;
    auto u32 = [&](unsigned v) { for(int i=0;i<4;++i) bytes.push_back((v>>(8*i))&255); };
    auto u16 = [&](unsigned v) { bytes.push_back(v&255); bytes.push_back(v>>8); };
    // Two entry points share an actor block. Event 42 jumps back to absolute PC 0.
    u32(1); u32(28); u32(123); u32(2);
    u16(0); u16(1); u16(65535); u16(42); u32(0); u32(4);
    bytes.insert(bytes.end(), {0, 1, 0, 0});
    std::vector<FFXIEventTable::EntityScripts> parsed;
    assert(FFXIEventTable::Parse(bytes, parsed));
    assert(parsed.size()==1 && parsed[0].events.size()==2);
    const auto& e=parsed[0].events[1];
    assert(e.id==42 && e.entryOffset==1 && e.code.size()==4);
    assert(e.code[e.entryOffset]==1 && e.code[0]==0);
    std::cout << "PASS: nonzero event entry retains absolute branch address space\n";
}
