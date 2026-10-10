# Koltzi Architectural Specification and System Manual

This document provides a comprehensive technical overview of Koltzi for engineers and automated coding agents. It describes the design principles, ingestion pipeline, detection algorithms, decompilation engine, user interface, and operational workflows.

---

## 1. Executive Summary

Koltzi is an offline, air-gapped desktop malware triage agent and static reverse engineering workbench designed for Windows x64. It analyzes untrusted Portable Executable binaries (PE32 and PE32+) in under 15 milliseconds without executing target code and without sending data to any cloud service.

### Core Objectives
1. Sub-millisecond to sub-100ms static triage speed on multi-gigabyte disks using zero-copy memory mapping.
2. Accurate heuristic detection of evasive malware behaviors (direct system calls, evasive PEB walks, API hashing loops, cross-process injection chains, browser credential theft, and unsigned payload droppers).
3. Elimination of false positives on legitimate software (Chromium browser sandboxes, Microsoft Visual C++ CRT runtime initialization, verified code-signed software).
4. Automated subroutine discovery and C pseudocode decompilation for both 32-bit x86 and 64-bit x64 binaries without requiring external decompilation frameworks like Ghidra or IDA Pro.
5. Rich desktop user experience featuring an animated companion mascot that reflects forensic findings through procedural animations and forensic dialogue.

---

## 2. System Architecture and Data Flow

The triage lifecycle progresses through six sequential, synchronous or asynchronous pipeline stages:

```
[Target Binary on Disk / Memory]
             |
             v
 [1. Zero-Copy Ingestion & PE Parser]  --> PeReader.cpp
     - Headers (DOS, File, Optional)
     - Sections & Per-Section Shannon Entropy
     - PE Overlay Detection & Container Signature Extraction
             |
             v
 [2. Cryptographic & Identity Verification] --> CryptoVerifier.cpp
     - MD5, SHA-1, SHA-256, Imphash
     - Authenticode WinVerifyTrust & Publisher Identity
             |
             v
 [3. Instruction Analysis & Control-Flow Engine] --> InstructionScanner.cpp
     - Zydis x86/x64 Disassembler
     - Direct Syscall & Stub SSN Recovery
     - Evasive PEB/TEB Module Walking
     - API Hashing Immediate & Loop Detection (Metasploit ROR13, DJB2, etc.)
     - Process Injection Co-Location & RWX Verification
     - Subroutine Prologue Sweeper (x86 & x64)
     - C Pseudocode Generation & Semantic Subroutine Naming
             |
             v
 [4. Targeted Artifact & String Scanning] --> StringScanner.cpp
     - DPAPI, Browser Credential Paths, Crypto Wallets
     - Exfiltration Endpoints (Discord, Telegram, Pastebin)
     - Defense Evasion (vssadmin, bcdedit, Defender tampering)
     - Browser Internal Context Suppression
             |
             v
 [5. Threat Scoring & Forensic Correlation] --> ThreatAssessor.cpp
     - Correlated Multi-Factor Scoring (0 to 100)
     - PE Overlay Unsigned Dropper Evaluation
     - Legitimate Installer Disambiguation
     - Mascot Dialogue & Emotion State Machine
             |
             v
 [6. User Interface Presentation]
     - WebView2 Liquid-Glass Workbench (frontend/index.html, app.js)
     - Direct2D Hardware-Accelerated Native Window (GhostWindow.cpp)
     - Headless CLI / JSON Output (--cli, --json, --test)
```

---

## 3. Component Deep Dive

