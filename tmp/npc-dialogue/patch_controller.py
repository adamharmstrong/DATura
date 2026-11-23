import sys, os
d = sys.argv[1]
p = os.path.join(d, 'npc_conversation_controller.h'); s = open(p, newline='').read()
old = "void BeginFallback(State& state, const std::string& dialogue);"
assert s.count(old) == 1
s = s.replace(old, "// splitLines=false keeps a multi-line retail message in one window.\nvoid BeginFallback(State& state, const std::string& dialogue, bool splitLines = true);")
old = "Presentation PresentNext(State& state);"
assert s.count(old) == 1
s = s.replace(old, old + "\n// True when the loaded event would show a message or choice before ending.\n// Runs on a copy of the runtime, so the conversation itself does not move.\nbool WillPresent(const State& state);")
open(p, 'w', newline='').write(s)

p = os.path.join(d, 'npc_conversation_controller.cpp'); s = open(p, newline='').read()
old = "void BeginFallback(State& state, const std::string& dialogue)\n{\n    state.fallbackFrames.clear();\n    state.fallbackIndex = 0;\n"
assert s.count(old) == 1
s = s.replace(old, "void BeginFallback(State& state, const std::string& dialogue, const bool splitLines)\n{\n    state.fallbackFrames.clear();\n    state.fallbackIndex = 0;\n    if (!splitLines)\n    {\n        state.fallbackFrames.push_back(dialogue);\n        return;\n    }\n")
old = "void DismissMessage(State& state)\n{"
assert s.count(old) == 1
s = s.replace(old, """bool WillPresent(const State& state)
{
    NpcEventRuntime::State probe = state.runtime;
    for (int steps = 0; steps < 256; ++steps)
    {
        const NpcEventRuntime::Outcome outcome = NpcEventRuntime::Advance(probe);
        if (outcome.step == NpcEventRuntime::Step::Choice)
            return true;
        if (outcome.step != NpcEventRuntime::Step::Message)
            return false;
        const std::uint32_t id = probe.pendingMessage;
        if (id < state.messages.size() && !state.messages[id].text.empty())
            return true;
        NpcEventRuntime::DismissMessage(probe);
    }
    return false;
}

""" + old)
open(p, 'w', newline='').write(s)
print('patched controller')
