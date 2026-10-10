<p align="center">
  <img src="assets/ghost-icon.png" width="160" height="160" alt="Koltzi Ghost Companion Mascot" />
</p>

<h1 align="center">Koltzi</h1>

<p align="center">
  <strong>Zero-Bloat Desktop Malware Triage Agent &amp; Ghost Companion</strong><br>
  <em>Sub-100ms PE Static Analysis with Verified Forensics &amp; Interactive Knowledge Base</em>
</p>

<p align="center">
  <a href="https://en.cppreference.com/w/cpp/20"><img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="C++20" /></a>
  <img src="https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20x64-brightgreen.svg" alt="Platform" />
  <img src="https://img.shields.io/badge/Binary%20Size-1.05%20MB%20(Single%20EXE)-orange.svg" alt="Binary Size" />
  <img src="https://img.shields.io/badge/Telemetry-0%25%20(100%25%20Offline)-success.svg" alt="Telemetry" />
</p>

---

## 1. Overview & Architecture
Koltzi is an offline Windows desktop utility designed for sub-100ms triage of untrusted Portable Executable (PE32/PE32+) binaries.

The application couples two layers:
1. **Low-Level Analysis Core:** Bare-metal C++20, zero-copy memory-mapped file I/O (`CreateFileMappingW` / `MapViewOfFile`), linear instruction decoding via Zydis, and lookup-table Shannon entropy calculation.
2. **User Interface:** A native hardware-accelerated desktop application window rendered via Direct2D/DirectWrite and Electron Liquid Glassmorphism (`WS_OVERLAPPEDWINDOW` with minimize, maximize, and full-screen controls). Features the animated Koltzi Ghost mascot, a 7-stage verifiable forensic execution graph, and an interactive Reverse Engineering Knowledge Base.

* **Single Standalone Binary:** Portable executable under **1.1 MB** (static CRT `/MT`, zero external runtime dependencies).
* **Memory Footprint:** Idle memory usage below **12 MB RAM**.
* **Analysis Latency:** Under 15 ms on typical binaries (verified on Windows system binaries).
* **Offline Privacy:** Zero cloud telemetry, zero network calls.

---

## 2. Directory Structure

```
Koltzi/
├── assets/                     # Ghost mascot icon assets (SVG, PNG, ICO)
│   ├── ghost-icon.svg          # Vector mascot graphic
│   ├── ghost-icon.png          # High-resolution raster icon (512x512)
│   └── koltzi.ico              # Multi-resolution Windows application icon
├── CMakeLists.txt              # CMake configuration with static CRT (/MT), bcrypt, wintrust & Zydis
├── FONT/                       # Creato Display font family (Thin to Black, SIL OFL 1.1)
├── frontend/                   # Liquid Morphism workbench UI & Ghost companion engine
│   ├── index.html              # Workbench shell with 6 specialized tabs
│   ├── style.css               # Glassmorphism design system & theme palettes
│   ├── ghost.js                # Procedural ghost animation & mood state machine
│   ├── app.js                  # Frontend triage renderer & attack chain graph
│   ├── main.js                 # Electron desktop wrapper with native window controls
│   ├── icon.svg                # Browser & titlebar vector icon
│   └── icon.png                # Window frame icon
├── res/
│   ├── resource.h              # Resource IDs for application icon and embedded fonts
│   ├── Koltzi.rc               # Windows resource script embedding koltzi.ico and fonts
│   └── koltzi.ico              # Embedded executable icon resource
├── src/
│   ├── Main.cpp                # WinMain entry point, Per-Monitor DPI V2 & COM initialization
│   ├── Common.h                # System headers, string helpers, BCrypt/WinTrust headers & Direct2D macros
│   ├── Core/
│   │   ├── TriageReport.h      # Structured report models, hashes, cert info, and audit log entries
│   │   ├── PeReader.h/.cpp     # Zero-copy memory-mapped PE parser with 256-bin Shannon entropy
│   │   ├── CryptoVerifier.h/.cpp# Antivirus-grade MD5, SHA-1, SHA-256, Imphash, and Authenticode WinVerifyTrust
│   │   ├── InstructionScanner  # Zydis linear sweeper with CRT TLS and crypto loop disambiguation
│   │   ├── StringScanner.h/.cpp# Targeted artifact extractor with browser profile context suppression
│   │   └── ThreatAssessor.h/.cpp# 7-stage attack chain synthesis, trust discount & audit logging
│   ├── UI/
│   │   ├── AnimationTypes.h    # State machine (IDLE, SNIFFING, ALARMED, PUZZLED, HAPPY) & particles
│   │   ├── FontManager.h/.cpp  # Embedded/disk Creato Display typography manager & DirectWrite formats
│   │   ├── GhostRenderer.h/.cpp# Procedural Direct2D geometry, Bézier wave ripples, expressive eyes
│   │   ├── SpeechBubble.h/.cpp # Glassmorphism speech card, typewriter effect, expandable HUD
│   │   └── GhostWindow.h/.cpp  # Resizable Win32 desktop window, dual-view dashboard & audit log
│   └── App/
│       ├── Application.h/.cpp  # Asynchronous worker thread (std::jthread) & sample generators
└── tests/
    └── TestRunner.cpp          # Automated test suite (14 tests) & command-line live file triage
```

---

## 3. Triage Heuristics & Findings Matrix

