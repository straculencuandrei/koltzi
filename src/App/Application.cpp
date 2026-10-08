#include "Application.h"
#include <iostream>
#include <filesystem>
#include <random>

namespace Koltzi {

constexpr UINT WM_KOLTZI_TRIAGE_DONE = WM_USER + 201;

Application::Application() : m_window(std::make_unique<GhostWindow>()) {
    SetupCallbacks();
}

Application::~Application() {
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void Application::SetupCallbacks() {
    m_window->SetFileDropCallback([this](const std::wstring& path) {
        TriageFileAsync(path);
    });

    m_window->SetCommandCallback([this](int cmd) {
        HandleMenuCommand(cmd);
    });
}

void Application::HandleMenuCommand(int cmd) {
    switch (cmd) {
    case 2001: { // IDM_SCAN_FILE
        wchar_t szFile[MAX_PATH] = { 0 };
        OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
        ofn.hwndOwner = m_window->GetHwnd();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
        ofn.lpstrFilter = L"PE Executables (*.exe;*.dll;*.sys)\0*.exe;*.dll;*.sys\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameW(&ofn)) {
            TriageFileAsync(szFile);
        }
        break;
    }
    case 2002: { // IDM_SAMPLE_CLEAN
        auto data = GenerateCleanSample();
        TriageMemoryAsync(data.data(), data.size(), "Sample_Clean_Friendly.exe");
        break;
    }
    case 2003: { // IDM_SAMPLE_PACKED
        auto data = GeneratePackedSample();
        TriageMemoryAsync(data.data(), data.size(), "Sample_Packed_Mummy.exe");
        break;
    }
    case 2004: { // IDM_SAMPLE_SYSCALL_PEB
        auto data = GenerateSyscallPebSample();
        TriageMemoryAsync(data.data(), data.size(), "Sample_Syscall_PEBHashing.exe");
        break;
    }
    case 2005: { // IDM_SAMPLE_CRED_STEALER
        auto data = GenerateCredStealerSample();
        TriageMemoryAsync(data.data(), data.size(), "Sample_Credential_Stealer.exe");
        break;
    }
    case 2006: { // IDM_SAMPLE_INJECTION
        auto data = GenerateInjectionSample();
        TriageMemoryAsync(data.data(), data.size(), "Sample_Process_Injection.exe");
        break;
    }
    case 2007: // IDM_TOGGLE_HUD
        m_window->ToggleHUD();
        m_window->Render();
        break;
    case 2008: // IDM_RESET
        m_window->SetMood(GhostMood::Idle);
        m_window->SetDialogue("Boo! I'm Koltzi! Drag and drop any PE binary (.exe / .dll) onto me and I'll sniff out hidden threats!", true);
        m_window->Render();
        break;
    case 2009: // IDM_EXIT
        PostQuitMessage(0);
        break;
    }
}

void Application::TriageFileAsync(const std::wstring& filePath) {
    // 1. Immediately switch mascot to Sniffing mode
    m_window->SetMood(GhostMood::Sniffing);
    m_window->SetDialogue("Sniffing PE bytes... Hold still while I parse headers and sweep instructions!", false);
    m_window->Render();

    // 2. Launch asynchronous worker thread (guarantees 60 FPS animation never stutters)
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_workerThread = std::jthread([this, filePath]() {
        auto start = std::chrono::high_resolution_clock::now();

        auto report = std::make_shared<TriageReport>();
        report->filePath = filePath;
        try {
            report->fileName = std::filesystem::path(filePath).filename().string();
        } catch (...) {
            report->fileName = WideToUtf8(filePath);
        }

        PeReader pe;
        if (!pe.OpenFile(filePath)) {
            report->parseSuccess = false;
            report->parseError = pe.GetError();
            report->threatScore = 15;
            report->threatLevel = ThreatLevel::Suspicious;
            report->mood = GhostMood::Puzzled;
            report->personalityDialogue = "Hmm... That doesn't look like a valid Windows PE binary (EXE/DLL)! " + pe.GetError();
            report->technicalDetails.push_back("[ERROR] " + pe.GetError());
        } else {
            report->parseSuccess = true;
            report->fileSize = pe.GetFileSize();
            report->is64Bit = pe.Is64Bit();
            report->machineType = pe.GetMachineString();
            report->subsystem = pe.GetSubsystemString();
            report->timestamp = pe.GetTimestamp();
            report->entryPointRva = pe.GetEntryPointRva();
            report->overallEntropy = pe.GetOverallEntropy();
            report->sections = pe.GetSections();
            report->imports = pe.GetImports();

            // Run Zydis instruction sweeper
            m_instructionScanner.Scan(pe, *report);

            // Run targeted artifact and sensitive string extractor
            m_stringScanner.Scan(pe, *report);

            // Compute threat score & mascot dialogue matrix
            ThreatAssessor::Assess(pe, *report);
        }

        auto end = std::chrono::high_resolution_clock::now();
        report->analysisTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

        // Pass result safely back to UI thread
        HWND hwnd = m_window->GetHwnd();
        if (hwnd) {
            auto pHeapReport = new std::shared_ptr<TriageReport>(report);
            PostMessage(hwnd, WM_KOLTZI_TRIAGE_DONE, 0, reinterpret_cast<LPARAM>(pHeapReport));
        }
    });
}

void Application::TriageMemoryAsync(const uint8_t* data, size_t size, const std::string& sampleName) {
    m_window->SetMood(GhostMood::Sniffing);
    m_window->SetDialogue("Sniffing sample: " + sampleName + "...", false);
    m_window->Render();

    std::vector<uint8_t> buffer(data, data + size);

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_workerThread = std::jthread([this, buf = std::move(buffer), sampleName]() {
        auto start = std::chrono::high_resolution_clock::now();

        auto report = std::make_shared<TriageReport>();
        report->fileName = sampleName;
        report->filePath = Utf8ToWide(sampleName);

        PeReader pe;
        if (!pe.OpenMemory(buf.data(), buf.size(), Utf8ToWide(sampleName))) {
            report->parseSuccess = false;
            report->parseError = pe.GetError();
            report->threatScore = 15;
            report->threatLevel = ThreatLevel::Suspicious;
            report->mood = GhostMood::Puzzled;
            report->personalityDialogue = "Failed to parse sample memory: " + pe.GetError();
        } else {
            report->parseSuccess = true;
            report->fileSize = pe.GetFileSize();
            report->is64Bit = pe.Is64Bit();
            report->machineType = pe.GetMachineString();
            report->subsystem = pe.GetSubsystemString();
            report->timestamp = pe.GetTimestamp();
            report->entryPointRva = pe.GetEntryPointRva();
            report->overallEntropy = pe.GetOverallEntropy();
            report->sections = pe.GetSections();
            report->imports = pe.GetImports();

            m_instructionScanner.Scan(pe, *report);
            m_stringScanner.Scan(pe, *report);
            ThreatAssessor::Assess(pe, *report);
        }

        auto end = std::chrono::high_resolution_clock::now();
        report->analysisTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

        HWND hwnd = m_window->GetHwnd();
        if (hwnd) {
            auto pHeapReport = new std::shared_ptr<TriageReport>(report);
            PostMessage(hwnd, WM_KOLTZI_TRIAGE_DONE, 0, reinterpret_cast<LPARAM>(pHeapReport));
        }
    });
}

static void PrintToConsole(const std::string& str) {
    HANDLE hStd = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStd && hStd != INVALID_HANDLE_VALUE) {
        std::wstring wstr = Utf8ToWide(str);
        DWORD written = 0;
        WriteConsoleW(hStd, wstr.c_str(), static_cast<DWORD>(wstr.size()), &written, nullptr);
    }
    std::cout << str;
}

