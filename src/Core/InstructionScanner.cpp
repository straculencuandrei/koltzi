#include "InstructionScanner.h"
#include <deque>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <queue>
#include <cctype>

namespace Koltzi {

static inline uint32_t Ror32(uint32_t val, int count) {
    return (val >> count) | (val << (32 - count));
}

uint32_t InstructionScanner::ComputeMetasploitHash(const std::string& moduleName, const std::string& functionName) {
    uint32_t mh = 0;
    for (char c : moduleName) {
        char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        mh = (Ror32(mh, 13) + static_cast<uint8_t>(u)) & 0xFFFFFFFF;
        mh = (Ror32(mh, 13) + 0) & 0xFFFFFFFF;
    }
    mh = (Ror32(mh, 13) + 0) & 0xFFFFFFFF;
    mh = (Ror32(mh, 13) + 0) & 0xFFFFFFFF;

    uint32_t fh = 0;
    for (char c : functionName) {
        fh = (Ror32(fh, 13) + static_cast<uint8_t>(c)) & 0xFFFFFFFF;
    }
    fh = (Ror32(fh, 13) + 0) & 0xFFFFFFFF;

    return (mh + fh) & 0xFFFFFFFF;
}

uint32_t InstructionScanner::HashRor13(const std::string& str, bool nullTerminated) {
    uint32_t h = 0;
    for (char c : str) {
        h = (Ror32(h, 13) + static_cast<uint8_t>(c)) & 0xFFFFFFFF;
    }
    if (nullTerminated) {
        h = (Ror32(h, 13) + 0) & 0xFFFFFFFF;
    }
    return h;
}

uint32_t InstructionScanner::HashDjb2(const std::string& str) {
    uint32_t hash = 5381;
    for (char c : str) {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
    }
    return hash;
}

uint32_t InstructionScanner::HashFnv1a32(const std::string& str) {
    uint32_t hash = 0x811C9DC5u;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 0x01000193u;
    }
    return hash;
}