| Detection Rule | Technical Pattern | Mascot Emotion | Mascot Assessment |
| :--- | :--- | :--- :--- | :--- |
| **Verified Publisher** | Valid Authenticode digital signature by trusted vendor (Mozilla, Google, Microsoft, Apple, Valve, Brave) | `HAPPY` | *"Verified publisher! Digitally signed by [Vendor]. Standard imports and cryptographic routines verified with zero exfiltration indicators."* |
| **Packed / Cryptic** | Section entropy $H > 7.20$ or total file entropy $> 7.80$ in executable code | `PUZZLED` | *"Whoa! This file is wrapped in thick encryption or packed like a mummy! I can't read the functions inside without running it. Be careful!"* |
| **Direct Syscalls + PEB Hashing** | Raw `syscall` (`0F 05`), `sysenter` (`0F 34`), PEB Ldr walking (`GS:[0x60]` / `FS:[0x30]`), ROR13 API hashing loop with stripped IAT | `ALARMED` | *"Sneaky sneaky! It's bypassing standard Windows libraries using direct syscalls and hiding its imports with PEB memory hashing. It's trying to ghost the antivirus!"* |
| **Credential Scraping** | Targeted DPAPI paths (`\Login Data`, `\Cookies`, `CryptUnprotectData`), Discord Webhooks, Telegram C2, Solana/MetaMask IDs | `ALARMED` | *"Red alert! Found hardcoded paths targeting your Chrome/Edge browser passwords and crypto wallets. Do NOT run this!"* |
| **Process Injection** | Cross-process chaining: `VirtualAllocEx` (RWX) -> `WriteProcessMemory` -> `CreateRemoteThread` / `QueueUserAPC` | `ALARMED` | *"Yikes! It's asking Windows to carve out executable memory in another process and pull the trigger! Classic process injection!"* |
| **Clean Binary** | Standard imports, legitimate section entropy, no evasion loops or stealer patterns | `HAPPY` | *"All clear! Normal imports, standard entropy, and no stealth injection loops. Looks like a friendly binary!"* |

---

## 4. Compilation

### Prerequisites
* Windows 10 or 11 x64
* MSVC v143+ (Visual Studio 2022 or Build Tools with C++20)
* CMake 3.20+ and Ninja

### Build Commands (Release Standalone Executable)
From a Visual Studio Developer Command Prompt:

```powershell
# Configure Release build
cmake -B build_rel -S . -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile and link single binary
cmake --build build_rel
```

The resulting binary `build_rel/Koltzi.exe` is:
* **Size:** ~1.05 MB
* **Runtime:** Fully static CRT (`/MT`)
* **Dependencies:** Standard Windows DLLs (`d2d1`, `dwrite`, `user32`, `gdi32`)

---

## 5. Controls & Interaction

### Graphical Mode
* **Standard Window Controls:** Native title bar with Minimize, Maximize / Full-Screen, and Close buttons (`WS_OVERLAPPEDWINDOW`). Fully resizable from 880x580 up to multi-monitor 4K with hardware-accelerated Direct2D scaling.
* **Toolbar Actions:** Clickable top toolbar buttons:
  * `[Open PE File...]` native file picker
  * Quick Malware Profiles: `[Clean PE]`, `[Packed]`, `[Syscall+PEB]`, `[Cred Stealer]`, `[Injection]`
* **Drag-and-Drop:** Drag any PE binary (`.exe`, `.dll`, `.sys`) directly into the window or onto the dedicated drop zone card. Triage executes asynchronously on a background worker thread (`std::jthread`), maintaining a locked 60 FPS animation loop.
* **Split-Pane Dashboard:**
  * **Left Pane:** Animated companion mascot reacting to threat mood, status badge, personality dialogue card, and drop target.
  * **Right Pane:** Low-level static telemetry dashboard including Target Overview, Threat Score progress meter, Shannon Entropy gauge, discrete Section Breakdown table, and detailed heuristic detections (Syscalls, PEB/TEB access, API hashing loops, Process injection primitives).
* **Right-Click Context Menu:** Context menu available anywhere within the window.

### CLI / Headless Triage Mode
```powershell
# Triage a file on disk from the command line
.\build_rel\KoltziTests.exe C:\Windows\System32\notepad.exe

# Execute unit test suite
.\build_rel\KoltziTests.exe
```

---

## 6. Test Suite Output

```text
========================================================
  KOLTZI AUTOMATED TEST SUITE
========================================================
[TEST] Running Shannon Entropy calculation tests...
  [PASS] Entropy calculation passed (Identical=0.00, Uniform=8.00).
[TEST] Running Clean Binary test...
  [PASS] Clean binary verdict: HAPPY / ALL CLEAR (Score: 0)
[TEST] Running Direct Syscall + PEB Hashing test...
  [PASS] Direct Syscall + PEB test passed: ALARMED / MALICIOUS (Syscalls: 1, PEB: 1, Score: 90)
[TEST] Running High Entropy / Packed Mummy test...
  [PASS] Packed Mummy test passed: PUZZLED (Section Entropy: 7.79 > 7.2)
[TEST] Running Credential Stealer Strings test...
  [PASS] Credential Stealer test passed: ALARMED / MALICIOUS (Strings: 4, Score: 65)
[TEST] Running Process Injection Chain test...
  [PASS] Process Injection test passed: ALARMED / MALICIOUS (Chained APIs: 3)
========================================================
  ALL TESTS PASSED WITH 100% SUCCESS!
========================================================
```
