#pragma once
#include "../Common.h"
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

struct DisassembledInstruction {
    uint64_t rva = 0;
    std::string hexBytes;
    std::string mnemonic;
    std::string operands;
    std::string comment;
};

struct DecompiledFunction {
    uint64_t rva = 0;
    std::string name;
    uint32_t size = 0;
    uint32_t instructionCount = 0;
    uint32_t branchCount = 0;
    bool hasSyscall = false;
    bool hasPebAccess = false;
    bool hasApiHash = false;
    bool hasInjection = false;
    bool isEntryPoint = false;
    std::vector<std::string> calledApis;
    std::vector<DisassembledInstruction> instructions;
    std::vector<std::string> pseudocodeLines;
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

    // Legitimate Installer / Setup Package Detection
    bool isInstaller = false;
    std::string installerType;

    // PE Overlay Analysis
    uint64_t overlayOffset = 0;
    uint64_t overlaySize = 0;
    double overlayEntropy = 0.0;
    double overlayRatio = 0.0;
    std::string overlayType;
    bool hasSuspiciousOverlay = false;

    std::vector<SectionInfo> sections;
    std::vector<ImportEntry> imports;
    std::vector<SyscallFinding> syscalls;
    std::vector<PebAccessFinding> pebAccesses;
    std::vector<ApiHashFinding> apiHashLoops;
    InjectionChainFinding injectionChain;
    std::vector<StringFinding> sensitiveStrings;
    std::vector<DecompiledFunction> decompiledFunctions;

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

    void Log(const std::string& subsystemName, const std::string& level, const std::string& msg, double timeMs = 0.0) {
        AddLog(subsystemName, level, msg, timeMs);
    }

    double analysisTimeMs = 0.0;
    int threatScore = 0; // 0 to 100
    ThreatLevel threatLevel = ThreatLevel::Clean;
    GhostMood mood = GhostMood::Happy;
    std::string personalityDialogue;
    std::vector<std::string> dialogueLines;
    size_t activeDialogueIndex = 0;

    std::string GetCurrentDialogue() const {
        if (!dialogueLines.empty() && activeDialogueIndex < dialogueLines.size()) {
            return dialogueLines[activeDialogueIndex];
        }
        return personalityDialogue;
    }

    void CycleDialogue() {
        if (!dialogueLines.empty()) {
            activeDialogueIndex = (activeDialogueIndex + 1) % dialogueLines.size();
            personalityDialogue = dialogueLines[activeDialogueIndex];
        }
    }

    std::vector<std::string> technicalDetails;

