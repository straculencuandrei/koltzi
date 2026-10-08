#pragma once
#include "Common.h"
#include "PeReader.h"
#include "TriageReport.h"
#include <Zydis/Zydis.h>
#include <vector>
#include <string>

namespace Koltzi {

class InstructionScanner {
public:
    InstructionScanner();
    ~InstructionScanner();

    // Scan all executable sections of the given PE
    bool Scan(const PeReader& pe, TriageReport& report);

private:
    void ScanSection(const PeReader& pe, const SectionInfo& sec, TriageReport& report);
    void EvaluateInjectionChain(const PeReader& pe, TriageReport& report);

    ZydisFormatter m_formatter;
};

} // namespace Koltzi
