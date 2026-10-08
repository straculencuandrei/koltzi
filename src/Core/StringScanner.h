#pragma once
#include "Common.h"
#include "PeReader.h"
#include "TriageReport.h"
#include <string>
#include <vector>

namespace Koltzi {

struct TargetPattern {
    std::string category;
    std::string pattern;
    std::string displayDescription;
};

class StringScanner {
public:
    StringScanner();
    ~StringScanner();

    bool Scan(const PeReader& pe, TriageReport& report);

private:
    std::vector<TargetPattern> m_targets;
};

} // namespace Koltzi