### 3.1. PeReader (`src/Core/PeReader.h`, `src/Core/PeReader.cpp`)
Responsible for reading PE headers and mapping raw binary data.
* **Memory-Mapped I/O**: Uses `CreateFileMappingW` and `MapViewOfFile` for zero-copy ingestion of large files up to several gigabytes. Supports in-memory buffers via `OpenMemory` for synthetic test suites.
* **Header Validation**: Validates `IMAGE_DOS_HEADER`, `IMAGE_NT_HEADERS32`, and `IMAGE_NT_HEADERS64`. Verifies magic constants, architecture (`IMAGE_FILE_MACHINE_I386` vs `IMAGE_FILE_MACHINE_AMD64`), entry point RVA, subsystem, and timestamp.
* **Section Analysis**: Parses section headers, calculates raw offsets, virtual sizes, memory permissions (Read, Write, Execute), and RWX violations.
* **Shannon Entropy**: Computes 256-bin lookup-table Shannon entropy (`0.00` to `8.00`) per section and overall file. Flags executable sections with entropy exceeding `7.20` as packed or encrypted.
* **PE Overlay Detection**: Computes `maxSectionEnd` across all PE sections. Any trailing data past `maxSectionEnd` is classified as PE overlay. Calculates overlay size, file percentage ratio, sampled Shannon entropy, and archive container signatures:
  * NSIS Solid-Compressed Payload (`NullsoftInst` or `0xDEADBEEF` marker)
  * Inno Setup Compressed Data
  * 7-Zip (`7z\xBC\xAF\x27\x1C`)
  * ZIP (`PK\x03\x04`)
  * RAR (`Rar!\x1A\x07`)
  * Embedded Portable Executable (`MZ`)

### 3.2. CryptoVerifier (`src/Core/CryptoVerifier.h`, `src/Core/CryptoVerifier.cpp`)
Handles cryptographic hashing and certificate validation.
* **Windows CNG (BCrypt)**: Calculates MD5, SHA-1, and SHA-256 digests via `BCryptOpenAlgorithmProvider` and `BCryptCreateHash`.
* **Import Hash (Imphash)**: Generates standard Mandiant/VirusTotal import hashes by normalizing DLL names to lowercase, ordering imports, formatting as `dll.function` or `dll.ordN`, comma-joining, and computing MD5.
* **Authenticode Validation**: Invokes `WinVerifyTrust` with `WINTRUST_ACTION_GENERIC_VERIFY_V2` and `WTD_CHOICE_FILE`. Evaluates digital signature validity, certificate chain status, and extracts publisher CN via `CryptQueryObject` and `CertGetNameStringW`.
* **Trusted Publisher Database**: Matches verified publishers against known vendors (Microsoft, Google, Mozilla, Apple, Valve, Brave, JetBrains).

### 3.3. InstructionScanner (`src/Core/InstructionScanner.h`, `src/Core/InstructionScanner.cpp`)
The core reverse engineering and static disassembly engine powered by Zydis.
* **Import Address Table Mapping**: Resolves IAT thunk addresses to `DLL!Function` names to annotate indirect calls (`call [rip+disp]` or `call [disp]`).
* **Direct Syscall Sweeper**: Scans executable sections for direct system call instructions (`0F 05` syscall, `0F 34` sysenter, and legacy `INT 0x2E`). Recovers System Service Numbers (SSN) from preceding `mov eax, <imm32>` instructions. Differentiates malicious evasion stubs from legitimate browser JIT compilers.
* **Evasive PEB Traversal**: Detects manual Process Environment Block traversals (`GS:[0x60]` in x64 or `FS:[0x30]` in x86). Distinguishes benign MSVC CRT cookie initialization (`__security_init_cookie` and TLS callbacks) from evasive `InMemoryOrderModuleList` dynamic export searching.
* **API Hashing Loop Recovery**: Scans for hashing loops resolving APIs dynamically without IAT entries. Supports Metasploit ROR13, DJB2, FNV1a (32-bit and 64-bit), CRC32, and SDBM. Validates against a static database of critical Windows API hashes (`LoadLibraryA`, `GetProcAddress`, `VirtualAlloc`, `WSAStartup`, `CreateProcessA`).
* **Process Injection Chain Verification**: Analyzes co-location of memory allocation (`VirtualAllocEx`), memory writing (`WriteProcessMemory`), and remote execution (`CreateRemoteThread`, `QueueUserAPC`, `SetThreadContext`) within the same subroutine scope. Flags `PAGE_EXECUTE_READWRITE` (0x40) arguments.
* **Subroutine Prologue Pattern Sweeper**: Discovers subroutine roots even in 32-bit x86 binaries lacking `.pdata` exception tables. Scans for standard function prologues:
  * x86: `55 8B EC` (`push ebp; mov ebp, esp`), `55 89 E5`, `8B FF 55 8B EC` (Microsoft HotPatch), `55 83 EC ??`, `55 81 EC ??`, `55 53 56 57`, `53 56 57`, `56 8B F1`.
  * x64: `48 83 EC ??` (`sub rsp, imm8`), `48 81 EC ??`, `55 48 89 E5`, `40 53`, `40 55`, `48 89 5C 24 ??`.
