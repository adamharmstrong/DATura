#include "stdafx.h"
#include "ffxi_parser_diagnostics.h"

namespace FFXIParserDiagnostics
{
ScopedSnapshot::ScopedSnapshot()
    : datChunks(std::move(gFF11LastDatChunks))
    , environmentRecords(std::move(gFF11LastEnvironmentRecords))
    , generatorRecords(std::move(gFF11LastGeneratorRecords))
    , keyframeRecords(std::move(gFF11LastKeyframeRecords))
{
}

ScopedSnapshot::~ScopedSnapshot()
{
    gFF11LastDatChunks = std::move(datChunks);
    gFF11LastEnvironmentRecords = std::move(environmentRecords);
    gFF11LastGeneratorRecords = std::move(generatorRecords);
    gFF11LastKeyframeRecords = std::move(keyframeRecords);
}
}