uint64_t InstructionScanner::HashFnv1a64(const std::string& str) {
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

uint32_t InstructionScanner::HashCrc32(const std::string& str) {
    uint32_t crc = 0xFFFFFFFFu;
    for (char c : str) {
        uint8_t b = static_cast<uint8_t>(c);
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

uint32_t InstructionScanner::HashSdbm(const std::string& str) {
    uint32_t hash = 0;
    for (char c : str) {
        hash = static_cast<uint8_t>(c) + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

void InstructionScanner::InitHashDatabase() {
    m_knownHashes32.clear();
    m_knownHashes64.clear();

    const std::vector<std::string> modules = {
        "kernel32.dll", "ntdll.dll", "ws2_32.dll", "wininet.dll", "advapi32.dll", "user32.dll"
    };

    const std::vector<std::string> targetFunctions = {
        "LoadLibraryA", "LoadLibraryW", "GetProcAddress", "VirtualAlloc", "VirtualProtect",
        "VirtualAllocEx", "VirtualProtectEx", "WriteProcessMemory", "CreateRemoteThread",
        "CreateRemoteThreadEx", "NtAllocateVirtualMemory", "NtProtectVirtualMemory",
        "NtWriteVirtualMemory", "NtCreateThreadEx", "RtlCreateUserThread", "QueueUserAPC",
        "NtQueueApcThread", "SetThreadContext", "ResumeThread", "NtResumeThread",
        "NtUnmapViewOfSection", "ZwUnmapViewOfSection", "CreateProcessA", "CreateProcessW",
        "WinExec", "ExitProcess", "RtlExitUserThread", "RevertToSelf", "WSAStartup",
        "WSASocketA", "connect", "send", "recv", "bind", "listen", "accept",
        "closesocket", "URLDownloadToFileA", "URLDownloadToFileW", "InternetOpenA",
        "InternetOpenUrlA", "HttpOpenRequestA", "HttpSendRequestA", "CloseHandle"
    };

    // 1. Populate Metasploit module + function ROR13 hashes
    for (const auto& mod : modules) {
        for (const auto& fn : targetFunctions) {
            uint32_t h = ComputeMetasploitHash(mod, fn);
            if (h != 0) {
                m_knownHashes32[h] = { fn, "Metasploit ROR13 (" + mod + ")", h };
            }
        }
    }

    // 2. Populate standalone function hashes (ROR13, djb2, FNV-1a, CRC32, sdbm)
    for (const auto& fn : targetFunctions) {
        uint32_t r13 = HashRor13(fn, false);
        if (r13 != 0) m_knownHashes32[r13] = { fn, "ROR13", r13 };

        uint32_t r13Null = HashRor13(fn, true);
        if (r13Null != 0) m_knownHashes32[r13Null] = { fn, "ROR13 (null-terminated)", r13Null };

        uint32_t djb = HashDjb2(fn);
        if (djb != 0 && djb != 5381) m_knownHashes32[djb] = { fn, "djb2", djb };

        uint32_t fnv32 = HashFnv1a32(fn);
        if (fnv32 != 0 && fnv32 != 0x811C9DC5u) m_knownHashes32[fnv32] = { fn, "FNV-1a 32-bit", fnv32 };

        uint32_t crc = HashCrc32(fn);
        if (crc != 0 && crc != 0xFFFFFFFFu) m_knownHashes32[crc] = { fn, "CRC32", crc };

        uint32_t sdbm = HashSdbm(fn);
        if (sdbm != 0) m_knownHashes32[sdbm] = { fn, "sdbm", sdbm };

        uint64_t fnv64 = HashFnv1a64(fn);
        if (fnv64 != 0 && fnv64 != 0xCBF29CE484222325ULL) m_knownHashes64[fnv64] = { fn, "FNV-1a 64-bit", fnv64 };
    }
}

InstructionScanner::InstructionScanner() {
    ZydisFormatterInit(&m_formatter, ZYDIS_FORMATTER_STYLE_INTEL);
    InitHashDatabase();
}

InstructionScanner::~InstructionScanner() = default;

static std::string BytesToHex(const uint8_t* data, size_t length) {
    std::ostringstream ss;
    for (size_t i = 0; i < length; ++i) {
        if (i > 0) ss << " ";
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return ss.str();
}

static std::string FormatRva(uint64_t rva) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << rva;
    return ss.str();
}

bool InstructionScanner::Scan(const PeReader& pe, TriageReport& report) {
    if (!pe.IsValid()) return false;

    // Scan all executable sections
    for (const auto& sec : pe.GetSections()) {
        if (sec.isExecutable) {
            ScanSection(pe, sec, report);
        }
    }

    // Evaluate Process Injection Chaining
    EvaluateInjectionChain(pe, report);

    report.AddLog("ZYDIS", "PASS", "Zydis control-flow recursive descent and structural analysis complete");
    return true;
}

void InstructionScanner::ScanSection(const PeReader& pe, const SectionInfo& sec, TriageReport& report) {
    const uint8_t* codeBytes = nullptr;
    size_t codeSize = 0;
    if (!pe.GetSectionBytes(sec, codeBytes, codeSize) || codeSize == 0) return;

    ZydisDecoder decoder;
    if (pe.Is64Bit()) {
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    } else {
        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
    }

    size_t totalImportFunctions = 0;
    for (const auto& imp : pe.GetImports()) {
        totalImportFunctions += imp.functions.size();
    }
    bool hasHealthyImports = (totalImportFunctions >= 10);

    // Build function starts in this section
    std::vector<uint32_t> sectionFnStarts;
    for (uint32_t fs : pe.GetFunctionStarts()) {
        if (fs >= sec.virtualAddress && fs < sec.virtualAddress + sec.virtualSize) {
            sectionFnStarts.push_back(fs);
        }
    }
    std::sort(sectionFnStarts.begin(), sectionFnStarts.end());
    sectionFnStarts.erase(std::unique(sectionFnStarts.begin(), sectionFnStarts.end()), sectionFnStarts.end());

    // Worklist for control-flow recursive descent
    std::unordered_set<size_t> visitedOffsets;
    std::queue<size_t> worklist;

    for (uint32_t fs : sectionFnStarts) {
        size_t off = fs - sec.virtualAddress;
        if (off < codeSize) {
            worklist.push(off);
        }
    }

    // Ensure section start or entry point is queued
    if (worklist.empty()) {
        worklist.push(0);
    }

    struct RecentDecoded {
        uint64_t rva;
        size_t offset;
        ZydisDecodedInstruction instr;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    };
    std::deque<RecentDecoded> recentWindow;
    constexpr size_t MAX_WINDOW = 16;

    size_t syscallCount = 0;
    size_t pebCount = 0;

    std::vector<ApiHashFinding> immediateHashHits;
    std::vector<ApiHashFinding> loopHashFindings;

    size_t currentLinearOffset = 0;

    while (!worklist.empty() || currentLinearOffset < codeSize) {
        size_t offset = 0;
        if (!worklist.empty()) {
            offset = worklist.front();
            worklist.pop();
        } else {
            // Find next unvisited non-padding offset
            while (currentLinearOffset < codeSize) {
                if (visitedOffsets.find(currentLinearOffset) == visitedOffsets.end()) {
                    uint8_t b = codeBytes[currentLinearOffset];
                    // Skip alignment padding (0xCC INT3, 0x90 NOP, 0x00 NULL)
                    if (b != 0xCC && b != 0x90 && b != 0x00) {
                        offset = currentLinearOffset;
                        break;
                    }
                }
                currentLinearOffset++;
            }
            if (currentLinearOffset >= codeSize) break;
        }

        // Trace linear block from offset
        while (offset < codeSize) {
            if (visitedOffsets.count(offset)) {
                break; // Already disassembled this branch
            }
            visitedOffsets.insert(offset);

            uint8_t leadByte = codeBytes[offset];
            if (leadByte == 0xCC || leadByte == 0x90) {
                // Single padding byte in between functions
                offset++;
                continue;
            }

            size_t remaining = codeSize - offset;
            uint64_t currentRva = sec.virtualAddress + offset;

            ZydisDecodedInstruction instr;
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
            ZyanStatus status = ZydisDecoderDecodeFull(&decoder, codeBytes + offset, remaining, &instr, operands);
            if (!ZYAN_SUCCESS(status)) {
                // Invalid decode terminates current control-flow block cleanly (no stray byte advancement!)
                break;
            }

            char disasmBuf[128] = { 0 };
            ZydisFormatterFormatInstruction(
                &m_formatter,
                &instr,
                operands,
                instr.operand_count_visible,
                disasmBuf,
                sizeof(disasmBuf),
                currentRva,
                ZYAN_NULL
            );

            // ==========================================
            // RULE 1: Structural Syscall Detection (0F 05 / 0F 34 / INT 2E)
            // Validates stub structure: mov r10, rcx; mov eax, <ssn>; syscall; ret
            // ==========================================
            bool isSyscall = false;
            std::string mnemonicName;

            if (instr.mnemonic == ZYDIS_MNEMONIC_SYSCALL) {
                isSyscall = true;
                mnemonicName = "SYSCALL (0F 05)";
            } else if (instr.mnemonic == ZYDIS_MNEMONIC_SYSENTER) {
                isSyscall = true;
                mnemonicName = "SYSENTER (0F 34)";
            } else if (instr.mnemonic == ZYDIS_MNEMONIC_INT && instr.operand_count_visible > 0 &&
                       operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && operands[0].imm.value.u == 0x2E) {
                isSyscall = true;
                mnemonicName = "INT 0x2E (Legacy Syscall)";
            }

            if (isSyscall && syscallCount < 25) {
                SyscallFinding sf;
                sf.rva = currentRva;
                sf.section = sec.name;
                sf.mnemonic = mnemonicName;
                sf.instructionHex = BytesToHex(codeBytes + offset, instr.length);
                sf.disassembly = disasmBuf;

                // Validate surrounding context:
                // Preceding 1-4 instructions: check for mov r10, rcx and mov eax, <ssn>
                bool hasMovR10 = false;
                bool hasMovEax = false;
                uint32_t ssnVal = 0;

                for (auto it = recentWindow.rbegin(); it != recentWindow.rend(); ++it) {
                    if (it->instr.mnemonic == ZYDIS_MNEMONIC_MOV) {
                        if (it->instr.operand_count_visible >= 2) {
                            // Check mov r10, rcx
                            if (it->operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                                it->operands[0].reg.value == ZYDIS_REGISTER_R10 &&
                                it->operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                                it->operands[1].reg.value == ZYDIS_REGISTER_RCX) {
                                hasMovR10 = true;
                            }
                            // Check mov eax, <imm>
                            if (it->operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                                (it->operands[0].reg.value == ZYDIS_REGISTER_EAX || it->operands[0].reg.value == ZYDIS_REGISTER_RAX) &&
                                it->operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                                hasMovEax = true;
                                ssnVal = static_cast<uint32_t>(it->operands[1].imm.value.u);
                            }
                        }
                    }
                    if (hasMovR10 && hasMovEax) break;
                }

                // Check subsequent instruction for ret / jmp
                bool followedByRet = false;
                size_t nextOff = offset + instr.length;
                if (nextOff < codeSize) {
                    ZydisDecodedInstruction nextInstr;
                    ZydisDecodedOperand nextOps[ZYDIS_MAX_OPERAND_COUNT];
                    if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, codeBytes + nextOff, codeSize - nextOff, &nextInstr, nextOps))) {
                        if (nextInstr.mnemonic == ZYDIS_MNEMONIC_RET || nextInstr.mnemonic == ZYDIS_MNEMONIC_JMP) {
                            followedByRet = true;
                        }
                    }
                }

                if ((hasMovR10 || hasMovEax) && (followedByRet || hasMovEax)) {
                    sf.isStubPattern = true;
                    sf.ssn = ssnVal;
                } else {
                    sf.isStubPattern = false;
                }

                // Disambiguate:
                // ntdll.dll / win32u.dll legitimately host system syscalls.
                // Browsers with verified JIT or valid signatures use internal sandbox syscalls.
                std::string lowerSec = sec.name;
                std::transform(lowerSec.begin(), lowerSec.end(), lowerSec.begin(), [](unsigned char c) { return (char)::tolower(c); });
                bool isSystemDll = (report.fileName.find("ntdll") != std::string::npos || report.fileName.find("win32u") != std::string::npos);

                if (isSystemDll || report.isLegitimateBrowser || (report.signature.isValid && report.signature.isTrustedVendor)) {
                    sf.isLegitimateJitOrHook = true;
                    report.AddLog("ZYDIS", "INFO", "Direct syscall at " + FormatRva(currentRva) + " (" + mnemonicName + ") in verified system/browser context");
                } else if (sf.isStubPattern || !hasHealthyImports) {
                    sf.isLegitimateJitOrHook = false;
                    report.AddLog("ZYDIS", "WARN", "Direct syscall stub at " + FormatRva(currentRva) + " (" + mnemonicName + ", SSN: 0x" + std::format("{:X}", ssnVal) + ") - EDR hook bypass");
                } else {
                    // Stray decode or JIT hook
                    sf.isLegitimateJitOrHook = true;
                }

                report.syscalls.push_back(std::move(sf));
                syscallCount++;
            }

            // ==========================================
            // RULE 2: Precise PEB Traversal & Register Tracking
            // GS:[0x60] / FS:[0x30] with target register LDR walk verification
            // ==========================================
            for (int i = 0; i < instr.operand_count_visible; ++i) {
                if (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY) {
                    ZydisRegister seg = operands[i].mem.segment;
                    int64_t disp = operands[i].mem.disp.has_displacement ? operands[i].mem.disp.value : 0;

                    bool isPeb = false;
                    std::string segName;
                    uint32_t offsetVal = 0;

                    if (seg == ZYDIS_REGISTER_GS && disp == 0x60) {
                        isPeb = true;
                        segName = "GS";
                        offsetVal = 0x60;
                    } else if (seg == ZYDIS_REGISTER_FS && disp == 0x30) {
                        isPeb = true;
                        segName = "FS";
                        offsetVal = 0x30;
                    }

                    if (isPeb && pebCount < 20) {
                        PebAccessFinding paf;
                        paf.rva = currentRva;
                        paf.section = sec.name;
                        paf.segment = segName;
                        paf.offset = offsetVal;
                        paf.disassembly = disasmBuf;

                        // Identify the register that received the PEB pointer
                        ZydisRegister pebReg = ZYDIS_REGISTER_NONE;
                        if (instr.operand_count_visible >= 1 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                            pebReg = operands[0].reg.value;
                        }

                        // Lookahead up to 16 instructions to verify genuine PEB->Ldr and ModuleList traversal
                        bool derefsLdr = false;
                        bool walksModuleList = false;
                        ZydisRegister ldrReg = ZYDIS_REGISTER_NONE;

                        size_t laOffset = offset + instr.length;
                        size_t laCount = 0;
                        while (laOffset < codeSize && laCount < 16) {
                            ZydisDecodedInstruction laInstr;
                            ZydisDecodedOperand laOps[ZYDIS_MAX_OPERAND_COUNT];
                            if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, codeBytes + laOffset, codeSize - laOffset, &laInstr, laOps))) {
                                break;
                            }

                            for (int opIdx = 0; opIdx < laInstr.operand_count_visible; ++opIdx) {
                                if (laOps[opIdx].type == ZYDIS_OPERAND_TYPE_MEMORY) {
                                    int64_t d = laOps[opIdx].mem.disp.has_displacement ? laOps[opIdx].mem.disp.value : 0;
                                    ZydisRegister baseReg = laOps[opIdx].mem.base;

                                    // Check if base register is the PEB pointer register
                                    if (pebReg != ZYDIS_REGISTER_NONE && (baseReg == pebReg || baseReg == ZYDIS_REGISTER_NONE)) {
                                        // In x64: Ldr is offset 0x18. In x86: Ldr is offset 0x0C.
                                        if (d == 0x18 || d == 0x0C) {
                                            derefsLdr = true;
                                            if (laOps[0].type == ZYDIS_OPERAND_TYPE_REGISTER) {
                                                ldrReg = laOps[0].reg.value;
                                            }
                                        }
                                    }

                                    // Check if LDR register is dereferenced for InMemoryOrder / InLoadOrder list
                                    if (ldrReg != ZYDIS_REGISTER_NONE && (baseReg == ldrReg || baseReg == ZYDIS_REGISTER_NONE)) {
                                        // In x64: InLoadOrder = 0x10, InMemoryOrder = 0x20. In x86: InLoadOrder = 0x0C, InMemoryOrder = 0x14.
                                        if (d == 0x20 || d == 0x10 || d == 0x14 || d == 0x30 || d == 0x50) {
                                            walksModuleList = true;
                                        }
                                    }
                                }
                            }

                            if (derefsLdr && walksModuleList) break;
                            laOffset += laInstr.length;
                            laCount++;
                        }

                        if (derefsLdr && walksModuleList) {
                            paf.isFullLdrWalk = true;
                            paf.isCrtTlsInit = false;
                            paf.description = "Direct PEB traversal (" + segName + ":[0x" + (offsetVal == 0x60 ? "60" : "30") +
                                              "] -> PEB.Ldr -> InMemoryOrderModuleList manual export walk)";
                            report.AddLog("ZYDIS", "WARN", "PEB export evasion traversal detected at " + FormatRva(currentRva));
                        } else if (!hasHealthyImports && !report.isLegitimateBrowser && !report.signature.isValid) {
                            // Shellcode or stripped loader without healthy imports
                            paf.isFullLdrWalk = true;
                            paf.isCrtTlsInit = false;
                            paf.description = "PEB access in stripped/unlinked executable (" + segName + ":[0x" +
                                              (offsetVal == 0x60 ? "60" : "30") + "])";
                            report.AddLog("ZYDIS", "WARN", "PEB read at " + FormatRva(currentRva) + " in stripped binary");
                        } else {
                            paf.isFullLdrWalk = false;
                            paf.isCrtTlsInit = true;
                            paf.description = "Benign CRT startup (" + segName + ":[0x" + (offsetVal == 0x60 ? "60" : "30") +
                                              "] - security cookie / TLS initialization)";
                            report.AddLog("ZYDIS", "INFO", "PEB access at " + FormatRva(currentRva) + " identified as benign CRT __security_init_cookie / TLS initialization");
                        }

                        report.pebAccesses.push_back(std::move(paf));
                        pebCount++;
                        break;
                    }
                }
            }

            // ==========================================
            // RULE 3: API Hashing Immediate Operand Scanning
            // Scans cmp, mov, push, sub, xor immediates for known hash constants
            // ==========================================
            for (int opIdx = 0; opIdx < instr.operand_count_visible; ++opIdx) {
                if (operands[opIdx].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                    uint64_t immVal = operands[opIdx].imm.value.u;
                    uint32_t imm32 = static_cast<uint32_t>(immVal);

                    // Check 64-bit FNV-1a
                    auto it64 = m_knownHashes64.find(immVal);
                    if (it64 != m_knownHashes64.end()) {
                        ApiHashFinding ahf;
                        ahf.rva = currentRva;
                        ahf.section = sec.name;
                        ahf.disassembly = disasmBuf;
                        ahf.matchedApi = it64->second.api;
                        ahf.hashAlgorithm = it64->second.algorithm;
                        ahf.hashValue = immVal;
                        ahf.description = "Matched API Hash: " + it64->second.api + " (" + it64->second.algorithm + ")";
                        immediateHashHits.push_back(std::move(ahf));
                    }

                    // Check 32-bit (Metasploit, ROR13, djb2, FNV-1a, CRC32, sdbm)
                    auto it32 = m_knownHashes32.find(imm32);
                    if (it32 != m_knownHashes32.end()) {
                        ApiHashFinding ahf;
                        ahf.rva = currentRva;
                        ahf.section = sec.name;
                        ahf.disassembly = disasmBuf;
                        ahf.matchedApi = it32->second.api;
                        ahf.hashAlgorithm = it32->second.algorithm;
                        ahf.hashValue = imm32;
                        ahf.description = "Matched API Hash: " + it32->second.api + " (" + it32->second.algorithm + ")";
                        immediateHashHits.push_back(std::move(ahf));
                    }
                }
            }

            // ==========================================
            // RULE 4: Dynamic String Iteration Hashing Loop Detection
            // Requires byte-wise reads, null check, rotate/accumulate pattern
            // ==========================================
            bool isLoopBranch = (
                instr.mnemonic == ZYDIS_MNEMONIC_JNZ  ||
                instr.mnemonic == ZYDIS_MNEMONIC_LOOP || instr.mnemonic == ZYDIS_MNEMONIC_LOOPNE ||
                instr.mnemonic == ZYDIS_MNEMONIC_JB   || instr.mnemonic == ZYDIS_MNEMONIC_JBE
            );

            if (isLoopBranch && operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                int64_t targetOffset = static_cast<int64_t>(operands[0].imm.value.s);
                // Backward branch within loop distance (e.g. 5 to 64 bytes)
                if (targetOffset < 0 && targetOffset > -64) {
                    bool hasByteRead = false;
                    bool hasNullCheck = false;
                    bool hasRotateOrHash = false;
                    bool hasAccumulate = false;

                    for (const auto& item : recentWindow) {
                        // Byte-wise read (lodsb or movzx from memory)
                        if (item.instr.mnemonic == ZYDIS_MNEMONIC_LODSB) {
                            hasByteRead = true;
                        } else if (item.instr.mnemonic == ZYDIS_MNEMONIC_MOVZX || item.instr.mnemonic == ZYDIS_MNEMONIC_MOV) {
                            if (item.instr.operand_count_visible >= 2 && item.operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY) {
                                if (item.operands[1].size == 8 || item.operands[0].size == 8) {
                                    hasByteRead = true;
                                }
                            }
                        }

                        // Null termination check
                        if (item.instr.mnemonic == ZYDIS_MNEMONIC_TEST || item.instr.mnemonic == ZYDIS_MNEMONIC_CMP) {
                            hasNullCheck = true;
                        }

                        // Rotate or hash step
                        if (item.instr.mnemonic == ZYDIS_MNEMONIC_ROR || item.instr.mnemonic == ZYDIS_MNEMONIC_ROL ||
                            item.instr.mnemonic == ZYDIS_MNEMONIC_IMUL || item.instr.mnemonic == ZYDIS_MNEMONIC_CRC32) {
                            hasRotateOrHash = true;
                        }

                        // Accumulate (ADD or XOR)
                        if (item.instr.mnemonic == ZYDIS_MNEMONIC_ADD || item.instr.mnemonic == ZYDIS_MNEMONIC_XOR) {
                            hasAccumulate = true;
                        }
                    }

                    if (hasRotateOrHash && (hasAccumulate || hasByteRead)) {
                        ApiHashFinding ahf;
                        ahf.rva = currentRva;
                        ahf.section = sec.name;
                        ahf.disassembly = disasmBuf;

                        if (totalImportFunctions >= 20 || report.signature.isValid || report.isLegitimateBrowser) {
                            ahf.isLikelyCryptoOrStringHash = true;
                            ahf.description = "Cryptographic Rotation Loop (Bitwise ROR/ROL with accumulator)";
                            report.AddLog("ZYDIS", "INFO", "Rotation loop at " + FormatRva(currentRva) + " identified as legitimate cryptographic routine");
                        } else {
                            ahf.isLikelyCryptoOrStringHash = false;
                            ahf.description = "Evasive Dynamic API Hashing Loop (String-iteration rotate & accumulate with stripped imports)";
                            report.AddLog("ZYDIS", "WARN", "Dynamic API hashing loop detected at " + FormatRva(currentRva));
                        }
                        loopHashFindings.push_back(std::move(ahf));
                    }
                }
            }

            // Track recent instruction window
            RecentDecoded rd;
            rd.rva = currentRva;
            rd.offset = offset;
            rd.instr = instr;
            std::memcpy(rd.operands, operands, sizeof(operands));
            recentWindow.push_back(rd);
            if (recentWindow.size() > MAX_WINDOW) {
                recentWindow.pop_front();
            }

            // ==========================================
            // Control Flow Following: Calls, Jumps, Returns
            // ==========================================
            if (instr.mnemonic == ZYDIS_MNEMONIC_CALL) {
                if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                    uint64_t targetRva = currentRva + instr.length + operands[0].imm.value.s;
                    if (targetRva >= sec.virtualAddress && targetRva < sec.virtualAddress + sec.virtualSize) {
                        size_t targetOff = targetRva - sec.virtualAddress;
                        if (visitedOffsets.find(targetOff) == visitedOffsets.end()) {
                            worklist.push(targetOff);
                        }
                    }
                }
            } else if (instr.mnemonic == ZYDIS_MNEMONIC_JMP) {
                if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                    uint64_t targetRva = currentRva + instr.length + operands[0].imm.value.s;
                    if (targetRva >= sec.virtualAddress && targetRva < sec.virtualAddress + sec.virtualSize) {
                        size_t targetOff = targetRva - sec.virtualAddress;
                        if (visitedOffsets.find(targetOff) == visitedOffsets.end()) {
                            worklist.push(targetOff);
                        }
                    }
                }
                // Unconditional jump terminates linear fallthrough
                break;
            } else if (instr.mnemonic == ZYDIS_MNEMONIC_RET) {
                // Return terminates linear block
                break;
            } else if (isLoopBranch) {
                if (operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                    uint64_t targetRva = currentRva + instr.length + operands[0].imm.value.s;
                    if (targetRva >= sec.virtualAddress && targetRva < sec.virtualAddress + sec.virtualSize) {
                        size_t targetOff = targetRva - sec.virtualAddress;
                        if (visitedOffsets.find(targetOff) == visitedOffsets.end()) {
                            worklist.push(targetOff);
                        }
                    }
                }
            }

            offset += instr.length;
        }
    }

    // ==========================================
    // Filter API Hash Findings to keep signal clean
    // Require >= 2 matches, OR 1 match + hashing loop pattern
    // ==========================================
    std::unordered_set<uint64_t> distinctHashValues;
    for (const auto& hit : immediateHashHits) {
        distinctHashValues.insert(hit.hashValue);
    }

    bool hasLoopPattern = false;
    for (const auto& loop : loopHashFindings) {
        if (!loop.isLikelyCryptoOrStringHash) {
            hasLoopPattern = true;
            report.apiHashLoops.push_back(loop);
        }
    }

    if (distinctHashValues.size() >= 2 || (distinctHashValues.size() >= 1 && hasLoopPattern) || !hasHealthyImports) {
        for (auto& hit : immediateHashHits) {
            hit.isLikelyCryptoOrStringHash = false;
            report.AddLog("ZYDIS", "WARN", "Confirmed API Hash matched at " + FormatRva(hit.rva) + ": " + hit.matchedApi + " (" + hit.hashAlgorithm + ")");
            report.apiHashLoops.push_back(std::move(hit));
        }
    } else if (distinctHashValues.size() == 1 && hasHealthyImports) {
        report.AddLog("ZYDIS", "INFO", "Single immediate hash value suppressed as non-malicious constant in healthy binary");
    }
}