* **C Pseudocode Reconstruction**: Translates native instruction streams into structured, readable C pseudocode (`dst = src;`, `push(src);`, `dst = pop();`, `dst = &(src);`, arithmetic operations). Distinguishes 32-bit calling conventions (`int32_t __stdcall fn(void)`) from 64-bit Microsoft FastCall (`int64_t fn(int64_t rcx, ...)`).
* **Semantic Subroutine Classification**: Categorizes and names discovered functions based on invoked APIs and features:
  * `entrypoint_`: Main application entry point
  * `fn_UiInstaller_`: Dialog, window, message box routines (`USER32.dll!DefWindowProcW`, `CreateWindowEx`, `ShowWindow`)
  * `fn_FilePayloadIO_`: File staging and extraction routines (`CreateFileW`, `WriteFile`, `GetTempPathW`)
  * `fn_RegistryConfig_`: Windows registry configuration (`RegOpenKeyExW`, `RegQueryValueExW`)
  * `fn_DynApiResolver_`: Dynamic module and procedure loaders (`LoadLibraryW`, `GetProcAddress`)
  * `fn_ProcExecution_`: Process spawning (`CreateProcessW`, `ShellExecuteW`)
  * `fn_MemManager_`: Memory allocators (`VirtualAlloc`, `HeapAlloc`)
  * `fn_NetworkC2_`: Network communication (`InternetOpenW`, `HttpSendRequestW`, `WSAStartup`)
  * `fn_SyscallStub_`: Direct kernel dispatch routines
  * `fn_PebWalker_`: Dynamic PEB dereferencing routines
  * `fn_ApiHasher_`: Dynamic API hashing resolution loops
  * `sub_`: Unclassified general subroutines

### 3.4. StringScanner (`src/Core/StringScanner.h`, `src/Core/StringScanner.cpp`)
Scans raw section data and mapped file bodies for high-signal forensic strings.
* **Categories**:
  * Credential Scraping: `\Login Data`, `\Cookies`, `CryptUnprotectData`, `vaultcli.dll`, `samlib.dll`
  * Exfiltration C2: Discord Webhooks (`discord.com/api/webhooks`), Telegram Bot API (`api.telegram.org/bot`), Pastebin raw links
  * Defense Evasion: `vssadmin delete shadows`, `bcdedit /set {default} recoveryenabled No`, `Set-MpPreference -DisableRealtimeMonitoring`
  * Cryptocurrency Targets: Extension identifiers for MetaMask, Phantom, Exodus, TronLink, and `wallet.dat`
* **Context Suppression**: Suppresses false positives when scanning legitimate browser binaries (Chrome, Edge, Brave) by analyzing import diversity, code signatures, and surrounding string context.

