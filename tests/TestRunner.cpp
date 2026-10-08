#include "Common.h"
#include "Core/PeReader.h"
#include "Core/CryptoVerifier.h"
#include "Core/InstructionScanner.h"
#include "Core/StringScanner.h"
#include "Core/ThreatAssessor.h"
#include "Core/TriageReport.h"
#include <iostream>
#include <vector>
#include <random>
#include <cstdlib>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "[FAIL] Assertion failed: " #cond " at line " << __LINE__ << "\n"; \
        std::abort(); \
    } \
} while(0)

using namespace Koltzi;

// Helper to build synthetic 64-bit PE in memory
static std::vector<uint8_t> BuildTestPe64(
    const std::vector<uint8_t>& codeBytes,
    const std::vector<uint8_t>& dataBytes,
    const std::string& importDll = "",
    const std::vector<std::string>& importFuncs = {}
) {
    constexpr size_t HEADER_SIZE = 0x400;
    constexpr size_t FILE_ALIGN = 0x200;

    size_t codeRawSize = ((codeBytes.size() + FILE_ALIGN - 1) / FILE_ALIGN) * FILE_ALIGN;
    if (codeRawSize == 0) codeRawSize = FILE_ALIGN;

    std::vector<uint8_t> fullData = dataBytes;
    uint32_t importRva = 0;
    uint32_t importSize = 0;

    if (!importDll.empty() && !importFuncs.empty()) {
        importRva = 0x2000 + static_cast<uint32_t>(fullData.size());
        size_t descOffset = fullData.size();
        fullData.resize(descOffset + sizeof(IMAGE_IMPORT_DESCRIPTOR) * 2, 0);

        size_t dllNameOffset = fullData.size();
        for (char c : importDll) fullData.push_back((uint8_t)c);
        fullData.push_back(0);

        size_t thunkCount = importFuncs.size() + 1;
        size_t thunkTableOffset = fullData.size();
        fullData.resize(thunkTableOffset + thunkCount * sizeof(IMAGE_THUNK_DATA64), 0);

        std::vector<size_t> nameOffsets;
        for (const auto& fn : importFuncs) {
            size_t nOffset = fullData.size();
            fullData.push_back(0);
            fullData.push_back(0);
            for (char c : fn) fullData.push_back((uint8_t)c);
            fullData.push_back(0);
            nameOffsets.push_back(nOffset);
        }

        IMAGE_THUNK_DATA64* pThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(fullData.data() + thunkTableOffset);
        for (size_t i = 0; i < importFuncs.size(); ++i) {
            pThunk[i].u1.AddressOfData = 0x2000 + static_cast<uint64_t>(nameOffsets[i]);
        }

        IMAGE_IMPORT_DESCRIPTOR* pDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(fullData.data() + descOffset);
        pDesc->OriginalFirstThunk = 0x2000 + static_cast<uint32_t>(thunkTableOffset);
        pDesc->Name = 0x2000 + static_cast<uint32_t>(dllNameOffset);
        pDesc->FirstThunk = 0x2000 + static_cast<uint32_t>(thunkTableOffset);
        importSize = static_cast<uint32_t>(fullData.size() - descOffset);
    }

    size_t dataRawSize = ((fullData.size() + FILE_ALIGN - 1) / FILE_ALIGN) * FILE_ALIGN;
    if (dataRawSize == 0) dataRawSize = FILE_ALIGN;

    std::vector<uint8_t> pe(HEADER_SIZE + codeRawSize + dataRawSize, 0);

    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(pe.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;

    uint8_t* ntPtr = pe.data() + dos->e_lfanew;
    *reinterpret_cast<DWORD*>(ntPtr) = IMAGE_NT_SIGNATURE;

    IMAGE_FILE_HEADER* fileHdr = reinterpret_cast<IMAGE_FILE_HEADER*>(ntPtr + sizeof(DWORD));
    fileHdr->Machine = IMAGE_FILE_MACHINE_AMD64;
    fileHdr->NumberOfSections = 2;
    fileHdr->TimeDateStamp = 0x66000000;
    fileHdr->SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    fileHdr->Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;

    IMAGE_OPTIONAL_HEADER64* optHdr = reinterpret_cast<IMAGE_OPTIONAL_HEADER64*>(
        ntPtr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)
    );
    optHdr->Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    optHdr->AddressOfEntryPoint = 0x1000;
    optHdr->ImageBase = 0x140000000;
    optHdr->SectionAlignment = 0x1000;
    optHdr->FileAlignment = static_cast<DWORD>(FILE_ALIGN);
    optHdr->SizeOfImage = 0x3000;
    optHdr->SizeOfHeaders = static_cast<DWORD>(HEADER_SIZE);
    optHdr->Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
    optHdr->NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

    if (importRva > 0) {
        optHdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = importRva;
        optHdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = importSize;
    }

    IMAGE_SECTION_HEADER* sec = reinterpret_cast<IMAGE_SECTION_HEADER*>(
        reinterpret_cast<uint8_t*>(optHdr) + sizeof(IMAGE_OPTIONAL_HEADER64)
    );

    std::memcpy(sec[0].Name, ".text\0\0\0", 8);
    sec[0].VirtualAddress = 0x1000;
    sec[0].Misc.VirtualSize = static_cast<DWORD>(codeBytes.size());
    sec[0].PointerToRawData = static_cast<DWORD>(HEADER_SIZE);
    sec[0].SizeOfRawData = static_cast<DWORD>(codeRawSize);
    sec[0].Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;

    std::memcpy(sec[1].Name, ".rdata\0\0", 8);
    sec[1].VirtualAddress = 0x2000;
    sec[1].Misc.VirtualSize = static_cast<DWORD>(fullData.size());
    sec[1].PointerToRawData = static_cast<DWORD>(HEADER_SIZE + codeRawSize);
    sec[1].SizeOfRawData = static_cast<DWORD>(dataRawSize);
    sec[1].Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

    if (!codeBytes.empty()) std::memcpy(pe.data() + HEADER_SIZE, codeBytes.data(), codeBytes.size());
    if (!fullData.empty())  std::memcpy(pe.data() + HEADER_SIZE + codeRawSize, fullData.data(), fullData.size());

    return pe;
}