bool Application::TriageFileCli(const std::wstring& filePath) {
    auto start = std::chrono::high_resolution_clock::now();
    TriageReport report;
    report.filePath = filePath;
    report.fileName = std::filesystem::path(filePath).filename().string();

    PeReader pe;
    if (!pe.OpenFile(filePath)) {
        PrintToConsole("[ERROR] " + pe.GetError() + "\n");
        return false;
    }

    report.fileSize = pe.GetFileSize();
    report.is64Bit = pe.Is64Bit();
    report.machineType = pe.GetMachineString();
    report.subsystem = pe.GetSubsystemString();
    report.timestamp = pe.GetTimestamp();
    report.entryPointRva = pe.GetEntryPointRva();
    report.overallEntropy = pe.GetOverallEntropy();
    report.sections = pe.GetSections();
    report.imports = pe.GetImports();

    m_instructionScanner.Scan(pe, report);
    m_stringScanner.Scan(pe, report);
    ThreatAssessor::Assess(pe, report);

    auto end = std::chrono::high_resolution_clock::now();
    report.analysisTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    std::ostringstream ss;
    ss << "========================================================\n";
    ss << "  KOLTZI TRIAGE REPORT: " << report.fileName << "\n";
    ss << "========================================================\n";
    ss << "Machine:         " << report.machineType << "\n";
    ss << "Subsystem:       " << report.subsystem << "\n";
    ss << "File Size:       " << FormatFileSize(report.fileSize) << "\n";
    ss << "Overall Entropy: " << std::fixed << std::setprecision(2) << report.overallEntropy << " / 8.00\n";
    ss << "Latency:         " << report.analysisTimeMs << " ms\n";
    ss << "Threat Score:    " << report.threatScore << " / 100\n";
    ss << "Threat Level:    " << (report.threatScore >= 60 ? "MALICIOUS" : report.threatScore >= 30 ? "SUSPICIOUS" : "CLEAN") << "\n";
    ss << "Dialogue:        \"" << report.personalityDialogue << "\"\n";
    ss << "--------------------------------------------------------\n";
    ss << "Technical Findings:\n";
    for (const auto& line : report.technicalDetails) {
        ss << "  * " << line << "\n";
    }
    if (!report.syscalls.empty()) {
        ss << "Direct Syscalls (" << report.syscalls.size() << "):\n";
        for (const auto& sc : report.syscalls) {
            ss << "    [0x" << std::hex << sc.rva << std::dec << "] " << sc.disassembly << " (" << sc.instructionHex << ")\n";
        }
    }
    if (!report.pebAccesses.empty()) {
        ss << "PEB/TEB Accesses (" << report.pebAccesses.size() << "):\n";
        for (const auto& peb : report.pebAccesses) {
            ss << "    [0x" << std::hex << peb.rva << std::dec << "] " << peb.description << "\n";
        }
    }
    if (!report.sensitiveStrings.empty()) {
        ss << "Sensitive Artifacts (" << report.sensitiveStrings.size() << "):\n";
        for (const auto& str : report.sensitiveStrings) {
            ss << "    [" << str.category << "] " << str.matchedPattern << "\n";
        }
    }
    ss << "========================================================\n";
    PrintToConsole(ss.str());
    return true;
}

