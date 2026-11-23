import sys
path = sys.argv[1]
b = open(path, 'rb').read()
assert b.count(b'\r\n') > 1000

def crlf(s):
    return s.replace('\r\n', '\n').replace('\n', '\r\n').encode()

# 1) include
inc = crlf('#include "npc_conversation_controller.h"\n')
assert b.count(inc) == 1
b = b.replace(inc, inc + crlf('#include "npc_trigger_dialogue.h"\n'), 1)

# 2) event selection in LoadRetailNpcConversation
fn = b.find(b'static bool LoadRetailNpcConversation(')
start = b.find(crlf('    for (const auto& scripts : dialogue.scripts)\n'), fn)
end_marker = crlf('    return false;\n}\n\nstatic bool PresentNpcEventStep')
end = b.find(end_marker, fn)
assert fn > 0 and start > fn and end > start and b.count(b'static bool PresentNpcEventStep') == 1
new_body = crlf('''    // LandSandBoat decides what a click does: a line from the dialog DAT, or
    // a specific event script (npc_trigger_dialogue.csv).
    NpcTriggerDialogue::Entry trigger;
    const bool hasTrigger = NpcTriggerDialogue::Find(FFXINpcPlacement::ZoneId(),
        placement.name, trigger);
    if (hasTrigger && trigger.kind == NpcTriggerDialogue::Kind::Text &&
        trigger.id < dialogue.messages.size() && !dialogue.messages[trigger.id].text.empty())
    {
        trace("entity=" + std::to_string(placement.entityId) +
            " server text=" + std::to_string(trigger.id));
        NpcConversationController::BeginFallback(dialogue,
            dialogue.messages[trigger.id].text, false);
        return false;
    }

    const FFXIEventTable::EntityScripts* own = nullptr;
    const FFXIEventTable::EntityScripts* zoneScripts = nullptr;
    for (const auto& scripts : dialogue.scripts)
    {
        if (scripts.entityId == placement.entityId)
            own = &scripts;
        else if (scripts.entityId == 0x7ffffff0u)
            zoneScripts = &scripts;
    }
    std::vector<const FFXIEventTable::Event*> candidates;
    if (hasTrigger && trigger.kind == NpcTriggerDialogue::Kind::Event)
    {
        // Shared zone events live in the 0x7ffffff0 block.
        for (const auto* scripts : { own, zoneScripts })
        {
            if (!scripts || !candidates.empty()) continue;
            for (const auto& event : scripts->events)
                if (event.id == trigger.id)
                {
                    candidates.push_back(&event);
                    break;
                }
        }
    }
    if (own)
        for (const auto& event : own->events)
            candidates.push_back(&event);
    if (candidates.empty())
    {
        trace("entity=" + std::to_string(placement.entityId) + " has no event script");
        return false;
    }

    for (const FFXIEventTable::Event* event : candidates)
    {
        NpcEventRuntime::Begin(dialogue.runtime, *event);
        NpcEventRuntime::State probe = dialogue.runtime;
        const NpcEventRuntime::Outcome firstStep = NpcEventRuntime::Advance(probe);
        if (firstStep.unsupportedOpcode)
        {
            char diagnostic[160]{};
            sprintf_s(diagnostic,
                "[DATura NPC dialogue] blocked event=%u pc=%zu opcode=0x%02x\\n",
                firstStep.eventId, firstStep.programCounter, firstStep.opcode);
            OutputDebugStringA(diagnostic);
        }
        if (NpcConversationController::WillPresent(dialogue))
        {
            trace("selected entity=" + std::to_string(placement.entityId) +
                " event=" + std::to_string(event->id) +
                " entry=" + std::to_string(event->entryOffset) +
                (hasTrigger && trigger.kind == NpcTriggerDialogue::Kind::Event &&
                    event->id == trigger.id ? " (LandSandBoat default)" : ""));
            return true;
        }
    }
    dialogue.runtime = {};
    trace("matched entity had no runnable retail dialogue");
    return false;
}

static bool PresentNpcEventStep''')
b = b[:start] + new_body + b[end + len(end_marker):]

# 3) StartNpcConversation fallback handling
old = crlf('''    if (LoadRetailNpcConversation(placement))
    {
        PresentNpcEventStep(owner);
        return;
    }
    NpcConversationController::BeginFallback(g_npcConversation.dialogue,
        NpcDialogueText(placement));
    ShowNpcConversationFrame(owner);
''')
new = crlf('''    if (LoadRetailNpcConversation(placement))
    {
        if (PresentNpcEventStep(owner))
            return;
        g_npcConversation.dialogue.runtime = {};
    }
    // A server text line may already be loaded as the fallback frame.
    if (!NpcConversationController::HasFallbackFrame(g_npcConversation.dialogue))
        NpcConversationController::BeginFallback(g_npcConversation.dialogue,
            NpcDialogueText(placement));
    ShowNpcConversationFrame(owner);
''')
assert b.count(old) == 1, b.count(old)
b = b.replace(old, new, 1)
open(path, 'wb').write(b)
print('patched main.cpp')