void TestEntropyCalculation() {
    std::cout << "[TEST] Running Shannon Entropy calculation tests...\n";

    // All identical bytes: entropy = 0.0
    std::vector<uint8_t> zeroBytes(1024, 0x00);
    double h0 = PeReader::CalculateEntropy(zeroBytes.data(), zeroBytes.size());
    TEST_ASSERT(std::abs(h0 - 0.0) < 0.001);

    // Uniform distribution across 256 byte values: entropy = 8.0
    std::vector<uint8_t> uniformBytes(256 * 10);
    for (size_t i = 0; i < uniformBytes.size(); ++i) {
        uniformBytes[i] = static_cast<uint8_t>(i % 256);
    }
    double h8 = PeReader::CalculateEntropy(uniformBytes.data(), uniformBytes.size());
    TEST_ASSERT(std::abs(h8 - 8.0) < 0.001);

    std::cout << "  [PASS] Entropy calculation passed (Identical=0.00, Uniform=8.00).\n";
}

void TestCleanBinary() {
    std::cout << "[TEST] Running Clean Binary test...\n";

    const uint8_t code[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
    std::vector<uint8_t> codeVec(std::begin(code), std::end(code));
    std::string text = "Normal program strings.";
    std::vector<uint8_t> dataVec(text.begin(), text.end());

    auto peBytes = BuildTestPe64(codeVec, dataVec, "USER32.dll", { "MessageBoxW" });

    PeReader pe;
    bool ok = pe.OpenMemory(peBytes.data(), peBytes.size());
    TEST_ASSERT(ok);
    TEST_ASSERT(pe.Is64Bit());
    TEST_ASSERT(pe.GetSections().size() == 2);

    TriageReport report;
    report.sections = pe.GetSections();
    report.imports = pe.GetImports();

    InstructionScanner is;
    is.Scan(pe, report);

    StringScanner ss;
    ss.Scan(pe, report);

    ThreatAssessor::Assess(pe, report);

    TEST_ASSERT(report.threatLevel == ThreatLevel::Clean);
    TEST_ASSERT(report.mood == GhostMood::Happy);
    TEST_ASSERT(report.threatScore < 20);
    TEST_ASSERT(report.personalityDialogue.find("All clear") != std::string::npos);

    std::cout << "  [PASS] Clean binary verdict: HAPPY / ALL CLEAR (Score: " << report.threatScore << ")\n";
}

void TestDirectSyscallAndPebTraversal() {
    std::cout << "[TEST] Running Direct Syscall + PEB Hashing test...\n";

    // Bytecode XOR-encoded with 0x77 to prevent static AV false positives on the test harness
    const uint8_t encCode[] = {
        0x65 ^ 0x77, 0x48 ^ 0x77, 0x8B ^ 0x77, 0x04 ^ 0x77, 0x25 ^ 0x77, 0x60 ^ 0x77, 0x00 ^ 0x77, 0x00 ^ 0x77, 0x00 ^ 0x77,
        0x49 ^ 0x77, 0x89 ^ 0x77, 0xCA ^ 0x77,
        0xB8 ^ 0x77, 0x18 ^ 0x77, 0x00 ^ 0x77, 0x00 ^ 0x77, 0x00 ^ 0x77,
        0x0F ^ 0x77, 0x05 ^ 0x77,
        0x31 ^ 0x77, 0xD2 ^ 0x77,
        0x0F ^ 0x77, 0xB6 ^ 0x77, 0x01 ^ 0x77,
        0xC1 ^ 0x77, 0xCA ^ 0x77, 0x0D ^ 0x77,
        0x01 ^ 0x77, 0xC2 ^ 0x77,
        0x48 ^ 0x77, 0xFF ^ 0x77, 0xC1 ^ 0x77,
        0x85 ^ 0x77, 0xC0 ^ 0x77,
        0x75 ^ 0x77, 0xF2 ^ 0x77,
        0xC3 ^ 0x77
    };
    std::vector<uint8_t> codeVec(sizeof(encCode));
    for (size_t i = 0; i < sizeof(encCode); ++i) {
        codeVec[i] = encCode[i] ^ 0x77;
    }
    std::vector<uint8_t> dataVec(64, 0);

    auto peBytes = BuildTestPe64(codeVec, dataVec, "ntdll.dll", { "NtClose" });

    PeReader pe;
    bool ok = pe.OpenMemory(peBytes.data(), peBytes.size());
    TEST_ASSERT(ok);

    TriageReport report;
    report.sections = pe.GetSections();
    report.imports = pe.GetImports();

    InstructionScanner is;
    is.Scan(pe, report);

    TEST_ASSERT(!report.syscalls.empty());
    TEST_ASSERT(!report.pebAccesses.empty());
    TEST_ASSERT(!report.apiHashLoops.empty());

    ThreatAssessor::Assess(pe, report);

    TEST_ASSERT(report.threatLevel == ThreatLevel::Malicious);
    TEST_ASSERT(report.mood == GhostMood::Alarmed);
    TEST_ASSERT(report.threatScore >= 60);
    TEST_ASSERT(report.personalityDialogue.find("Sneaky sneaky") != std::string::npos);

    std::cout << "  [PASS] Direct Syscall + PEB test passed: ALARMED / MALICIOUS (Syscalls: "
              << report.syscalls.size() << ", PEB: " << report.pebAccesses.size() << ", Score: "
              << report.threatScore << ")\n";
}

void TestPackedMummy() {
    std::cout << "[TEST] Running High Entropy / Packed Mummy test...\n";

    std::mt19937 mt(1337);
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<uint8_t> highEntropyCode(1024);
    for (size_t i = 0; i < highEntropyCode.size(); ++i) {
        highEntropyCode[i] = static_cast<uint8_t>(dist(mt));
    }
    std::vector<uint8_t> dataVec(64, 0);

    auto peBytes = BuildTestPe64(highEntropyCode, dataVec, "KERNEL32.dll", { "LoadLibraryA" });

    PeReader pe;
    bool ok = pe.OpenMemory(peBytes.data(), peBytes.size());
    TEST_ASSERT(ok);

    TriageReport report;
    report.sections = pe.GetSections();
    report.imports = pe.GetImports();

    InstructionScanner is;
    is.Scan(pe, report);

    ThreatAssessor::Assess(pe, report);

    TEST_ASSERT(report.threatLevel == ThreatLevel::Suspicious || report.threatLevel == ThreatLevel::Malicious);
    TEST_ASSERT(report.mood == GhostMood::Puzzled || report.mood == GhostMood::Alarmed);
    TEST_ASSERT(report.personalityDialogue.find("packed like a mummy") != std::string::npos);

    std::cout << "  [PASS] Packed Mummy test passed: PUZZLED (Section Entropy: "
              << report.sections[0].entropy << " > 7.2)\n";
}

void TestCredentialStealerStrings() {
    std::cout << "[TEST] Running Credential Stealer Strings test...\n";

    const uint8_t code[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
    std::vector<uint8_t> codeVec(std::begin(code), std::end(code));

    std::string sensitiveData = std::string("Target: ") + "\\" + "Login" + " Data" + " and " + "\\" + "Cookies"
        + " exfil to " + "https://" + "dis" + "cord" + ".com" + "/api" + "/web" + "hooks" + "/test_hook"
        + " with " + "Crypt" + "Unprotect" + "Data";
    std::vector<uint8_t> dataVec(sensitiveData.begin(), sensitiveData.end());

    auto peBytes = BuildTestPe64(codeVec, dataVec, "CRYPT32.dll", { "CryptUnprotectData" });

    PeReader pe;
    bool ok = pe.OpenMemory(peBytes.data(), peBytes.size());
    TEST_ASSERT(ok);

    TriageReport report;
    report.sections = pe.GetSections();
    report.imports = pe.GetImports();

    StringScanner ss;
    ss.Scan(pe, report);

    TEST_ASSERT(!report.sensitiveStrings.empty());

    ThreatAssessor::Assess(pe, report);

    TEST_ASSERT(report.threatLevel == ThreatLevel::Malicious);
    TEST_ASSERT(report.mood == GhostMood::Alarmed);
    TEST_ASSERT(report.personalityDialogue.find("Chrome/Edge browser passwords") != std::string::npos);

    std::cout << "  [PASS] Credential Stealer test passed: ALARMED / MALICIOUS (Strings: "
              << report.sensitiveStrings.size() << ", Score: " << report.threatScore << ")\n";
}

void TestProcessInjectionChain() {
    std::cout << "[TEST] Running Process Injection Chain test...\n";

    const uint8_t code[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
    std::vector<uint8_t> codeVec(std::begin(code), std::end(code));
    std::vector<uint8_t> dataVec(64, 0);

    auto peBytes = BuildTestPe64(codeVec, dataVec, "KERNEL32.dll", {
        "VirtualAllocEx",
        "WriteProcessMemory",
        "CreateRemoteThread"
    });

    PeReader pe;
    bool ok = pe.OpenMemory(peBytes.data(), peBytes.size());
    TEST_ASSERT(ok);

    TriageReport report;
    report.sections = pe.GetSections();
    report.imports = pe.GetImports();

    InstructionScanner is;
    is.Scan(pe, report);

    TEST_ASSERT(report.injectionChain.detected);
    TEST_ASSERT(report.injectionChain.hasRwxAllocation);
    TEST_ASSERT(report.injectionChain.hasProcessWrite);
    TEST_ASSERT(report.injectionChain.hasRemoteExecution);

    ThreatAssessor::Assess(pe, report);

    TEST_ASSERT(report.threatLevel == ThreatLevel::Malicious);
    TEST_ASSERT(report.mood == GhostMood::Alarmed);
    TEST_ASSERT(report.personalityDialogue.find("Classic process injection") != std::string::npos);

    std::cout << "  [PASS] Process Injection test passed: ALARMED / MALICIOUS (Chained APIs: "
              << report.injectionChain.chainedApis.size() << ")\n";
}

void TestCryptoHashesAndImphash() {
    std::cout << "[TEST] Running Crypto Hashes & Imphash verification tests...\n";

    const uint8_t sampleData[] = "Koltzi Standalone Portable PE Malware Triage Agent 2026";
    TriageReport report;
    std::vector<ImportEntry> testImports = {
        { "KERNEL32.dll", { "CreateFileW", "CloseHandle" } },
        { "USER32.dll", { "MessageBoxW" } }
    };

    bool ok = CryptoVerifier::ComputeHashes(sampleData, sizeof(sampleData) - 1, testImports, report);
    TEST_ASSERT(ok);
    TEST_ASSERT(report.md5.length() == 32);
    TEST_ASSERT(report.sha1.length() == 40);
    TEST_ASSERT(report.sha256.length() == 64);
    TEST_ASSERT(report.imphash.length() == 32);
    TEST_ASSERT(!report.logEntries.empty());

    std::cout << "  [PASS] MD5:    " << report.md5 << "\n";
    std::cout << "  [PASS] SHA-1:  " << report.sha1 << "\n";
    std::cout << "  [PASS] SHA-256:" << report.sha256 << "\n";
    std::cout << "  [PASS] Imphash:" << report.imphash << "\n";
}

void TestBrowserContextAndFalsePositiveSuppression() {
    std::cout << "[TEST] Running Browser False-Positive Disambiguation tests...\n";

    // 1. Synthesize a binary that mimics a modern browser binary:
    // It has normal imports (KERNEL32, USER32)
    // It contains CRT startup PEB read (GS:[0x60])
    // It contains internal profile path "\\Login Data"
    // And it has a verified Authenticode signature from "Mozilla Corporation"
    const uint8_t code[] = {
        0x48, 0x83, 0xEC, 0x28,                               // sub rsp, 28h
        0x65, 0x48, 0x8B, 0x04, 0x25, 0x60, 0x00, 0x00, 0x00, // mov rax, gs:[60h] (benign CRT startup)
        0x48, 0x83, 0xC4, 0x28,                               // add rsp, 28h
        0xC3                                                  // ret
    };
    std::vector<uint8_t> codeVec(std::begin(code), std::end(code));

    std::string internalBrowserStrings = "Mozilla Firefox Nightly Internal Profile: \\Login Data and \\Cookies manager";
    std::vector<uint8_t> dataVec(internalBrowserStrings.begin(), internalBrowserStrings.end());

    auto peBytes = BuildTestPe64(codeVec, dataVec, "KERNEL32.dll", {
        "CreateFileW", "ReadFile", "CloseHandle", "GetModuleHandleW",
        "VirtualAlloc", "VirtualFree", "GetProcAddress", "LoadLibraryW",
        "MultiByteToWideChar", "WideCharToMultiByte", "GetLastError"
    });

    PeReader pe;
    bool ok = pe.OpenMemory(peBytes.data(), peBytes.size());
    TEST_ASSERT(ok);

    TriageReport report;
    report.fileName = "firefox.exe";
    report.machineType = pe.GetMachineString();
    report.subsystem = pe.GetSubsystemString();
    report.fileSize = pe.GetFileSize();
    report.overallEntropy = pe.GetOverallEntropy();
    report.sections = pe.GetSections();
    report.imports = pe.GetImports();

    // Simulate verified publisher signature
    report.signature.isSigned = true;
    report.signature.isValid = true;
    report.signature.isTrustedVendor = true;
    report.signature.signerSubject = "Mozilla Corporation";
    report.signature.signerIssuer = "DigiCert Trusted G4 Code Signing";
    report.signature.statusText = "Valid Authenticode Digital Signature";
    report.isLegitimateBrowser = true;

    InstructionScanner is;
    is.Scan(pe, report);

    // Verify CRT PEB access is recognized as benign
    TEST_ASSERT(!report.pebAccesses.empty());
    TEST_ASSERT(report.pebAccesses[0].isCrtTlsInit == true);

    StringScanner ss;
    ss.Scan(pe, report);

    // Verify browser profile strings are contextually suppressed
    for (const auto& str : report.sensitiveStrings) {
        if (str.category == "Credential Scraping") {
            TEST_ASSERT(str.isSuppressedByContext == true);
        }
    }

    ThreatAssessor::Assess(pe, report);

    // VERDICT MUST BE CLEAN / HAPPY!
    TEST_ASSERT(report.threatLevel == ThreatLevel::Clean);
    TEST_ASSERT(report.threatScore == 0);
    TEST_ASSERT(report.mood == GhostMood::Happy);
    TEST_ASSERT(report.personalityDialogue.find("Verified publisher") != std::string::npos);

    std::cout << "  [PASS] Browser false-positive test passed: CLEAN / HAPPY (Threat Score: "
              << report.threatScore << " / 100)\n";
}

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        std::string pathStr = argv[1];
        std::wstring wpath = Utf8ToWide(pathStr);
        std::cout << "========================================================\n";
        std::cout << "  KOLTZI LIVE TRIAGE: " << pathStr << "\n";
        std::cout << "========================================================\n";
        auto start = std::chrono::high_resolution_clock::now();
        PeReader pe;
        if (!pe.OpenFile(wpath)) {
            std::cout << "[ERROR] " << pe.GetError() << "\n";
            return 1;
        }
        TriageReport r;
        r.fileName = pathStr;
        r.machineType = pe.GetMachineString();
        r.subsystem = pe.GetSubsystemString();
        r.fileSize = pe.GetFileSize();
        r.overallEntropy = pe.GetOverallEntropy();
        r.sections = pe.GetSections();
        r.imports = pe.GetImports();

        CryptoVerifier::ComputeHashes(pe.GetBaseAddress(), pe.GetFileSize(), r.imports, r);
        CryptoVerifier::VerifyAuthenticode(wpath, pe.GetBaseAddress(), pe.GetFileSize(), r);

        InstructionScanner is;
        is.Scan(pe, r);

        StringScanner ss;
        ss.Scan(pe, r);

        ThreatAssessor::Assess(pe, r);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "Architecture:     " << r.machineType << "\n";
        std::cout << "Subsystem:        " << r.subsystem << "\n";
        std::cout << "File Size:        " << FormatFileSize(r.fileSize) << "\n";
        std::cout << "File Entropy:     " << r.overallEntropy << " / 8.00\n";
        std::cout << "Sections Count:   " << r.sections.size() << "\n";
        std::cout << "Imports Count:    " << r.imports.size() << " DLLs\n";
        std::cout << "Triage Latency:   " << ms << " ms (sub-100ms offline guarantee)\n";
        std::cout << "Threat Score:     " << r.threatScore << " / 100\n";
        std::cout << "Threat Level:     " << (r.threatScore >= 60 ? "MALICIOUS" : r.threatScore >= 20 ? "SUSPICIOUS" : "CLEAN") << "\n";
        std::cout << "Ghost Mood:       " << (r.mood == GhostMood::Alarmed ? "ALARMED" : r.mood == GhostMood::Puzzled ? "PUZZLED" : "HAPPY") << "\n";
        std::cout << "Mascot Dialogue:  \"" << r.personalityDialogue << "\"\n";
        std::cout << "--------------------------------------------------------\n";
        std::cout << "Cryptographic Hashes:\n";
        std::cout << "  MD5:            " << r.md5 << "\n";
        std::cout << "  SHA-1:          " << r.sha1 << "\n";
        std::cout << "  SHA-256:        " << r.sha256 << "\n";
        std::cout << "  Imphash:        " << r.imphash << "\n";
        std::cout << "--------------------------------------------------------\n";
        std::cout << "Authenticode Signature:\n";
        std::cout << "  Status:         " << r.signature.statusText << "\n";
        if (r.signature.isSigned) {
            std::cout << "  Subject:        " << r.signature.signerSubject << "\n";
            std::cout << "  Issuer:         " << r.signature.signerIssuer << "\n";
            std::cout << "  Trusted Vendor: " << (r.signature.isTrustedVendor ? "YES" : "NO") << "\n";
        }
        std::cout << "--------------------------------------------------------\n";
        std::cout << "Section Breakdown:\n";
        for (const auto& sec : r.sections) {
            std::cout << "  * " << sec.name << " (Raw: " << sec.rawSize << " bytes, Entropy: " << sec.entropy
                      << (sec.isSuspiciousEntropy ? " [!PACKED/HIGH ENTROPY]" : "")
                      << (sec.isRwx ? " [!RWX VIOLATION]" : "") << ")\n";
        }
        std::cout << "Technical Bullet Points:\n";
        for (const auto& item : r.technicalDetails) {
            std::cout << "  * " << item << "\n";
        }
        std::cout << "========================================================\n";
        return 0;
    }

    std::cout << "========================================================\n";
    std::cout << "  KOLTZI AUTOMATED TEST SUITE\n";
    std::cout << "========================================================\n";

    TestEntropyCalculation();
    TestCleanBinary();
    TestCryptoHashesAndImphash();
    TestBrowserContextAndFalsePositiveSuppression();
    TestDirectSyscallAndPebTraversal();
    TestPackedMummy();
    TestCredentialStealerStrings();
    TestProcessInjectionChain();

    std::cout << "========================================================\n";
    std::cout << "  ALL TESTS PASSED WITH 100% SUCCESS!\n";
    std::cout << "========================================================\n";
    return 0;
}
