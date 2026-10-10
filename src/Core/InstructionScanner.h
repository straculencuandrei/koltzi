#pragma once
#include "../Common.h"
#include "PeReader.h"
#include "TriageReport.h"
#include <Zydis/Zydis.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <set>

namespace Koltzi {

struct ApiHashInfo {
    std::string api;
    std::string algorithm;
    uint64_t hashValue = 0;
};

class InstructionScanner {
public:
    InstructionScanner();
    ~InstructionScanner();

    // Scan all executable sections of the given PE
    bool Scan(const PeReader& pe, TriageReport& report);

    // Static hash helper functions (available for tests and scanner)
    static uint32_t ComputeMetasploitHash(const std::string& moduleName, const std::string& functionName);
    static uint32_t HashRor13(const std::string& str, bool nullTerminated = false);
    static uint32_t HashDjb2(const std::string& str);
    static uint32_t HashFnv1a32(const std::string& str);
    static uint64_t HashFnv1a64(const std::string& str);
    static uint32_t HashCrc32(const std::string& str);
    static uint32_t HashSdbm(const std::string& str);

private:
    void InitHashDatabase();
    void ScanSection(const PeReader& pe, const SectionInfo& sec, TriageReport& report, std::unordered_map<uint32_t, std::string>& iatSlotMap);
    void EvaluateInjectionChain(const PeReader& pe, TriageReport& report, const std::unordered_map<uint32_t, std::string>& iatSlotMap);
    void DecompileFunctions(const PeReader& pe, TriageReport& report, const std::unordered_map<uint32_t, std::string>& iatSlotMap);
    DecompiledFunction ReconstructFunction(
        const PeReader& pe,
        uint64_t fnRva,
        const std::string& fnName,
        bool isEntry,
        const std::unordered_map<uint32_t, std::string>& iatSlotMap
    );

    ZydisFormatter m_formatter;
    std::unordered_map<uint32_t, ApiHashInfo> m_knownHashes32;
    std::unordered_map<uint64_t, ApiHashInfo> m_knownHashes64;
    std::vector<uint32_t> m_discoveredCallTargets;
};

} // namespace Koltzi
