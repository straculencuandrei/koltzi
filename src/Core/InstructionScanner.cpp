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
            // Skip 1 byte if invalid or padding
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

        if (isSyscall && syscallCount < 25) { // Cap at 25 findings to prevent flood
            SyscallFinding sf;
            sf.rva = currentRva;
            sf.section = sec.name;
            sf.mnemonic = mnemonicName;
            sf.instructionHex = BytesToHex(codeBytes + offset, instr.length);
            sf.disassembly = disasmBuf;
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
                std::string desc;
                std::string segName;
                uint32_t offsetVal = 0;

                if (seg == ZYDIS_REGISTER_GS && disp == 0x60) {
                    isPeb = true;
                    segName = "GS";
                    offsetVal = 0x60;
                    desc = "Direct PEB access (GS:[0x60] - x64 Process Environment Block evasion)";
                } else if (seg == ZYDIS_REGISTER_FS && disp == 0x30) {
                    isPeb = true;
                    segName = "FS";
                    offsetVal = 0x30;
                    desc = "Direct PEB access (FS:[0x30] - x86 Process Environment Block evasion)";
                }

                if (isPeb && pebCount < 20) {
                    PebAccessFinding paf;
                    paf.rva = currentRva;
                    paf.section = sec.name;
                    paf.segment = segName;
                    paf.offset = offsetVal;
                    paf.description = desc;
                    paf.disassembly = disasmBuf;
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
            instr.mnemonic == ZYDIS_MNEMONIC_LOOP || instr.mnemonic == ZYDIS_MNEMONIC_LOOPNE||
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
                ahf.description = "API Hashing Resolution Loop (Bitwise ROR/ROL with XOR in tight backward loop)";
                ahf.disassembly = disasmBuf;
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
            // Case-insensitive comparison
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
    } else if (hasWrite && hasExec) {
        report.injectionChain.detected = true;
        report.injectionChain.description =
            "Process Injection primitives detected (Remote Process Write + Remote Thread/APC Execution).";
    } else if (hasAlloc && hasWrite) {
        report.injectionChain.detected = true;
        report.injectionChain.description =
            "Suspicious Cross-Process Memory Allocation and Write primitives detected.";
    }
}

} // namespace Koltzi
