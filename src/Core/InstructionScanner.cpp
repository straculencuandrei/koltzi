#include "InstructionScanner.h"
#include <deque>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Koltzi {

InstructionScanner::InstructionScanner() {
    ZydisFormatterInit(&m_formatter, ZYDIS_FORMATTER_STYLE_INTEL);
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

    report.AddLog("ZYDIS", "PASS", "Zydis disassembly sweep complete across executable sections");

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

    // Compute total import functions count for heuristic context
    size_t totalImportFunctions = 0;
    for (const auto& imp : pe.GetImports()) {
        totalImportFunctions += imp.functions.size();
    }
    bool hasHealthyImports = (totalImportFunctions >= 10);

    struct RecentInstr {
        uint64_t rva;
        ZydisMnemonic mnemonic;
    };
    std::deque<RecentInstr> recentWindow;
    constexpr size_t MAX_WINDOW = 12;

    size_t offset = 0;
    ZydisDecodedInstruction instr;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    size_t syscallCount = 0;
    size_t pebCount = 0;
    size_t hashLoopCount = 0;

    while (offset < codeSize) {
        size_t remaining = codeSize - offset;
        uint64_t currentRva = sec.virtualAddress + offset;

        ZyanStatus status = ZydisDecoderDecodeFull(&decoder, codeBytes + offset, remaining, &instr, operands);
        if (!ZYAN_SUCCESS(status)) {
            offset++;
            continue;
        }

        // Format disassembly for findings
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
        // RULE 1: Direct Syscall Detection (0F 05 / 0F 34 / INT 2E)
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

            // Disambiguate: Browser sandboxes (Chromium / Firefox) use internal syscall stubs
            if (report.isLegitimateBrowser || (report.signature.isValid && report.signature.isTrustedVendor)) {
                sf.isLegitimateJitOrHook = true;
                report.AddLog("ZYDIS", "INFO", "Direct syscall at " + FormatRva(currentRva) + " (" + mnemonicName + ") in verified browser sandbox/JIT context");
            } else {
                sf.isLegitimateJitOrHook = false;
                report.AddLog("ZYDIS", "WARN", "Direct syscall at " + FormatRva(currentRva) + " (" + mnemonicName + ") - potential EDR hook bypass");
            }

            report.syscalls.push_back(std::move(sf));
            syscallCount++;
        }

        // ==========================================
        // RULE 2: PEB Traversal & Segment Access
        // x86: FS:[0x30] (PEB), FS:[0x18] (TEB)
        // x64: GS:[0x60] (PEB), GS:[0x30] (TEB)
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

                    // Disambiguate: Check if subsequent instructions perform Ldr walking
                    // Malware: dereferences [reg + 0x18] (Ldr) followed by [reg + 0x20]/[reg + 0x10] (ModuleList)
                    // CRT: reads GS:[0x60] into reg and uses it for security cookie (__security_init_cookie) or TLS
                    bool followsLdrWalk = false;
                    size_t lookaheadOffset = offset + instr.length;
                    size_t lookaheadCount = 0;
                    ZydisDecodedInstruction laInstr;
                    ZydisDecodedOperand laOperands[ZYDIS_MAX_OPERAND_COUNT];

                    while (lookaheadOffset < codeSize && lookaheadCount < 8) {
                        ZyanStatus laStatus = ZydisDecoderDecodeFull(&decoder, codeBytes + lookaheadOffset, codeSize - lookaheadOffset, &laInstr, laOperands);
                        if (!ZYAN_SUCCESS(laStatus)) break;

                        for (int opIdx = 0; opIdx < laInstr.operand_count_visible; ++opIdx) {
                            if (laOperands[opIdx].type == ZYDIS_OPERAND_TYPE_MEMORY && laOperands[opIdx].mem.disp.has_displacement) {
                                int64_t d = laOperands[opIdx].mem.disp.value;
                                // In x64: Ldr is offset 0x18, InMemoryOrder is 0x20 / 0x10. In x86: Ldr is 0x0C.
                                if (d == 0x18 || d == 0x20 || d == 0x10 || d == 0x0C || d == 0x14) {
                                    followsLdrWalk = true;
                                    break;
                                }
                            }
                        }
                        if (followsLdrWalk) break;
                        lookaheadOffset += laInstr.length;
                        lookaheadCount++;
                    }

                    if (!followsLdrWalk && hasHealthyImports) {
                        paf.isCrtTlsInit = true;
                        paf.description = "Benign CRT startup (" + segName + ":[0x" + (offsetVal == 0x60 ? "60" : "30") + "] - security cookie / TLS initialization)";
                        report.AddLog("ZYDIS", "INFO", "PEB access at " + FormatRva(currentRva) + " identified as benign CRT __security_init_cookie / TLS initialization");
                    } else {
                        paf.isCrtTlsInit = false;
                        paf.description = "Direct PEB traversal (" + segName + ":[0x" + (offsetVal == 0x60 ? "60" : "30") + "] - manual module/export evasion walk)";
                        report.AddLog("ZYDIS", "WARN", "PEB traversal detected at " + FormatRva(currentRva) + " (" + segName + ":[0x" + (offsetVal == 0x60 ? "60" : "30") + "])");
                    }

                    report.pebAccesses.push_back(std::move(paf));
                    pebCount++;
                    break;
                }
            }
        }

        // ==========================================
        // RULE 2 (Part B): Dynamic API Hashing Loops
        // Detects tight loops with ROR/ROL + XOR
        // ==========================================
        bool isLoopOrBranch = (
            instr.mnemonic == ZYDIS_MNEMONIC_JNZ  ||
            instr.mnemonic == ZYDIS_MNEMONIC_LOOP || instr.mnemonic == ZYDIS_MNEMONIC_LOOPNE ||
            instr.mnemonic == ZYDIS_MNEMONIC_JB   || instr.mnemonic == ZYDIS_MNEMONIC_JBE
        );

        if (isLoopOrBranch && hashLoopCount < 10) {
            bool hasRotate = false;
            bool hasXor = false;
            for (const auto& item : recentWindow) {
                if (item.mnemonic == ZYDIS_MNEMONIC_ROR || item.mnemonic == ZYDIS_MNEMONIC_ROL) {
                    hasRotate = true;
                }
                if (item.mnemonic == ZYDIS_MNEMONIC_XOR) {
                    hasXor = true;
                }
            }

            if (hasRotate && hasXor) {
                ApiHashFinding ahf;
                ahf.rva = currentRva;
                ahf.section = sec.name;
                ahf.disassembly = disasmBuf;

                // Disambiguate:
                // Standard browsers and large software have crypto ciphers (ChaCha20/Poly1305/SHA/CRC/hash tables)
                // Real malware uses API hashing to dynamically resolve functions because its IAT is stripped (< 10 imports).
                if (totalImportFunctions >= 20 || report.signature.isValid || report.isLegitimateBrowser) {
                    ahf.isLikelyCryptoOrStringHash = true;
                    ahf.description = "Cryptographic Rotation Loop (Bitwise ROR/ROL with XOR - algorithmic cipher/hash routine)";
                    report.AddLog("ZYDIS", "INFO", "Bitwise rotation loop at " + FormatRva(currentRva) + " classified as legitimate cryptographic cipher/hash routine");
                } else {
                    ahf.isLikelyCryptoOrStringHash = false;
                    ahf.description = "Evasive Dynamic API Hashing Loop (Bitwise ROR/ROL with XOR in tight loop with stripped imports)";
                    report.AddLog("ZYDIS", "WARN", "Dynamic API hashing loop detected at " + FormatRva(currentRva) + " (potential unimported API resolution)");
                }

                report.apiHashLoops.push_back(std::move(ahf));
                hashLoopCount++;
            }
        }

        // Track sliding window of recent instructions
        recentWindow.push_back({ currentRva, instr.mnemonic });
        if (recentWindow.size() > MAX_WINDOW) {
            recentWindow.pop_front();
        }

        offset += instr.length;
    }
}

