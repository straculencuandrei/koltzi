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
    bool isSuspiciousEntropy = false; // > 7.2 for executable code
    bool isRwx = false;               // Executable AND Writable (W^X violation)
};

struct SyscallFinding {
    uint64_t rva = 0;
    std::string section;
    std::string mnemonic;
    std::string instructionHex;
    std::string disassembly;
    bool isLegitimateJitOrHook = false;
    bool isStubPattern = false; // verified mov r10, rcx; mov eax, <ssn>; syscall pattern
    uint32_t ssn = 0;
};

struct PebAccessFinding {
    uint64_t rva = 0;
    std::string section;
    std::string segment;    // FS or GS
    uint32_t offset = 0;     // 0x30 or 0x60
    std::string description;
    std::string disassembly;
    bool isCrtTlsInit = false; // Normal CRT __security_init_cookie or TLS index read
    bool isFullLdrWalk = false; // Followed by actual PEB->Ldr and ModuleList/Export traversal
};

struct ApiHashFinding {
    uint64_t rva = 0;
    std::string section;
    std::string description;
    std::string disassembly;
    bool isLikelyCryptoOrStringHash = false;
    std::string matchedApi;
    std::string hashAlgorithm;
    uint64_t hashValue = 0;
};

struct InjectionChainFinding {
    bool detected = false;
    bool hasRwxAllocation = false;
    bool hasProcessWrite = false;
    bool hasRemoteExecution = false;
    bool isCoLocated = false; // Called within the same function or direct caller/callee
    bool hasRwxProtectArg = false; // PAGE_EXECUTE_READWRITE passed to allocation/protection API
    bool isProcessHollowing = false;
    std::vector<std::string> chainedApis;
    std::string description;
};

struct StringFinding {
    std::string category;
    std::string matchedPattern;
    uint64_t offset = 0;
    bool isUtf16 = false;
    bool isSuppressedByContext = false; // e.g. legitimate browser profile wizard
};

struct ImportEntry {
    std::string dllName;
    std::vector<std::string> functions;
    std::vector<uint32_t> iatRvas;
};

struct SignatureInfo {
    bool isSigned = false;
    bool isValid = false;
    bool isTrustedVendor = false;
    std::string signerSubject;
    std::string signerIssuer;
    std::string statusText = "Unsigned binary";
};

struct TriageLogEntry {
    std::string timestamp; // e.g. "0.012 ms"
    std::string level;     // "INFO", "WARN", "CRIT", "PASS", "AUDIT"
    std::string subsystem; // "HASH", "CERT", "HEADER", "ZYDIS", "STRINGS", "ASSESS"
    std::string message;
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

    // Cryptographic Hashes (Standard AV / VirusTotal style)
    std::string md5;
    std::string sha1;
    std::string sha256;
    std::string imphash;

    // Authenticode Digital Signature
    SignatureInfo signature;

    // Legitimate Browser / Software Identity
    bool isLegitimateBrowser = false;
    std::string browserIdentity;

    std::vector<SectionInfo> sections;
    std::vector<ImportEntry> imports;
    std::vector<SyscallFinding> syscalls;
    std::vector<PebAccessFinding> pebAccesses;
    std::vector<ApiHashFinding> apiHashLoops;
    InjectionChainFinding injectionChain;
    std::vector<StringFinding> sensitiveStrings;

    // Detailed Audit & Triage Log
    std::vector<TriageLogEntry> logEntries;

    void AddLog(const std::string& subsystemName, const std::string& level, const std::string& msg, double timeMs = 0.0) {
        TriageLogEntry entry;
        char timeBuf[32];
        snprintf(timeBuf, sizeof(timeBuf), "+%.2fms", timeMs);
        entry.timestamp = timeBuf;
        entry.subsystem = subsystemName;
        entry.level = level;
        entry.message = msg;
        logEntries.push_back(std::move(entry));
    }

    double analysisTimeMs = 0.0;
    int threatScore = 0; // 0 to 100
    ThreatLevel threatLevel = ThreatLevel::Clean;
    GhostMood mood = GhostMood::Happy;
    std::string personalityDialogue;
    std::vector<std::string> technicalDetails;
};

} // namespace Koltzi
