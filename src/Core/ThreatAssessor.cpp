#include "ThreatAssessor.h"
#include <sstream>
#include <iomanip>

namespace Koltzi {

void ThreatAssessor::Assess(const PeReader& pe, TriageReport& report) {
    (void)pe;
    int score = 0;
    std::vector<std::string> redFlags;
    std::vector<std::string> yellowFlags;

    bool hasSyscalls = !report.syscalls.empty();
    bool hasPebAccess = !report.pebAccesses.empty();
    bool hasApiHashLoops = !report.apiHashLoops.empty();
    bool hasInjection = report.injectionChain.detected;

    // Check credentials & exfiltration
    bool hasCredStealer = false;
    bool hasEvasionCommands = false;
    bool hasCryptoTarget = false;
    for (const auto& s : report.sensitiveStrings) {
        if (s.category == "Credential Scraping" || s.category == "Exfiltration C2") {
            hasCredStealer = true;
        } else if (s.category == "Defense Evasion") {
            hasEvasionCommands = true;
        } else if (s.category == "Crypto Wallets") {
            hasCryptoTarget = true;
        }
    }

    // Check packed sections / high entropy
    bool hasPackedSection = false;
    std::string packedSectionName;
    double maxSectionEntropy = 0.0;
    bool hasRwxSection = false;

    for (const auto& sec : report.sections) {
        if (sec.entropy > maxSectionEntropy) {
            maxSectionEntropy = sec.entropy;
        }
        if (sec.isSuspiciousEntropy) {
            hasPackedSection = true;
            if (packedSectionName.empty()) packedSectionName = sec.name;
        }
        if (sec.isRwx) {
            hasRwxSection = true;
        }
    }

    // Scoring weights
    if (hasSyscalls) {
        score += 40;
        redFlags.push_back(std::format("Direct Syscall instructions detected ({} found) - AV/EDR hook bypass indicator", report.syscalls.size()));
    }

    if (hasPebAccess) {
        score += 25;
        redFlags.push_back(std::format("Manual PEB/TEB traversal detected ({} accesses, e.g. FS:[0x30]/GS:[0x60])", report.pebAccesses.size()));
    }

    if (hasApiHashLoops) {
        score += 25;
        redFlags.push_back(std::format("API hashing loops identified ({} dynamic resolution loops detected)", report.apiHashLoops.size()));
    }

    if (hasInjection) {
        score += 65;
        redFlags.push_back(report.injectionChain.description);
    }

    if (hasCredStealer) {
        score += 65;
        redFlags.push_back("Hardcoded browser credential scraping paths or exfiltration endpoints detected");
    }

    if (hasEvasionCommands) {
        score += 55;
        redFlags.push_back("Defense evasion / shadow copy deletion commands found");
    }

    if (hasCryptoTarget) {
        score += 40;
        redFlags.push_back("Cryptocurrency wallet extension IDs or wallet.dat targeting found");
    }

    if (hasRwxSection) {
        score += 25;
        redFlags.push_back("RWX (Readable, Writable & Executable) memory section found (W^X violation)");
    }

    if (hasPackedSection) {
        score += 30;
        yellowFlags.push_back(std::format("High entropy section '{}' (H = {:.2f} > 7.2) - packed/encrypted code", packedSectionName, maxSectionEntropy));
    }

    // Cap score at 100
    report.threatScore = std::min(100, score);

    // Determine Threat Level & Mascot Mood
    if (report.threatScore >= 55 || hasCredStealer || (hasSyscalls && hasPebAccess) || hasInjection) {
        report.threatLevel = ThreatLevel::Malicious;
        report.mood = GhostMood::Alarmed;
    } else if (report.threatScore >= 20 || hasPackedSection) {
        report.threatLevel = ThreatLevel::Suspicious;
        report.mood = GhostMood::Puzzled;
    } else {
        report.threatLevel = ThreatLevel::Clean;
        report.mood = GhostMood::Happy;
    }

    // Translate to Plain-English Personality Dialogue
    // Prioritize specific high-fidelity match rules from brief
    if (hasPebAccess && hasSyscalls) {
        report.personalityDialogue =
            "Sneaky sneaky! It's bypassing standard Windows libraries using direct syscalls "
            "and hiding its imports with PEB memory hashing. It's trying to ghost the antivirus!";
    } else if (hasCredStealer || hasCryptoTarget) {
        report.personalityDialogue =
            "Red alert! Found hardcoded paths targeting your Chrome/Edge browser passwords "
            "and crypto wallets. Do NOT run this!";
    } else if (hasInjection) {
        report.personalityDialogue =
            "Yikes! It's asking Windows to carve out executable memory in another process "
            "and pull the trigger! Classic process injection!";
    } else if (hasSyscalls) {
        report.personalityDialogue =
            "Suspicious! Raw direct syscalls (0F 05) found in the executable! "
            "Legitimate programs almost never do this unless they are trying to evade security hooks!";
    } else if (hasPackedSection) {
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

    // Construct Technical HUD Breakdown
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
