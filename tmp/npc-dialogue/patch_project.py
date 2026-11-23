import sys, os
d = sys.argv[1]
def edit(name, pairs):
    p = os.path.join(d, name); b = open(p, 'rb').read()
    for old, new in pairs:
        old = old.replace('\n', '\r\n').encode(); new = new.replace('\n', '\r\n').encode()
        if new in b: continue
        assert b.count(old) == 1, (name, old)
        b = b.replace(old, new, 1)
    open(p, 'wb').write(b)
edit('DATura.vcxproj', [
 ('    <ClCompile Include="npc_conversation_controller.cpp" />\n',
  '    <ClCompile Include="npc_conversation_controller.cpp" />\n    <ClCompile Include="npc_trigger_dialogue.cpp" />\n'),
 ('    <ClInclude Include="npc_conversation_controller.h" />\n',
  '    <ClInclude Include="npc_conversation_controller.h" />\n    <ClInclude Include="npc_trigger_dialogue.h" />\n'),
 ('    <None Include="npc_starter_quests.csv" />\n',
  '    <None Include="npc_starter_quests.csv" />\n    <None Include="npc_trigger_dialogue.csv" />\n'),
 ('copy /Y "$(ProjectDir)npc_starter_quests.csv" "$(OutDir)npc_starter_quests.csv"\n',
  'copy /Y "$(ProjectDir)npc_starter_quests.csv" "$(OutDir)npc_starter_quests.csv"\ncopy /Y "$(ProjectDir)npc_trigger_dialogue.csv" "$(OutDir)npc_trigger_dialogue.csv"\n'),
])
edit('DATura.vcxproj.filters', [
 ('    <ClInclude Include="npc_conversation_controller.h">\n      <Filter>Header Files</Filter>\n    </ClInclude>\n',
  '    <ClInclude Include="npc_conversation_controller.h">\n      <Filter>Header Files</Filter>\n    </ClInclude>\n    <ClInclude Include="npc_trigger_dialogue.h">\n      <Filter>Header Files</Filter>\n    </ClInclude>\n'),
 ('    <ClCompile Include="npc_conversation_controller.cpp"><Filter>Source Files</Filter></ClCompile>\n',
  '    <ClCompile Include="npc_conversation_controller.cpp"><Filter>Source Files</Filter></ClCompile>\n    <ClCompile Include="npc_trigger_dialogue.cpp"><Filter>Source Files</Filter></ClCompile>\n'),
 ('    <None Include="npc_starter_quests.csv" />\n',
  '    <None Include="npc_starter_quests.csv" />\n    <None Include="npc_trigger_dialogue.csv" />\n'),
])
print('patched project')
