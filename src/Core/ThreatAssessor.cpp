#include "ThreatAssessor.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Koltzi {

void ThreatAssessor::Assess(const PeReader& pe, TriageReport& report) {
    (void)pe;
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

    // 2. Evaluate PEB Accesses (filter out CRT TLS / security cookie initialization)
    size_t activePebAccesses = 0;
    for (const auto& peb : report.pebAccesses) {
        if (!peb.isCrtTlsInit) {
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
        redFlags.push_back(std::format("Evasive API hashing loops identified ({} dynamic resolution loops with stripped imports)", activeApiHashLoops));
        report.AddLog("ASSESS", "WARN", std::format("+25 pts: Evasive API hashing loops detected (count: {})", activeApiHashLoops));
    } else if (!report.apiHashLoops.empty()) {
        report.AddLog("ASSESS", "INFO", std::format("0 pts: {} rotation loops identified as legitimate cryptographic/cipher primitives", report.apiHashLoops.size()));
    }

    if (hasInjection) {
        score += 65;
        redFlags.push_back(report.injectionChain.description);
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
        score += 30;
        yellowFlags.push_back(std::format("High entropy executable section '{}' (H = {:.2f} > 7.2) - packed/encrypted code", packedSectionName, maxSectionEntropy));
        report.AddLog("ASSESS", "WARN", std::format("+30 pts: High entropy packed section '{}' (H = {:.2f})", packedSectionName, maxSectionEntropy));
    }

    // ----------------------------------------------------
    // Authenticode Digital Signature Trust Adjustment
    // ----------------------------------------------------
    if (report.signature.isValid) {
        if (report.signature.isTrustedVendor) {
            score = std::max(0, score - 60);
            report.AddLog("ASSESS", "PASS", "Trust discount: -60 pts applied for verified software publisher: " + report.signature.signerSubject);
            // If no active exfil or injection, force clean
            if (!hasExfil && !hasInjection && !hasEvasionCommands) {
                score = 0;
            }
        } else {
            score = std::max(0, score - 30);
            report.AddLog("ASSESS", "PASS", "Trust discount: -30 pts applied for valid Authenticode certificate");
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
    // Mascot Personality Dialogue
    // ----------------------------------------------------
    if (report.signature.isValid && report.signature.isTrustedVendor && report.threatLevel == ThreatLevel::Clean) {
        report.personalityDialogue =
            "Verified publisher! Digitally signed by " + report.signature.signerSubject +
            ". Standard imports and cryptographic routines verified with zero exfiltration indicators.";
    } else if (activePebAccesses > 0 && activeSyscalls > 0) {
        report.personalityDialogue =
            "Sneaky sneaky! It's bypassing standard Windows libraries using direct syscalls "
            "and hiding its imports with PEB memory hashing. It's trying to ghost the antivirus!";
    } else if (hasExfil || hasCredStealer || hasCryptoTarget) {
        report.personalityDialogue =
            "Red alert! Found hardcoded paths targeting your Chrome/Edge browser passwords "
            "and crypto wallets. Do NOT run this!";
    } else if (hasInjection) {
        report.personalityDialogue =
            "Yikes! It's asking Windows to carve out executable memory in another process "
            "and pull the trigger! Classic process injection!";
    } else if (activeSyscalls > 0) {
        report.personalityDialogue =
            "Suspicious! Raw direct syscalls (0F 05) found in the executable! "
            "Legitimate programs almost never do this unless they are trying to evade security hooks!";
    } else if (hasPackedCodeSection) {
        report.personalityDialogue =
            "Whoa! This file is wrapped in thick encryption or packed like a mummy! "
            "I can't read the functions inside without running it. Be careful!";
    } else if (hasEvasionCommands) {
        report.personalityDialogue =
            "Watch out! Found commands attempting to wipe Windows shadow copies or disable Defender!";
    } else {
        report.personalityDialogue =
            "All clear! Normal imports, standard entropy, and no stealth injection loops. "
            "Looks like a friendly binary!";
    }

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

    for (const auto& flag : redFlags) {
        report.technicalDetails.push_back("[CRITICAL] " + flag);
    }

    for (const auto& flag : yellowFlags) {
        report.technicalDetails.push_back("[WARNING] " + flag);
    }

    if (report.threatLevel == ThreatLevel::Clean) {
        report.technicalDetails.push_back("[INFO] No direct syscalls, no PEB stealth access, normal section entropy.");
        report.technicalDetails.push_back(std::format("[INFO] Imports verified: {} DLL dependencies loaded.", report.imports.size()));
    }
}

} // namespace Koltzi
