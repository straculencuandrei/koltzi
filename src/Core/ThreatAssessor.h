#pragma once
#include "../Common.h"
#include "TriageReport.h"
#include "PeReader.h"

namespace Koltzi {

class ThreatAssessor {
public:
    static void Assess(const PeReader& pe, TriageReport& report);
};

} // namespace Koltzi