    std::string ToJson() const {
        auto EscapeJson = [](const std::string& s) {
            std::string out;
            out.reserve(s.size() + 16);
            for (char c : s) {
                switch (c) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                            out += buf;
                        } else {
                            out += c;
                        }
                        break;
                }
            }
            return out;
        };

        std::string json = "{\n";
        json += "  \"parseSuccess\": " + std::string(parseSuccess ? "true" : "false") + ",\n";
        json += "  \"parseError\": \"" + EscapeJson(parseError) + "\",\n";
        json += "  \"fileName\": \"" + EscapeJson(fileName) + "\",\n";
        json += "  \"filePath\": \"" + EscapeJson(WideToUtf8(filePath)) + "\",\n";
        json += "  \"fileSize\": " + std::to_string(fileSize) + ",\n";
        json += "  \"is64Bit\": " + std::string(is64Bit ? "true" : "false") + ",\n";
        json += "  \"machineType\": \"" + EscapeJson(machineType) + "\",\n";
        json += "  \"subsystem\": \"" + EscapeJson(subsystem) + "\",\n";
        json += "  \"timestamp\": " + std::to_string(timestamp) + ",\n";
        json += "  \"entryPointRva\": " + std::to_string(entryPointRva) + ",\n";
        json += "  \"overallEntropy\": " + std::to_string(overallEntropy) + ",\n";
        json += "  \"threatScore\": " + std::to_string(threatScore) + ",\n";
        json += "  \"threatLevel\": " + std::to_string(static_cast<int>(threatLevel)) + ",\n";
        json += "  \"mood\": " + std::to_string(static_cast<int>(mood)) + ",\n";
        json += "  \"personalityDialogue\": \"" + EscapeJson(personalityDialogue) + "\",\n";
        json += "  \"analysisTimeMs\": " + std::to_string(analysisTimeMs) + ",\n";
        json += "  \"isInstaller\": " + std::string(isInstaller ? "true" : "false") + ",\n";
        json += "  \"installerType\": \"" + EscapeJson(installerType) + "\",\n";
        json += "  \"overlayOffset\": " + std::to_string(overlayOffset) + ",\n";
        json += "  \"overlaySize\": " + std::to_string(overlaySize) + ",\n";
        json += "  \"overlayEntropy\": " + std::to_string(overlayEntropy) + ",\n";
        json += "  \"overlayRatio\": " + std::to_string(overlayRatio) + ",\n";
        json += "  \"overlayType\": \"" + EscapeJson(overlayType) + "\",\n";
        json += "  \"hasSuspiciousOverlay\": " + std::string(hasSuspiciousOverlay ? "true" : "false") + ",\n";
        json += "  \"isLegitimateBrowser\": " + std::string(isLegitimateBrowser ? "true" : "false") + ",\n";
        json += "  \"browserIdentity\": \"" + EscapeJson(browserIdentity) + "\",\n";
        json += "  \"md5\": \"" + EscapeJson(md5) + "\",\n";
        json += "  \"sha1\": \"" + EscapeJson(sha1) + "\",\n";
        json += "  \"sha256\": \"" + EscapeJson(sha256) + "\",\n";
        json += "  \"imphash\": \"" + EscapeJson(imphash) + "\",\n";
        json += "  \"signature\": {\n";
        json += "    \"isSigned\": " + std::string(signature.isSigned ? "true" : "false") + ",\n";
        json += "    \"isValid\": " + std::string(signature.isValid ? "true" : "false") + ",\n";
        json += "    \"isTrustedVendor\": " + std::string(signature.isTrustedVendor ? "true" : "false") + ",\n";
        json += "    \"signerSubject\": \"" + EscapeJson(signature.signerSubject) + "\",\n";
        json += "    \"signerIssuer\": \"" + EscapeJson(signature.signerIssuer) + "\",\n";
        json += "    \"statusText\": \"" + EscapeJson(signature.statusText) + "\"\n";
        json += "  },\n";
        json += "  \"sections\": [\n";
        for (size_t i = 0; i < sections.size(); ++i) {
            const auto& s = sections[i];
            json += "    {\n";
            json += "      \"name\": \"" + EscapeJson(s.name) + "\",\n";
            json += "      \"virtualAddress\": " + std::to_string(s.virtualAddress) + ",\n";
            json += "      \"virtualSize\": " + std::to_string(s.virtualSize) + ",\n";
            json += "      \"rawOffset\": " + std::to_string(s.rawOffset) + ",\n";
            json += "      \"rawSize\": " + std::to_string(s.rawSize) + ",\n";
            json += "      \"entropy\": " + std::to_string(s.entropy) + ",\n";
            json += "      \"isExecutable\": " + std::string(s.isExecutable ? "true" : "false") + ",\n";
            json += "      \"isWritable\": " + std::string(s.isWritable ? "true" : "false") + ",\n";
            json += "      \"isSuspiciousEntropy\": " + std::string(s.isSuspiciousEntropy ? "true" : "false") + ",\n";
            json += "      \"isRwx\": " + std::string(s.isRwx ? "true" : "false") + "\n";
            json += "    }" + std::string(i + 1 < sections.size() ? "," : "") + "\n";
        }
        json += "  ],\n";
        json += "  \"imports\": [\n";
        for (size_t i = 0; i < imports.size(); ++i) {
            const auto& imp = imports[i];
            json += "    {\n";
            json += "      \"dllName\": \"" + EscapeJson(imp.dllName) + "\",\n";
            json += "      \"functions\": [";
            for (size_t j = 0; j < imp.functions.size(); ++j) {
                json += "\"" + EscapeJson(imp.functions[j]) + "\"" + (j + 1 < imp.functions.size() ? ", " : "");
            }
            json += "]\n";
            json += "    }" + std::string(i + 1 < imports.size() ? "," : "") + "\n";
        }
        json += "  ],\n";
        json += "  \"technicalDetails\": [\n";
        for (size_t i = 0; i < technicalDetails.size(); ++i) {
            json += "    \"" + EscapeJson(technicalDetails[i]) + "\"" + (i + 1 < technicalDetails.size() ? "," : "") + "\n";
        }
        json += "  ],\n";
        json += "  \"logEntries\": [\n";
        for (size_t i = 0; i < logEntries.size(); ++i) {
            const auto& le = logEntries[i];
            json += "    {\n";
            json += "      \"timestamp\": \"" + EscapeJson(le.timestamp) + "\",\n";
            json += "      \"level\": \"" + EscapeJson(le.level) + "\",\n";
            json += "      \"subsystem\": \"" + EscapeJson(le.subsystem) + "\",\n";
            json += "      \"message\": \"" + EscapeJson(le.message) + "\"\n";
            json += "    }" + std::string(i + 1 < logEntries.size() ? "," : "") + "\n";
        }
        json += "  ],\n";
        json += "  \"decompiledFunctions\": [\n";
        for (size_t i = 0; i < decompiledFunctions.size(); ++i) {
            const auto& fn = decompiledFunctions[i];
            json += "    {\n";
            json += "      \"rva\": " + std::to_string(fn.rva) + ",\n";
            json += "      \"name\": \"" + EscapeJson(fn.name) + "\",\n";
            json += "      \"size\": " + std::to_string(fn.size) + ",\n";
            json += "      \"instructionCount\": " + std::to_string(fn.instructionCount) + ",\n";
            json += "      \"branchCount\": " + std::to_string(fn.branchCount) + ",\n";
            json += "      \"hasSyscall\": " + std::string(fn.hasSyscall ? "true" : "false") + ",\n";
            json += "      \"hasPebAccess\": " + std::string(fn.hasPebAccess ? "true" : "false") + ",\n";
            json += "      \"hasApiHash\": " + std::string(fn.hasApiHash ? "true" : "false") + ",\n";
            json += "      \"hasInjection\": " + std::string(fn.hasInjection ? "true" : "false") + ",\n";
            json += "      \"isEntryPoint\": " + std::string(fn.isEntryPoint ? "true" : "false") + ",\n";
            json += "      \"calledApis\": [";
            for (size_t j = 0; j < fn.calledApis.size(); ++j) {
                json += "\"" + EscapeJson(fn.calledApis[j]) + "\"" + (j + 1 < fn.calledApis.size() ? ", " : "");
            }
            json += "],\n";
            json += "      \"instructions\": [\n";
            for (size_t j = 0; j < fn.instructions.size(); ++j) {
                const auto& in = fn.instructions[j];
                json += "        {\n";
                json += "          \"rva\": " + std::to_string(in.rva) + ",\n";
                json += "          \"hex\": \"" + EscapeJson(in.hexBytes) + "\",\n";
                json += "          \"mnemonic\": \"" + EscapeJson(in.mnemonic) + "\",\n";
                json += "          \"operands\": \"" + EscapeJson(in.operands) + "\",\n";
                json += "          \"comment\": \"" + EscapeJson(in.comment) + "\"\n";
                json += "        }" + std::string(j + 1 < fn.instructions.size() ? "," : "") + "\n";
            }
            json += "      ],\n";
            json += "      \"pseudocode\": [\n";
            for (size_t j = 0; j < fn.pseudocodeLines.size(); ++j) {
                json += "        \"" + EscapeJson(fn.pseudocodeLines[j]) + "\"" + (j + 1 < fn.pseudocodeLines.size() ? "," : "") + "\n";
            }
            json += "      ]\n";
            json += "    }" + std::string(i + 1 < decompiledFunctions.size() ? "," : "") + "\n";
        }
        json += "  ]\n";
        json += "}\n";
        return json;
    }
};

} // namespace Koltzi