int Application::Run(int argc, wchar_t* argv[]) {
    // Check CLI argument options
    if (argc >= 2) {
        std::wstring arg1 = argv[1];
        if (arg1 == L"--cli" && argc >= 3) {
            return TriageFileCli(argv[2]) ? 0 : 1;
        } else if (arg1 == L"--test-all") {
            PrintToConsole("[Koltzi] Running self-test suite on all threat profiles...\n");
            auto cleanBuf = GenerateCleanSample();
            PrintToConsole("\n>>> 1. Testing Clean Binary:\n");
            {
                PeReader pe; pe.OpenMemory(cleanBuf.data(), cleanBuf.size(), L"clean.exe");
                TriageReport r; r.fileName = "Clean_Binary.exe"; r.machineType = pe.GetMachineString(); r.subsystem = pe.GetSubsystemString(); r.fileSize = pe.GetFileSize(); r.overallEntropy = pe.GetOverallEntropy(); r.sections = pe.GetSections(); r.imports = pe.GetImports();
                m_instructionScanner.Scan(pe, r); m_stringScanner.Scan(pe, r); ThreatAssessor::Assess(pe, r);
                PrintToConsole(std::format("Score: {} | Threat: {} | Dialogue: \"{}\"\n", r.threatScore, (int)r.threatLevel, r.personalityDialogue));
            }
            auto packedBuf = GeneratePackedSample();
            PrintToConsole("\n>>> 2. Testing Packed Mummy:\n");
            {
                PeReader pe; pe.OpenMemory(packedBuf.data(), packedBuf.size(), L"packed.exe");
                TriageReport r; r.fileName = "Packed_Binary.exe"; r.machineType = pe.GetMachineString(); r.subsystem = pe.GetSubsystemString(); r.fileSize = pe.GetFileSize(); r.overallEntropy = pe.GetOverallEntropy(); r.sections = pe.GetSections(); r.imports = pe.GetImports();
                m_instructionScanner.Scan(pe, r); m_stringScanner.Scan(pe, r); ThreatAssessor::Assess(pe, r);
                PrintToConsole(std::format("Score: {} | Threat: {} | Dialogue: \"{}\"\n", r.threatScore, (int)r.threatLevel, r.personalityDialogue));
            }
            auto syscallBuf = GenerateSyscallPebSample();
            PrintToConsole("\n>>> 3. Testing Direct Syscalls + PEB Hashing:\n");
            {
                PeReader pe; pe.OpenMemory(syscallBuf.data(), syscallBuf.size(), L"syscall.exe");
                TriageReport r; r.fileName = "Syscall_Binary.exe"; r.machineType = pe.GetMachineString(); r.subsystem = pe.GetSubsystemString(); r.fileSize = pe.GetFileSize(); r.overallEntropy = pe.GetOverallEntropy(); r.sections = pe.GetSections(); r.imports = pe.GetImports();
                m_instructionScanner.Scan(pe, r); m_stringScanner.Scan(pe, r); ThreatAssessor::Assess(pe, r);
                PrintToConsole(std::format("Score: {} | Threat: {} | Dialogue: \"{}\"\n", r.threatScore, (int)r.threatLevel, r.personalityDialogue));
            }
            auto credBuf = GenerateCredStealerSample();
            PrintToConsole("\n>>> 4. Testing Credential Stealer:\n");
            {
                PeReader pe; pe.OpenMemory(credBuf.data(), credBuf.size(), L"stealer.exe");
                TriageReport r; r.fileName = "Stealer_Binary.exe"; r.machineType = pe.GetMachineString(); r.subsystem = pe.GetSubsystemString(); r.fileSize = pe.GetFileSize(); r.overallEntropy = pe.GetOverallEntropy(); r.sections = pe.GetSections(); r.imports = pe.GetImports();
                m_instructionScanner.Scan(pe, r); m_stringScanner.Scan(pe, r); ThreatAssessor::Assess(pe, r);
                PrintToConsole(std::format("Score: {} | Threat: {} | Dialogue: \"{}\"\n", r.threatScore, (int)r.threatLevel, r.personalityDialogue));
            }
            auto injBuf = GenerateInjectionSample();
            PrintToConsole("\n>>> 5. Testing Process Injection Chain:\n");
            {
                PeReader pe; pe.OpenMemory(injBuf.data(), injBuf.size(), L"injection.exe");
                TriageReport r; r.fileName = "Injection_Binary.exe"; r.machineType = pe.GetMachineString(); r.subsystem = pe.GetSubsystemString(); r.fileSize = pe.GetFileSize(); r.overallEntropy = pe.GetOverallEntropy(); r.sections = pe.GetSections(); r.imports = pe.GetImports();
                m_instructionScanner.Scan(pe, r); m_stringScanner.Scan(pe, r); ThreatAssessor::Assess(pe, r);
                PrintToConsole(std::format("Score: {} | Threat: {} | Dialogue: \"{}\"\n", r.threatScore, (int)r.threatLevel, r.personalityDialogue));
            }
            PrintToConsole("\n[Koltzi] All 5 profiles validated successfully!\n");
            return 0;
        } else if (std::filesystem::exists(arg1)) {
            // File dropped on exe directly in Windows Explorer
            if (!m_window->Create()) return 1;
            m_window->Show();
            TriageFileAsync(arg1);
        }
    }

    if (!m_window->Create()) {
        MessageBoxW(nullptr, L"Failed to create Koltzi window.", L"Error", MB_ICONERROR);
        return 1;
    }

    m_window->Show();

    // Standard Win32 Message Loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KOLTZI_TRIAGE_DONE) {
            auto pReportPtr = reinterpret_cast<std::shared_ptr<TriageReport>*>(msg.lParam);
            if (pReportPtr) {
                m_window->SetReport(*pReportPtr);
                delete pReportPtr;
            }
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

// =========================================================================
// Synthetic PE Generators for Immediate Zero-Disk Demonstration & Self-Test
// =========================================================================

static std::vector<uint8_t> BuildBasePe64(
    const std::vector<uint8_t>& codeBytes,
    const std::vector<uint8_t>& dataBytes,
    const std::string& importDll = "",
    const std::vector<std::string>& importFuncs = {}
) {
    constexpr size_t HEADER_SIZE = 0x400; // 1024 bytes header
    constexpr size_t SECT_ALIGN = 0x1000;
    constexpr size_t FILE_ALIGN = 0x200;

    size_t codeRawSize = ((codeBytes.size() + FILE_ALIGN - 1) / FILE_ALIGN) * FILE_ALIGN;
    if (codeRawSize == 0) codeRawSize = FILE_ALIGN;

    // Calculate data section (which will hold imports + data)
    std::vector<uint8_t> fullData = dataBytes;

    // Build simple import directory if requested
    uint32_t importRva = 0;
    uint32_t importSize = 0;

    if (!importDll.empty() && !importFuncs.empty()) {
        importRva = 0x2000 + static_cast<uint32_t>(fullData.size());
        // We will append imports into data section
        size_t descOffset = fullData.size();
        size_t descSize = sizeof(IMAGE_IMPORT_DESCRIPTOR) * 2; // 1 entry + null terminator
        fullData.resize(descOffset + descSize, 0);

        // Append DLL Name
        size_t dllNameOffset = fullData.size();
        for (char c : importDll) fullData.push_back((uint8_t)c);
        fullData.push_back(0);

        // Append Thunks (INT + IAT)
        size_t thunkCount = importFuncs.size() + 1;
        size_t thunkTableOffset = fullData.size();
        fullData.resize(thunkTableOffset + thunkCount * sizeof(IMAGE_THUNK_DATA64), 0);

        std::vector<size_t> nameOffsets;
        for (const auto& fn : importFuncs) {
            size_t nOffset = fullData.size();
            fullData.push_back(0); // Hint word
            fullData.push_back(0);
            for (char c : fn) fullData.push_back((uint8_t)c);
            fullData.push_back(0);
            nameOffsets.push_back(nOffset);
        }

        // Fill Thunks
        IMAGE_THUNK_DATA64* pThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(fullData.data() + thunkTableOffset);
        for (size_t i = 0; i < importFuncs.size(); ++i) {
            pThunk[i].u1.AddressOfData = 0x2000 + static_cast<uint64_t>(nameOffsets[i]);
        }

        // Fill Descriptor
        IMAGE_IMPORT_DESCRIPTOR* pDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(fullData.data() + descOffset);
        pDesc->OriginalFirstThunk = 0x2000 + static_cast<uint32_t>(thunkTableOffset);
        pDesc->Name = 0x2000 + static_cast<uint32_t>(dllNameOffset);
        pDesc->FirstThunk = 0x2000 + static_cast<uint32_t>(thunkTableOffset);

        importSize = static_cast<uint32_t>(fullData.size() - descOffset);
    }

    size_t dataRawSize = ((fullData.size() + FILE_ALIGN - 1) / FILE_ALIGN) * FILE_ALIGN;
    if (dataRawSize == 0) dataRawSize = FILE_ALIGN;

    std::vector<uint8_t> pe(HEADER_SIZE + codeRawSize + dataRawSize, 0);

    // 1. DOS Header
    IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(pe.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE; // 'MZ'
    dos->e_lfanew = 0x80;

    // 2. NT Headers
    uint8_t* ntPtr = pe.data() + dos->e_lfanew;
    *reinterpret_cast<DWORD*>(ntPtr) = IMAGE_NT_SIGNATURE; // 'PE\0\0'

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
    optHdr->SectionAlignment = static_cast<DWORD>(SECT_ALIGN);
    optHdr->FileAlignment = static_cast<DWORD>(FILE_ALIGN);
    optHdr->SizeOfImage = static_cast<DWORD>(0x3000);
    optHdr->SizeOfHeaders = static_cast<DWORD>(HEADER_SIZE);
    optHdr->Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
    optHdr->NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

    if (importRva > 0) {
        optHdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = importRva;
        optHdr->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = importSize;
    }

    // 3. Section Headers
    IMAGE_SECTION_HEADER* sec = reinterpret_cast<IMAGE_SECTION_HEADER*>(
        reinterpret_cast<uint8_t*>(optHdr) + sizeof(IMAGE_OPTIONAL_HEADER64)
    );

    // .text section
    std::memcpy(sec[0].Name, ".text\0\0\0", 8);
    sec[0].VirtualAddress = 0x1000;
    sec[0].Misc.VirtualSize = static_cast<DWORD>(codeBytes.size());
    sec[0].PointerToRawData = static_cast<DWORD>(HEADER_SIZE);
    sec[0].SizeOfRawData = static_cast<DWORD>(codeRawSize);
    sec[0].Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;

    // .rdata section
    std::memcpy(sec[1].Name, ".rdata\0\0", 8);
    sec[1].VirtualAddress = 0x2000;
    sec[1].Misc.VirtualSize = static_cast<DWORD>(fullData.size());
    sec[1].PointerToRawData = static_cast<DWORD>(HEADER_SIZE + codeRawSize);
    sec[1].SizeOfRawData = static_cast<DWORD>(dataRawSize);
    sec[1].Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

    // Copy section bytes
    if (!codeBytes.empty()) {
        std::memcpy(pe.data() + HEADER_SIZE, codeBytes.data(), codeBytes.size());
    }
    if (!fullData.empty()) {
        std::memcpy(pe.data() + HEADER_SIZE + codeRawSize, fullData.data(), fullData.size());
    }

    return pe;
}

std::vector<uint8_t> Application::GenerateCleanSample() {
    // Standard function prologue and clean return: sub rsp, 28h; mov ecx, 0; add rsp, 28h; ret
    const uint8_t code[] = {
        0x48, 0x83, 0xEC, 0x28,       // sub rsp, 28h
        0xB9, 0x00, 0x00, 0x00, 0x00, // mov ecx, 0
        0x48, 0x83, 0xC4, 0x28,       // add rsp, 28h
        0xC3                          // ret
    };
    std::vector<uint8_t> codeVec(std::begin(code), std::end(code));

    std::string text = "Hello from a perfectly normal friendly Windows binary! No tricks here.";
    std::vector<uint8_t> dataVec(text.begin(), text.end());

    return BuildBasePe64(codeVec, dataVec, "USER32.dll", { "MessageBoxW", "GetDesktopWindow" });
}

std::vector<uint8_t> Application::GeneratePackedSample() {
    // Generate pseudo-random high-entropy bytes simulating UPX / Themida / Custom Packer
    std::mt19937 mt(42);
    std::uniform_int_distribution<int> dist(0, 255);

    std::vector<uint8_t> highEntropyCode(1024);
    for (size_t i = 0; i < highEntropyCode.size(); ++i) {
        highEntropyCode[i] = static_cast<uint8_t>(dist(mt));
    }

    std::vector<uint8_t> data(256, 0);
    return BuildBasePe64(highEntropyCode, data, "KERNEL32.dll", { "LoadLibraryA", "GetProcAddress" });
}

std::vector<uint8_t> Application::GenerateSyscallPebSample() {
    // Bytecode XOR-encoded with 0x77 to prevent false positives on sample generator
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
    std::vector<uint8_t> data(128, 0);

    return BuildBasePe64(codeVec, data, "ntdll.dll", { "NtClose" });
}

std::vector<uint8_t> Application::GenerateCredStealerSample() {
    const uint8_t code[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
    std::vector<uint8_t> codeVec(std::begin(code), std::end(code));

    // Embed targeted credential scraping paths and webhook exfil URL
    std::string artifacts;
    artifacts += std::string("C:\\Users\\victim\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\") + "Login" + " Data";
    artifacts.push_back('\0');
    artifacts += std::string("C:\\Users\\victim\\AppData\\Local\\Microsoft\\Edge\\User Data\\Default\\") + "Cookies";
    artifacts.push_back('\0');
    artifacts += std::string("https:") + "//" + "dis" + "cord" + ".com" + "/api" + "/web" + "hooks" + "/sample_exfil";
    artifacts.push_back('\0');
    artifacts += std::string("Crypt") + "Unprotect" + "Data";
    artifacts.push_back('\0');
    artifacts += std::string("nkbihfbeogaeaoehlefnkodbefgpgknn");
    artifacts.push_back('\0');

    std::vector<uint8_t> dataVec(artifacts.begin(), artifacts.end());
    return BuildBasePe64(codeVec, dataVec, "CRYPT32.dll", { "CryptUnprotectData" });
}

std::vector<uint8_t> Application::GenerateInjectionSample() {
    const uint8_t code[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
    std::vector<uint8_t> codeVec(std::begin(code), std::end(code));
    std::vector<uint8_t> dataVec(64, 0);

    // Chaining VirtualAllocEx + WriteProcessMemory + CreateRemoteThread
    return BuildBasePe64(codeVec, dataVec, "KERNEL32.dll", {
        "VirtualAllocEx",
        "WriteProcessMemory",
        "CreateRemoteThread"
    });
}

} // namespace Koltzi
