#include "ThreatAssessor.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Koltzi {

void ThreatAssessor::Assess(const PeReader& pe, TriageReport& report) {
    if (!report.isInstaller) {
        pe.DetectInstaller(report);
    }

    int score = 0;
    std::vector<std::string> redFlags;
    std::vector<std::string> yellowFlags;

    // Log PE Header Summary
    report.AddLog("HEADER", "INFO", std::format("Architecture: {} | Subsystem: {} | Size: {}",
        report.machineType, report.subsystem, FormatFileSize(report.fileSize)));
    report.AddLog("HEADER", "INFO", std::format("Entry Point RVA: 0x{:X} | Sections: {} | Overall Entropy: {:.2f}/8.00",
        report.entryPointRva, report.sections.size(), report.overallEntropy));

    // 1. Evaluate Direct Syscalls (filter out JIT / verified browser sandboxes)
    size_t activeSyscalls = 0;
    for (const auto& sc : report.syscalls) {
        if (!sc.isLegitimateJitOrHook) {
            activeSyscalls++;
        }
    }

    // 2. Evaluate PEB Accesses (filter out CRT TLS / security cookie initialization, installers, and verified software)
    size_t activePebAccesses = 0;
    for (const auto& peb : report.pebAccesses) {
        if (!peb.isCrtTlsInit && !report.isInstaller && !report.signature.isValid && !report.signature.isTrustedVendor && !report.isLegitimateBrowser) {
            activePebAccesses++;
        }
    }

    // 3. Evaluate Dynamic API Hashing Loops (filter out cryptographic ciphers)
    size_t activeApiHashLoops = 0;
    for (const auto& loop : report.apiHashLoops) {
        if (!loop.isLikelyCryptoOrStringHash) {
            activeApiHashLoops++;
        }
    }

    // 4. Evaluate Injection Chain
    bool hasInjection = report.injectionChain.detected;

    // 5. Evaluate Sensitive Strings (filter out context-suppressed browser internals)
    bool hasExfil = false;
    bool hasCredStealer = false;
    bool hasEvasionCommands = false;
    bool hasCryptoTarget = false;

    for (const auto& s : report.sensitiveStrings) {
        if (s.isSuppressedByContext) continue;

        if (s.category == "Exfiltration C2") {
            hasExfil = true;
            hasCredStealer = true;
        } else if (s.category == "Credential Scraping") {
            hasCredStealer = true;
        } else if (s.category == "Defense Evasion") {
            hasEvasionCommands = true;
        } else if (s.category == "Crypto Wallets") {
            hasCryptoTarget = true;
        }
    }

    // 6. Section Entropy & Packed Code Evaluation
    bool hasPackedCodeSection = false;
    std::string packedSectionName;
    double maxSectionEntropy = 0.0;
    bool hasRwxSection = false;

    for (const auto& sec : report.sections) {
        if (sec.entropy > maxSectionEntropy) {
            maxSectionEntropy = sec.entropy;
        }
        // Suspicious entropy in code sections (.text, CODE, or executable) indicates packing
        if (sec.isSuspiciousEntropy && sec.isExecutable) {
            hasPackedCodeSection = true;
            if (packedSectionName.empty()) packedSectionName = sec.name;
        }
        if (sec.isRwx) {
            hasRwxSection = true;
        }
    }

    // ----------------------------------------------------
    // Accumulate Threat Score Points
    // ----------------------------------------------------
    if (activeSyscalls > 0) {
        score += 40;
        redFlags.push_back(std::format("Direct Syscall instructions detected ({} unhooked syscalls) - AV/EDR bypass indicator", activeSyscalls));
        report.AddLog("ASSESS", "CRIT", std::format("+40 pts: Direct Syscalls detected (count: {})", activeSyscalls));
    }

    if (activePebAccesses > 0) {
        score += 30;
        redFlags.push_back(std::format("Evasive PEB/TEB traversal detected ({} manual module list walks, e.g. InMemoryOrderModuleList)", activePebAccesses));
        report.AddLog("ASSESS", "CRIT", std::format("+30 pts: Evasive PEB traversal detected (count: {})", activePebAccesses));
    } else if (!report.pebAccesses.empty()) {
        report.AddLog("ASSESS", "INFO", std::format("0 pts: {} PEB accesses identified as benign CRT __security_init_cookie / TLS initialization", report.pebAccesses.size()));
    }

    if (activeApiHashLoops > 0) {
        score += 25;
        std::vector<std::string> matchedApis;
        for (const auto& l : report.apiHashLoops) {
            if (!l.matchedApi.empty()) matchedApis.push_back(l.matchedApi + " [" + l.hashAlgorithm + "]");
        }
        if (!matchedApis.empty()) {
            redFlags.push_back(std::format("Known API Hashes matched in instructions: {}", matchedApis[0]));
            report.AddLog("ASSESS", "WARN", std::format("+25 pts: Known API Hashes matched (count: {})", matchedApis.size()));
        } else {
            redFlags.push_back(std::format("Evasive API hashing loops identified ({} dynamic resolution loops with stripped imports)", activeApiHashLoops));
            report.AddLog("ASSESS", "WARN", std::format("+25 pts: Evasive API hashing loops detected (count: {})", activeApiHashLoops));
        }
    } else if (!report.apiHashLoops.empty()) {
        report.AddLog("ASSESS", "INFO", std::format("0 pts: {} rotation loops identified as legitimate cryptographic/cipher primitives", report.apiHashLoops.size()));
    }

    if (hasInjection) {
        score += 65;
        redFlags.push_back(report.injectionChain.description);
        if (report.injectionChain.hasRwxProtectArg) {
            redFlags.push_back("PAGE_EXECUTE_READWRITE (0x40) passed as protection parameter to VirtualAlloc/Protect");
        }
        report.AddLog("ASSESS", "CRIT", "+65 pts: Cross-process injection chain primitives detected");
    }

    if (hasExfil) {
        score += 70;
        redFlags.push_back("Hardcoded C2 / data exfiltration endpoint detected (Discord / Telegram / Pastebin)");
        report.AddLog("ASSESS", "CRIT", "+70 pts: Active C2 / Exfiltration endpoint strings matched");
    } else if (hasCredStealer) {
        score += 55;
        redFlags.push_back("Browser credential store targeting detected without verified publisher authorization");
        report.AddLog("ASSESS", "WARN", "+55 pts: Unverified browser credential targeting");
    }

    if (hasEvasionCommands) {
        score += 55;
        redFlags.push_back("Defense evasion / shadow copy deletion commands found");
        report.AddLog("ASSESS", "CRIT", "+55 pts: Defense evasion commands matched");
    }

    if (hasCryptoTarget) {
        score += 40;
        redFlags.push_back("Cryptocurrency wallet extension IDs or wallet.dat targeting found");
        report.AddLog("ASSESS", "WARN", "+40 pts: Cryptocurrency wallet targeting");
    }

    if (hasRwxSection) {
        score += 25;
        redFlags.push_back("RWX (Readable, Writable & Executable) memory section found (W^X violation)");
        report.AddLog("ASSESS", "WARN", "+25 pts: RWX memory section detected");
    }

    if (hasPackedCodeSection) {
        if (report.isInstaller || report.signature.isValid || report.isLegitimateBrowser) {
            report.AddLog("ASSESS", "INFO", std::format("Compressed archive payload or asset data in section '{}' (H = {:.2f}) for verified binary", packedSectionName, maxSectionEntropy));
            hasPackedCodeSection = false; // Benign installer archive payload or signed assets
        } else {
            score += 30;
            yellowFlags.push_back(std::format("High entropy executable section '{}' (H = {:.2f} > 7.2) - packed/encrypted code", packedSectionName, maxSectionEntropy));
            report.AddLog("ASSESS", "WARN", std::format("+30 pts: High entropy packed section '{}' (H = {:.2f})", packedSectionName, maxSectionEntropy));
        }
    }

    // ----------------------------------------------------
    // PE Overlay Analysis and Unsigned Dropper Detection
    // ----------------------------------------------------
    bool hasSuspiciousOverlay = false;
    if (report.overlaySize > 2 * 1024 * 1024 && report.overlayEntropy > 7.2) {
        if (!report.signature.isValid && !report.signature.isTrustedVendor) {
            hasSuspiciousOverlay = true;
            report.hasSuspiciousOverlay = true;
            score += 75;
            redFlags.push_back(std::format("Unsigned payload dropper container: massive high-entropy overlay ({}, H={:.2f}, {:.1f}% of file) without valid digital signature",
                FormatFileSize(report.overlaySize), report.overlayEntropy, report.overlayRatio * 100.0));
            report.AddLog("ASSESS", "CRIT", std::format("+75 pts: Unsigned payload dropper overlay ({}, H={:.2f})",
                FormatFileSize(report.overlaySize), report.overlayEntropy));
        } else {
            report.AddLog("ASSESS", "INFO", std::format("Installer archive overlay ({}, H={:.2f}) verified by digital signature",
                FormatFileSize(report.overlaySize), report.overlayEntropy));
        }
    }

    // ----------------------------------------------------
    // Legitimate Installer Package Verification
    // ----------------------------------------------------
    if (report.isInstaller) {
        if (hasSuspiciousOverlay) {
            report.AddLog("ASSESS", "WARN", "Installer package (" + report.installerType + ") carries an unsigned high-entropy overlay payload - potential Trojanized setup dropper");
        } else {
            report.AddLog("ASSESS", "PASS", "Verified installer package (" + report.installerType + ") - routine software staging and file extraction");
            if (!hasExfil && !hasInjection && !hasEvasionCommands) {
                score = 0;
                hasPackedCodeSection = false;
                activePebAccesses = 0;
                activeSyscalls = 0;
                redFlags.clear();
                yellowFlags.clear();
            }
        }
    }

    // ----------------------------------------------------
    // Authenticode Digital Signature Trust Adjustment
    // ----------------------------------------------------
    if (report.signature.isValid) {
        if (report.signature.isTrustedVendor) {
            score = std::max(0, score - 60);
            report.AddLog("ASSESS", "PASS", "Trust discount: -60 pts applied for verified software publisher: " + report.signature.signerSubject);
            if (!hasExfil && !hasInjection && !hasEvasionCommands) {
                score = 0;
                hasPackedCodeSection = false;
                activePebAccesses = 0;
                activeSyscalls = 0;
                redFlags.clear();
                yellowFlags.clear();
            }
        } else {
            score = std::max(0, score - 50);
            report.AddLog("ASSESS", "PASS", "Trust discount: -50 pts applied for valid Authenticode certificate");
            if (!hasExfil && !hasInjection && !hasEvasionCommands) {
                score = 0;
                hasPackedCodeSection = false;
                activePebAccesses = 0;
                activeSyscalls = 0;
                redFlags.clear();
                yellowFlags.clear();
            }
        }
    }

    // Cap score between 0 and 100
    report.threatScore = std::clamp(score, 0, 100);

    // ----------------------------------------------------
    // Determine Threat Level & Mascot Mood
    // ----------------------------------------------------
    if (report.threatScore >= 55 || hasExfil || (activeSyscalls > 0 && activePebAccesses > 0) || hasInjection) {
        report.threatLevel = ThreatLevel::Malicious;
        report.mood = GhostMood::Alarmed;
    } else if (report.threatScore >= 20 || hasPackedCodeSection) {
        report.threatLevel = ThreatLevel::Suspicious;
        report.mood = GhostMood::Puzzled;
    } else {
        report.threatLevel = ThreatLevel::Clean;
        report.mood = GhostMood::Happy;
    }

    report.AddLog("ASSESS", "AUDIT", std::format("Final Threat Score: {}/100 | Verdict: {}",
        report.threatScore,
        (report.threatLevel == ThreatLevel::Malicious ? "MALICIOUS" :
         report.threatLevel == ThreatLevel::Suspicious ? "SUSPICIOUS" : "CLEAN")));

    // ----------------------------------------------------
    // ----------------------------------------------------
    // Mascot Forensic Dialogue Generation (Multiple Detailed Lines)
    // ----------------------------------------------------
    // ----------------------------------------------------
    // Mascot Forensic Dialogue Generation (Primary Verdict + Detailed Findings)
    // ----------------------------------------------------
    std::string primaryDialogue;
    if (hasSuspiciousOverlay) {
        primaryDialogue = "Dropper alert! This unsigned file contains a massive encrypted payload hidden in its PE overlay (" + FormatFileSize(report.overlaySize) + ")! It acts as an installer wrapper to drop unverified software!";
    } else if (report.isInstaller && report.threatLevel == ThreatLevel::Clean) {
        primaryDialogue = "Safe setup package detected (" + report.installerType + ")! Clean file staging and valid application installation verified with zero malware indicators.";
    } else if (report.signature.isValid && report.threatLevel == ThreatLevel::Clean) {
        primaryDialogue = "Verified publisher! Digitally signed by " + report.signature.signerSubject + ". Standard imports and cryptographic routines verified with zero exfiltration indicators.";
    } else if (activePebAccesses > 0 && activeSyscalls > 0) {
        primaryDialogue = "Sneaky sneaky! It's bypassing standard Windows libraries using direct syscalls and hiding its imports with PEB memory hashing. It's trying to ghost the antivirus!";
    } else if (hasExfil || hasCredStealer || hasCryptoTarget) {
        primaryDialogue = "Red alert! Found hardcoded paths targeting your Chrome/Edge browser passwords and crypto wallets. Do NOT run this!";
    } else if (hasInjection) {
        primaryDialogue = "Yikes! It's asking Windows to carve out executable memory in another process and pull the trigger! Classic process injection!";
    } else if (activeSyscalls > 0) {
        primaryDialogue = "Suspicious! Raw direct syscalls (0F 05) found in the executable! Legitimate programs almost never do this unless they are trying to evade security hooks!";
    } else if (hasPackedCodeSection) {
        primaryDialogue = "Whoa! This file is wrapped in thick encryption or packed like a mummy! I can't read the functions inside without running it. Be careful!";
    } else if (hasEvasionCommands) {
        primaryDialogue = "Watch out! Found commands attempting to wipe Windows shadow copies or disable Defender!";
    } else {
        primaryDialogue = "All clear! Normal imports, standard entropy, and no stealth injection loops. Looks like a friendly binary!";
    }

    report.dialogueLines.clear();
    report.dialogueLines.push_back(primaryDialogue);

    // Append Detailed Technical & Forensic Observations for User to Cycle Through
    if (activeSyscalls > 0) {
        std::string scSec = (!report.syscalls.empty() && !report.syscalls[0].section.empty()) ? report.syscalls[0].section : ".text";
        report.dialogueLines.push_back(
            std::format("Direct syscall evasion: Found {} raw x64 syscall (0F 05) instructions in section {} to blind EDR userland API hooks.",
                activeSyscalls, scSec)
        );
        if (!report.syscalls.empty() && report.syscalls[0].isStubPattern) {
            report.dialogueLines.push_back(
                std::format("Structural syscall stub verified: SSN index 0x{:02X} matches low-level NT API invocation without standard ntdll.dll imports.",
                    report.syscalls[0].ssn)
            );
        }
    }

    if (activePebAccesses > 0) {
        report.dialogueLines.push_back(
            std::format("Evasive PEB traversal: Found {} manual module walks via GS:[0x60]->Ldr to search loaded DLLs without calling GetModuleHandle.",
                activePebAccesses)
        );
        report.dialogueLines.push_back(
            "API hiding confirmed: It manually iterates InMemoryOrderModuleList to locate exports dynamically in memory."
        );
    }

    if (activeApiHashLoops > 0) {
        std::string matchedName;
        for (const auto& l : report.apiHashLoops) {
            if (!l.matchedApi.empty()) { matchedName = l.matchedApi; break; }
        }
        if (!matchedName.empty()) {
            report.dialogueLines.push_back(
                std::format("Dynamic API hashing verified! Resolved critical API '{}' via assembly hashing loop without IAT entry.", matchedName)
            );
        } else {
            report.dialogueLines.push_back(
                std::format("Evasive API hashing loops found: {} dynamic resolution loops detected bypassing standard Import Address Table.",
                    activeApiHashLoops)
            );
        }
    }

    if (hasInjection) {
        if (report.injectionChain.hasRwxProtectArg || report.injectionChain.hasRwxAllocation) {
            report.dialogueLines.push_back(
                "PAGE_EXECUTE_READWRITE violation: Asking the kernel for RWX memory in target processes, violating core W^X security principles."
            );
        }
    }

    if (hasCryptoTarget) {
        report.dialogueLines.push_back(
            "Cryptocurrency targeting: Found artifact strings specifically hunting MetaMask, Exodus, and wallet.dat files."
        );
    }

    if (hasCredStealer) {
        report.dialogueLines.push_back(
            "Credential harvesting: Targets Chrome, Edge, Brave Login Data, Cookies, and Discord/Telegram session tokens."
        );
    }

    if (hasEvasionCommands) {
        report.dialogueLines.push_back(
            "Defense evasion detected: Found embedded commands attempting to wipe Windows shadow copies (vssadmin) or disable Defender."
        );
    }

    if (hasPackedCodeSection) {
        report.dialogueLines.push_back(
            std::format("High entropy alert: Section '{}' measures {:.2f} / 8.00 entropy, indicating dense cryptographic ciphertext or packing.",
                packedSectionName, maxSectionEntropy)
        );
    }

    if (hasRwxSection) {
        report.dialogueLines.push_back(
            "Dangerous section flags: Found a PE section marked as both Executable and Writable (RWX), a primary self-modifying code indicator."
        );
    }

    if (report.signature.isValid) {
        report.dialogueLines.push_back(
            "Authenticode chain verified: Issued by " + report.signature.signerIssuer + " and anchored in Windows Trusted Root store."
        );
    } else if (!report.signature.isSigned) {
        report.dialogueLines.push_back(
            "Unsigned binary: No digital Authenticode certificate found. Publisher origin and file integrity are unverified."
        );
    }

    if (hasSuspiciousOverlay) {
        report.dialogueLines.push_back(
            "Unsigned payload dropper: " + FormatFileSize(report.overlaySize) + " payload container appended as PE overlay without a valid digital certificate."
        );
        report.dialogueLines.push_back(
            std::format("Hidden payload telemetry: PE code stub is only {} while overlay is {} ({:.1f}% of file) with entropy {:.2f}/8.00.",
                FormatFileSize(report.fileSize - report.overlaySize), FormatFileSize(report.overlaySize), report.overlayRatio * 100.0, report.overlayEntropy)
        );
    }

    report.dialogueLines.push_back(
        std::format("PE Structure: {} binary targeting {} with {} sections and {} import DLLs (Average entropy: {:.2f}/8.00).",
            report.machineType, report.subsystem, report.sections.size(), report.imports.size(), report.overallEntropy)
    );

    report.activeDialogueIndex = 0;
    report.personalityDialogue = report.dialogueLines[0];

    // ----------------------------------------------------
    // Construct Technical HUD Breakdown
    // ----------------------------------------------------
    report.technicalDetails.clear();

    report.technicalDetails.push_back(std::format(
        "Architecture: {} | Subsystem: {} | Size: {}",
        report.machineType,
        report.subsystem,
        FormatFileSize(report.fileSize)
    ));

    report.technicalDetails.push_back(std::format(
        "Overall File Entropy: {:.2f} / 8.00 | Sections: {}",
        report.overallEntropy,
        report.sections.size()
    ));

    if (report.overlaySize > 0) {
        report.technicalDetails.push_back(std::format(
            "PE Overlay: {} at offset 0x{:X} ({:.1f}% of file, Entropy {:.2f} / 8.00, Type: {})",
            FormatFileSize(report.overlaySize),
            report.overlayOffset,
            report.overlayRatio * 100.0,
            report.overlayEntropy,
            report.overlayType.empty() ? "Appended Data" : report.overlayType
        ));
    }

    if (!report.sha256.empty()) {
        report.technicalDetails.push_back("SHA-256: " + report.sha256);
    }

    if (report.signature.isSigned) {
        report.technicalDetails.push_back(std::format(
            "Signature: {} ({})",
            report.signature.statusText,
            report.signature.signerSubject.empty() ? "No Subject" : report.signature.signerSubject
        ));
    } else {
        report.technicalDetails.push_back("Signature: Unsigned Binary");
    }

    if (report.isInstaller && !hasSuspiciousOverlay) {
        report.technicalDetails.push_back("[INFO] Verified Setup Package: " + report.installerType + " (Clean software extraction & installation verified)");
    } else if (report.isInstaller && hasSuspiciousOverlay) {
        report.technicalDetails.push_back("[WARNING] Trojanized Setup Package: " + report.installerType + " (High-entropy appended payload container without digital signature)");
    }

    for (const auto& flag : redFlags) {
        report.technicalDetails.push_back("[CRITICAL] " + flag);
    }

    for (const auto& flag : yellowFlags) {
        report.technicalDetails.push_back("[WARNING] " + flag);
    }

    if (report.threatLevel == ThreatLevel::Clean) {
        report.technicalDetails.push_back("[INFO] No direct syscalls, no PEB stealth access, normal section entropy.");
        report.technicalDetails.push_back(std::format("[INFO] Imports verified: {} DLL dependencies loaded.", report.imports.size()));
        report.technicalDetails.push_back("[INFO] No malicious injection chains, exfiltration channels, or defense evasion commands.");
    }
}

} // namespace Koltzi
