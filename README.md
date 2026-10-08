# Koltzi
> **Zero-Bloat Desktop Malware Triage Agent**  
> *Sub-100ms PE Static Analysis with a Lightweight Direct2D Floating Mascot*

[![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20x64-brightgreen.svg)]()
[![Binary Size](https://img.shields.io/badge/Binary%20Size-1.05%20MB%20(Single%20EXE)-orange.svg)]()
[![Telemetry](https://img.shields.io/badge/Telemetry-0%25%20(100%25%20Offline)-success.svg)]()

---

## 1. Overview & Architecture
Koltzi is an offline Windows desktop utility designed for sub-100ms triage of untrusted Portable Executable (PE32/PE32+) binaries.

The application couples two layers:
1. **Low-Level Analysis Core:** Bare-metal C++20, zero-copy memory-mapped file I/O (`CreateFileMappingW` / `MapViewOfFile`), linear instruction decoding via Zydis, and lookup-table Shannon entropy calculation.
2. **User Interface:** A borderless desktop companion rendered via Direct2D/DirectWrite with per-pixel alpha transparency that analyzes dropped binaries and presents plain-English threat assessments alongside an expandable technical HUD.

* **Single Standalone Binary:** Portable executable under **1.05 MB** (static CRT `/MT`, zero external runtime dependencies).
* **Memory Footprint:** Idle memory usage below **10 MB RAM**.
* **Analysis Latency:** Under 15 ms on typical binaries (verified on Windows system binaries).
* **Offline Privacy:** Zero cloud telemetry, zero network calls.

---

## 2. Directory Structure

```
Koltzi/
├── CMakeLists.txt              # CMake configuration with static CRT (/MT) & Zydis FetchContent
├── src/
│   ├── Main.cpp                # WinMain entry point, Per-Monitor DPI V2 & COM initialization
│   ├── Common.h                # System headers, string helpers, and Direct2D safe-release macros
│   ├── Core/
│   │   ├── TriageReport.h      # Structured report models, threat scores, and finding types
│   │   ├── PeReader.h/.cpp     # Zero-copy memory-mapped PE parser with 256-bin Shannon entropy
│   │   ├── InstructionScanner  # Zydis-based linear sweeper (Syscalls, PEB, API Hashing, Injections)
│   │   ├── StringScanner.h/.cpp# Targeted artifact extractor (DPAPI, Webhooks, Evasion commands)
│   │   └── ThreatAssessor.h/.cpp# Scoring engine, mood transitions, and mascot dialogue matrix
│   ├── UI/
│   │   ├── AnimationTypes.h    # State machine (IDLE, SNIFFING, ALARMED, PUZZLED, HAPPY) & particles
│   │   ├── GhostRenderer.h/.cpp# Procedural Direct2D geometry, Bézier wave ripples, expressive eyes
│   │   ├── SpeechBubble.h/.cpp # Glassmorphism speech card, typewriter effect, expandable HUD
│   │   └── GhostWindow.h/.cpp  # Layered window (UpdateLayeredWindow), drag-and-drop, context menu
│   └── App/
│       ├── Application.h/.cpp  # Asynchronous worker thread (std::jthread) & sample generators
└── tests/
    └── TestRunner.cpp          # Automated test suite & command-line live file triage
```

---

## 3. Triage Heuristics & Findings Matrix

| Detection Rule | Technical Pattern | Mascot Emotion | Mascot Assessment |
| :--- | :--- | :---: | :--- |
| **Packed / Cryptic** | Section entropy $H > 7.20$ or total file entropy $> 7.80$ | `PUZZLED` | *"Whoa! This file is wrapped in thick encryption or packed like a mummy! I can't read the functions inside without running it. Be careful!"* |
| **Direct Syscalls + PEB Hashing** | Raw `syscall` (`0F 05`), `sysenter` (`0F 34`), PEB access (`GS:[0x60]` / `FS:[0x30]`), ROR13 API hashing loop | `ALARMED` | *"Sneaky sneaky! It's bypassing standard Windows libraries using direct syscalls and hiding its imports with PEB memory hashing. It's trying to ghost the antivirus!"* |
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
* **Drag-and-Drop:** Drag any PE binary (`.exe`, `.dll`, `.sys`) onto the companion window. Triage runs asynchronously on a dedicated worker thread (`std::jthread`), ensuring the 60 FPS animation loop is never blocked.
* **Repositioning:** Left-click and drag the companion to reposition it on the screen.
* **Expand Technical Findings HUD:** Left-click the speech bubble or press `Tab`/`Space` to expand low-level details (section table, entropy gauges, threat meter, and detection breakdown).
* **Right-Click Context Menu:**
  * Analyze PE Binary File... (native open file dialog)
  * Test Profile: Clean PE (Standard Imports)
  * Test Profile: High Entropy / Packed Code
  * Test Profile: Direct Syscalls + PEB Hashing
  * Test Profile: Credential Scraping Artifacts
  * Test Profile: Process Injection Chain
  * Toggle Technical Findings HUD
  * Reset Mascot to Idle
  * Exit Koltzi

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