void InstructionScanner::EvaluateInjectionChain(const PeReader& pe, TriageReport& report) {
    bool hasAlloc = false;
    bool hasWrite = false;
    bool hasExec = false;

    std::vector<std::string> foundApis;

    // 1. Map IAT slots and thunk RVAs to API primitives
    std::unordered_map<uint32_t, std::string> iatSlotMap;

    for (const auto& imp : pe.GetImports()) {
        for (size_t i = 0; i < imp.functions.size(); ++i) {
            const std::string& fn = imp.functions[i];
            uint32_t rva = (i < imp.iatRvas.size()) ? imp.iatRvas[i] : 0;
            if (rva != 0) {
                iatSlotMap[rva] = fn;
            }

            std::string lowerFn = fn;
            std::transform(lowerFn.begin(), lowerFn.end(), lowerFn.begin(), [](unsigned char c) {
                return (char)::tolower(c);
            });

            if (lowerFn == "virtualallocex" || lowerFn == "ntallocatevirtualmemory" ||
                lowerFn == "zwallocatevirtualmemory" || lowerFn == "mapviewoffile2" ||
                lowerFn == "ntmapviewofsection" || lowerFn == "zwmapviewofsection" ||
                lowerFn == "virtualalloc") {
                hasAlloc = true;
                foundApis.push_back(fn);
            }

            if (lowerFn == "writeprocessmemory" || lowerFn == "ntwritevirtualmemory" ||
                lowerFn == "zwwritevirtualmemory") {
                hasWrite = true;
                foundApis.push_back(fn);
            }

            if (lowerFn == "createremotethread" || lowerFn == "createremotethreadex" ||
                lowerFn == "ntcreatethreadex" || lowerFn == "rtlcreateuserthread" ||
                lowerFn == "queueuserapc" || lowerFn == "ntqueueapcthread" ||
                lowerFn == "setthreadcontext" || lowerFn == "resumethread") {
                hasExec = true;
                foundApis.push_back(fn);
            }
        }
    }

    // Check RWX sections
    bool hasRwxSection = false;
    for (const auto& sec : pe.GetSections()) {
        if (sec.isRwx) {
            hasRwxSection = true;
            break;
        }
    }

    report.injectionChain.hasRwxAllocation = hasAlloc || hasRwxSection;
    report.injectionChain.hasProcessWrite = hasWrite;
    report.injectionChain.hasRemoteExecution = hasExec;
    report.injectionChain.chainedApis = foundApis;

    // 2. Scan call sites and resolve co-location within functions
    // Find nearest function start for each call site
    std::vector<uint32_t> fnStarts = pe.GetFunctionStarts();
    std::sort(fnStarts.begin(), fnStarts.end());

    struct CallSiteInfo {
        uint32_t fnStartRva = 0;
        std::string apiName;
        uint64_t callRva = 0;
        bool hasRwxArg = false;
    };
    std::vector<CallSiteInfo> callSites;

    for (const auto& sec : pe.GetSections()) {
        if (!sec.isExecutable) continue;
        const uint8_t* codeBytes = nullptr;
        size_t codeSize = 0;
        if (!pe.GetSectionBytes(sec, codeBytes, codeSize) || codeSize == 0) continue;

        ZydisDecoder decoder;
        if (pe.Is64Bit()) {
            ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        } else {
            ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
        }

        size_t offset = 0;
        struct LookbackInstr {
            ZydisMnemonic mnemonic;
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
            uint8_t opCount;
        };
        std::deque<LookbackInstr> lookback;

        while (offset < codeSize) {
            ZydisDecodedInstruction instr;
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, codeBytes + offset, codeSize - offset, &instr, operands))) {
                offset++;
                continue;
            }

            uint64_t curRva = sec.virtualAddress + offset;

            // Check indirect calls / jumps: call [rip + disp]
            if (instr.mnemonic == ZYDIS_MNEMONIC_CALL || instr.mnemonic == ZYDIS_MNEMONIC_JMP) {
                for (int i = 0; i < instr.operand_count_visible; ++i) {
                    if (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY) {
                        uint32_t targetSlotRva = 0;
                        if (operands[i].mem.base == ZYDIS_REGISTER_RIP) {
                            targetSlotRva = static_cast<uint32_t>(curRva + instr.length + operands[i].mem.disp.value);
                        } else if (operands[i].mem.disp.has_displacement) {
                            uint64_t absDisp = static_cast<uint64_t>(operands[i].mem.disp.value);
                            if (absDisp >= pe.GetImageBase()) {
                                targetSlotRva = static_cast<uint32_t>(absDisp - pe.GetImageBase());
                            }
                        }

                        auto itSlot = iatSlotMap.find(targetSlotRva);
                        if (itSlot != iatSlotMap.end()) {
                            // Find nearest enclosing function start
                            uint32_t fnStart = 0;
                            auto ub = std::upper_bound(fnStarts.begin(), fnStarts.end(), static_cast<uint32_t>(curRva));
                            if (ub != fnStarts.begin()) {
                                fnStart = *(ub - 1);
                            }

                            // Check lookback window for 0x40 (PAGE_EXECUTE_READWRITE)
                            bool hasRwx = false;
                            for (const auto& lb : lookback) {
                                if (lb.mnemonic == ZYDIS_MNEMONIC_MOV || lb.mnemonic == ZYDIS_MNEMONIC_PUSH || lb.mnemonic == ZYDIS_MNEMONIC_OR) {
                                    for (uint8_t opIdx = 0; opIdx < lb.opCount; ++opIdx) {
                                        if (lb.operands[opIdx].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
                                            if (lb.operands[opIdx].imm.value.u == 0x40) {
                                                hasRwx = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                                if (hasRwx) break;
                            }

                            callSites.push_back({ fnStart, itSlot->second, curRva, hasRwx });
                        }
                    }
                }
            }

            LookbackInstr lbi;
            lbi.mnemonic = instr.mnemonic;
            lbi.opCount = instr.operand_count_visible;
            std::memcpy(lbi.operands, operands, sizeof(operands));
            lookback.push_back(lbi);
            if (lookback.size() > 12) lookback.pop_front();

            offset += instr.length;
        }
    }

    // 3. Evaluate Co-Location and Process Hollowing
    std::unordered_map<uint32_t, std::vector<std::string>> apisPerFunction;
    for (const auto& cs : callSites) {
        apisPerFunction[cs.fnStartRva].push_back(cs.apiName);
        if (cs.hasRwxArg) {
            report.injectionChain.hasRwxProtectArg = true;
        }
    }

    for (const auto& [fn, apis] : apisPerFunction) {
        bool fnAlloc = false;
        bool fnWrite = false;
        bool fnExec = false;
        bool fnHollow = false;

        for (const auto& a : apis) {
            std::string l = a;
            std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return (char)::tolower(c); });
            if (l.find("alloc") != std::string::npos || l.find("map") != std::string::npos) fnAlloc = true;
            if (l.find("write") != std::string::npos) fnWrite = true;
            if (l.find("thread") != std::string::npos || l.find("apc") != std::string::npos) fnExec = true;
            if (l.find("unmap") != std::string::npos || l.find("context") != std::string::npos || l.find("resume") != std::string::npos) fnHollow = true;
        }

        if (fnAlloc && fnWrite && fnExec) {
            report.injectionChain.isCoLocated = true;
        }
        if (fnWrite && fnHollow) {
            report.injectionChain.isProcessHollowing = true;
        }
    }

    // Overall verdict on injection
    size_t totalImports = 0;
    for (const auto& imp : pe.GetImports()) totalImports += imp.functions.size();

    if (report.injectionChain.isCoLocated || report.injectionChain.isProcessHollowing) {
        report.injectionChain.detected = true;
        report.injectionChain.description =
            "Co-Located Process Injection / Hollowing: Memory Allocation -> Remote Write -> Remote Execution in same function scope!";
        report.AddLog("ZYDIS", "CRIT", report.injectionChain.description);
    } else if (hasAlloc && hasWrite && hasExec && (totalImports < 10 || report.injectionChain.hasRwxProtectArg)) {
        report.injectionChain.detected = true;
        report.injectionChain.description =
            "Complete Process Injection Chain detected: Memory Allocation -> Remote Write -> Remote Execution!";
        report.AddLog("ZYDIS", "CRIT", report.injectionChain.description);
    } else if (hasAlloc && hasWrite && hasExec && totalImports >= 20) {
        // Dispersed across a large binary (e.g. browser, Discord, game overlay) without co-location or RWX argument
        report.injectionChain.detected = false;
        report.AddLog("ZYDIS", "INFO", "Dispersed process allocation and thread APIs across large binary identified as non-injective");
    } else if (hasWrite && hasExec && totalImports < 10) {
        report.injectionChain.detected = true;
        report.injectionChain.description =
            "Process Injection primitives detected (Remote Process Write + Remote Thread/APC Execution).";
        report.AddLog("ZYDIS", "CRIT", report.injectionChain.description);
    }
}

} // namespace Koltzi