### 3.5. ThreatAssessor (`src/Core/ThreatAssessor.h`, `src/Core/ThreatAssessor.cpp`)
Synthesizes findings into a unified threat score (0 to 100), threat level, and mascot emotion.
* **Scoring Rules**:
  * Direct Syscalls: `+40 pts`
  * Evasive PEB Module Traversal: `+30 pts`
  * Dynamic API Hashing Loops: `+25 pts`
  * Cross-Process Injection Chain: `+65 pts`
  * Active Exfiltration C2 Strings: `+70 pts`
  * Unverified Credential Scraping: `+55 pts`
  * Defense Evasion Commands: `+55 pts`
  * Cryptocurrency Wallet Targeting: `+40 pts`
  * RWX Section Violations: `+25 pts`
  * Packed Code Section (High Entropy): `+30 pts`
  * Unsigned High-Entropy PE Overlay Dropper (> 2 MB, H > 7.2): `+75 pts`
* **Trust Adjustments**:
  * Verified Trusted Publisher: `-60 pts` discount
  * Valid Authenticode Signature: `-50 pts` discount
  * Legitimate Clean Installer: Zeroed score only if digitally signed or carrying no anomalous unsigned high-entropy payload container
* **Mascot Mood State Machine**:
  * `Happy` (Score 0-19): Clean binary with verified or benign characteristics
  * `Puzzled` (Score 20-54): Suspicious packed code or unexplained entropy anomalies
  * `Alarmed` (Score 55-100): Evasive behaviors, injection chains, stealer patterns, or unsigned dropper containers
  * `Sniffing`: Active background ingestion state

---

## 4. Frontend and User Interface Architecture

Koltzi provides an interactive desktop environment with dual-mode support:

### 4.1. Modern Liquid-Glass Web Workbench (`frontend/`)
* **Technology**: Modern HTML5, Vanilla CSS3, and ES6 JavaScript hosted in an air-gapped WebView2 runtime (`app.js`, `index.html`, `style.css`).
* **Design System**: Liquid-glass morphism with translucent frosted surfaces, dynamic SVG glow filters, responsive layout, and theme gradient options. Zero external CDN dependencies.
* **Canvas Ghost Companion**: 60 FPS HTML5 Canvas particle mascot (`GhostCompanion` class in `app.js`). Features procedural floating physics, floating particles, eye tracking, expressive mood states, and interactive click speech bubbles.
* **Interactive Workbench Tabs**:
  1. *Overview*: Threat score dial, verdict badges, static heuristics list, and cryptographic hash copy tiles.
  2. *Sections and Headers*: Detailed section table with virtual/raw sizing, entropy progress bars, and memory permissions.
  3. *Telemetry Log*: Chronological audit log with severity filtering (`ALL`, `CRIT`, `WARN`, `PASS`, `INFO`, `DEBUG`).
  4. *Decompiler & Reverse Engineering*: Subroutine browser with search filtering, instruction metrics, called API tags, dual-mode C pseudocode and assembly listing views, and single-click clipboard copying.
* **Drag and Drop Engine**: Handles HTML5 drag-and-drop events with visual highlighting.

### 4.2. Direct2D Native Desktop Companion (`src/UI/`)
* **Components**: `GhostWindow.cpp`, `GhostRenderer.cpp`, `SpeechBubble.cpp`, `FontManager.cpp`.
* **Renderer**: Native hardware-accelerated Direct2D and DirectWrite with embedded Creato Display font typography.

---

## 5. File System and Repository Layout

