#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Koltzi {

enum class ThreatLevel {
    Clean,
    Suspicious,
    Malicious
};

enum class GhostMood {
    Idle,
    Sniffing,
    Happy,
    Puzzled,
    Alarmed
};

struct SectionInfo {
    std::string name;
    uint32_t virtualAddress = 0;
    uint32_t virtualSize = 0;
    uint32_t rawOffset = 0;
    uint32_t rawSize = 0;
    uint32_t characteristics = 0;
    double entropy = 0.0;
    bool isExecutable = false;
    bool isWritable = false;
    bool isSuspiciousEntropy = false; // > 7.2 for executable
    bool isRwx = false;               // Executable AND Writable (W^X violation)
};

struct SyscallFinding {
    uint64_t rva = 0;
    std::string section;
    std::string mnemonic;
    std::string instructionHex;
    std::string disassembly;
};

struct PebAccessFinding {
    uint64_t rva = 0;
    std::string section;
    std::string segment;    // FS or GS
    uint32_t offset = 0;     // 0x30 or 0x60
    std::string description;
    std::string disassembly;
};

struct ApiHashFinding {
    uint64_t rva = 0;
    std::string section;
    std::string description;
    std::string disassembly;
};

struct InjectionChainFinding {
    bool detected = false;
    bool hasRwxAllocation = false;
    bool hasProcessWrite = false;
    bool hasRemoteExecution = false;
    std::vector<std::string> chainedApis;
    std::string description;
};

struct StringFinding {
    std::string category;
    std::string matchedPattern;
    uint64_t offset = 0;
    bool isUtf16 = false;
};

struct ImportEntry {
    std::string dllName;
    std::vector<std::string> functions;
};

struct TriageReport {
    bool parseSuccess = false;
    std::string parseError;

    std::wstring filePath;
    std::string fileName;
    uint64_t fileSize = 0;
    double overallEntropy = 0.0;
    bool is64Bit = true;
    std::string machineType;
    std::string subsystem;
    uint32_t timestamp = 0;
    uint32_t entryPointRva = 0;

    std::vector<SectionInfo> sections;
    std::vector<ImportEntry> imports;
    std::vector<SyscallFinding> syscalls;
    std::vector<PebAccessFinding> pebAccesses;
    std::vector<ApiHashFinding> apiHashLoops;
    InjectionChainFinding injectionChain;
    std::vector<StringFinding> sensitiveStrings;

    double analysisTimeMs = 0.0;
    int threatScore = 0; // 0 to 100
    ThreatLevel threatLevel = ThreatLevel::Clean;
    GhostMood mood = GhostMood::Happy;
    std::string personalityDialogue;
    std::vector<std::string> technicalDetails;
};

} // namespace Koltzi
