#pragma once

#include "noesis_rapi.h"
#include "model_ff11.h"

#include <vector>

// Temporarily protects parser diagnostics while a secondary DAT is loaded.
// The prior diagnostics are restored automatically when this object leaves
// scope, including through an early return.
namespace FFXIParserDiagnostics
{
    class ScopedSnapshot
    {
    public:
        ScopedSnapshot();
        ~ScopedSnapshot();

        ScopedSnapshot(const ScopedSnapshot&) = delete;
        ScopedSnapshot& operator=(const ScopedSnapshot&) = delete;

    private:
        std::vector<ff11DatChunkDebug_t> datChunks;
        std::vector<ff11EnvironmentRecord_t> environmentRecords;
        std::vector<ff11GeneratorRecord_t> generatorRecords;
        std::vector<ff11KeyframeRecord_t> keyframeRecords;
    };
}