```
Koltzi/
├── CMakeLists.txt                # CMake build definition (Release /MT, Zydis, BCrypt, WinTrust)
├── run.ps1                       # Primary build, test, and execution script
├── ARCHITECTURE.md               # This system manual
├── README.md                     # Project overview and quickstart guide
├── frontend/                     # Liquid-glass web frontend
│   ├── index.html                # Workbench markup, tab panes, and layout
│   ├── app.js                    # UI logic, Ghost companion canvas physics, decompiler renderer
│   └── style.css                 # CSS custom property tokens, glassmorphism styling
├── FONT/                         # Creato Display font weights (Thin, Regular, Medium, Bold, Black)
├── res/                          # Windows resource script and embedded fonts
│   ├── resource.h
│   └── Koltzi.rc
├── src/
│   ├── Main.cpp                  # WinMain entry point, command-line dispatch
│   ├── Common.h                  # Common macros, types, and utility functions
│   ├── App/
│   │   ├── Application.h/.cpp    # Application coordinator, async triage workers, sample generators
│   │   └── SampleGenerators.cpp  # Synthetic PE generators for unit testing
│   ├── Core/
│   │   ├── TriageReport.h        # Core data models and JSON serialization
│   │   ├── PeReader.h/.cpp       # Memory-mapped PE parser and overlay analyzer
│   │   ├── CryptoVerifier.h/.cpp # Cryptographic hashing and Authenticode validation
│   │   ├── InstructionScanner.h/.cpp # Zydis recursive descent, decompiler, and prologue sweeper
│   │   ├── StringScanner.h/.cpp  # Forensic string extraction and context filters
│   │   └── ThreatAssessor.h/.cpp # Scoring engine and mascot dialogue generator
│   └── UI/
│       ├── AnimationTypes.h      # Animation constants and mood states
│       ├── FontManager.h/.cpp    # DirectWrite font loader
│       ├── GhostRenderer.h/.cpp  # Procedural Direct2D companion mascot
│       ├── SpeechBubble.h/.cpp   # Direct2D dialogue bubble and typewriter animator
│       └── GhostWindow.h/.cpp    # Native Win32 window and event dispatcher
└── tests/
    └── TestRunner.cpp            # Automated 14-test suite and live CLI file triage
```

---

## 6. Real-World Case Study: HADES / UnoApp.exe

During development, Koltzi was tested against a real-world infostealer sample:
* **Target**: `UnoApp.exe` (124.5 MB disguised Uno card game installer).
* **Payload**: Electron v41 with Bytenode V8 bytecode (`app.protected.jsc`), credential harvesting modules, and Discord exfiltration.
* **Initial Behavior**: Koltzi incorrectly rated the file as `Score 0 / Safe installer` because:
  1. The executable section `.ndata` matched NSIS installer signatures.
  2. The installer check blindly wiped all threat flags.
  3. The malicious payload was stored entirely in the 118.65 MB PE overlay (`H = 8.00`), which was not inspected.
  4. The 32-bit NSIS stub had no `.pdata` table, leaving its functions undetected.
* **Resolution**:
  1. Implemented PE overlay extraction and Shannon entropy computation.
  2. Updated `ThreatAssessor` to assign `+75 pts` (Critical Malicious) to any unsigned binary carrying an overlay larger than 2 MB with entropy above 7.2.
  3. Restricted installer trust discounts to digitally signed setups or clean archives with no suspicious overlay.
  4. Added the x86 subroutine prologue pattern sweeper, uncovering 80 functions in `UnoApp.exe`.
* **Current Result**: Correctly flagged as `MALICIOUS` (Score 75/100, Alarmed mood) with all 80 functions decompiled cleanly.

---

## 7. Developer and Agent Guidelines

When maintaining or extending Koltzi:
1. **Coding Conventions**: Strict C++20. Zero external runtime dependencies beyond standard Windows system DLLs. Use `/MT` static CRT.
2. **Zero Emojis**: Do not use emojis in code, comments, documentation, or commit messages.
3. **Punctuation Rules**: Do not use em dashes or mid-sentence colons.
4. **Project Naming**: The application name is strictly **Koltzi**.
5. **Test Integrity**: All 14 test cases in `tests/TestRunner.cpp` must pass 100% on every modification.
6. **Execution Commands**:
   * Build release binaries: `powershell -ExecutionPolicy Bypass -File .\run.ps1 -Build`
   * Run automated tests: `powershell -ExecutionPolicy Bypass -File .\run.ps1 -Test`
   * Scan specific file via CLI: `.\build_rel\KoltziTests.exe <path>`
   * Launch application: `powershell -ExecutionPolicy Bypass -File .\run.ps1`