void InstructionScanner::EvaluateInjectionChain(const PeReader& pe, TriageReport& report) {
    bool hasAlloc = false;
    bool hasWrite = false;
    bool hasExec = false;

    std::vector<std::string> foundApis;

    // Check imports
    for (const auto& imp : pe.GetImports()) {
        for (const auto& fn : imp.functions) {
            std::string lowerFn = fn;
            std::transform(lowerFn.begin(), lowerFn.end(), lowerFn.begin(), [](unsigned char c) {
                return (char)::tolower(c);
            });

            // Allocation primitives
            if (lowerFn == "virtualallocex" || lowerFn == "ntallocatevirtualmemory" ||
                lowerFn == "zwallocatevirtualmemory" || lowerFn == "mapviewoffile2" ||
                lowerFn == "ntmapviewofsection" || lowerFn == "zwmapviewofsection") {
                hasAlloc = true;
                foundApis.push_back(fn);
            }

            // Write primitives
            if (lowerFn == "writeprocessmemory" || lowerFn == "ntwritevirtualmemory" ||
                lowerFn == "zwwritevirtualmemory") {
                hasWrite = true;
                foundApis.push_back(fn);
            }

            // Remote execution primitives
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

    if (hasAlloc && hasWrite && hasExec) {
        report.injectionChain.detected = true;
        report.injectionChain.description =
            "Complete Process Injection Chain detected: Memory Allocation -> Remote Write -> Remote Execution!";
        report.AddLog("ZYDIS", "CRIT", report.injectionChain.description);
    } else if (hasWrite && hasExec) {
        report.injectionChain.detected = true;
        report.injectionChain.description =
            "Process Injection primitives detected (Remote Process Write + Remote Thread/APC Execution).";
        report.AddLog("ZYDIS", "CRIT", report.injectionChain.description);
    } else if (hasAlloc && hasWrite) {
        report.injectionChain.detected = true;
        report.injectionChain.description =
            "Suspicious Cross-Process Memory Allocation and Write primitives detected.";
        report.AddLog("ZYDIS", "WARN", report.injectionChain.description);
    }
}

} // namespace Koltzi
