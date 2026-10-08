#include "StringScanner.h"
#include <algorithm>
#include <unordered_set>

namespace Koltzi {

StringScanner::StringScanner() {
    // DPAPI & Browser Credentials
    m_targets.push_back({ "Credential Scraping", "\\Login Data", "Chrome/Edge Saved Passwords SQLite path" });
    m_targets.push_back({ "Credential Scraping", "\\Web Data", "Autofill & Credit Card DB path" });
    m_targets.push_back({ "Credential Scraping", "\\Cookies", "Browser Session Cookies DB path" });
    m_targets.push_back({ "Credential Scraping", "\\Local State", "Chrome/Edge Master Key DPAPI path" });
    m_targets.push_back({ "Credential Scraping", "\\key4.db", "Firefox Master Key database" });
    m_targets.push_back({ "Credential Scraping", "\\logins.json", "Firefox Saved Logins file" });
    m_targets.push_back({ "Credential Scraping", "CryptUnprotectData", "DPAPI Decryption API reference" });
    m_targets.push_back({ "Credential Scraping", "VaultEnumerateItems", "Windows Credential Manager enum" });
    m_targets.push_back({ "Credential Scraping", "Google\\Chrome\\User Data", "Chrome user profile directory" });
    m_targets.push_back({ "Credential Scraping", "Microsoft\\Edge\\User Data", "Edge user profile directory" });

    // Exfiltration & C2 Endpoints
    m_targets.push_back({ "Exfiltration C2", "discord.com/api/webhooks", "Discord Webhook Exfiltration API" });
    m_targets.push_back({ "Exfiltration C2", "discordapp.com/api/webhooks", "DiscordApp Webhook Exfiltration API" });
    m_targets.push_back({ "Exfiltration C2", "api.telegram.org/bot", "Telegram Bot Exfiltration API" });
    m_targets.push_back({ "Exfiltration C2", "pastebin.com/raw", "Pastebin Raw C2 / Payload Dropper" });
    m_targets.push_back({ "Exfiltration C2", "webhook.site", "Webhook.site Exfiltration Endpoint" });
    m_targets.push_back({ "Exfiltration C2", "transfer.sh", "Transfer.sh file drop endpoint" });

    // Defense Evasion & Ransomware
    m_targets.push_back({ "Defense Evasion", "vssadmin delete shadows", "VSS Shadow Copies Deletion (Ransomware)" });
    m_targets.push_back({ "Defense Evasion", "Set-MpPreference -DisableRealtimeMonitoring", "Windows Defender Real-time disable" });
    m_targets.push_back({ "Defense Evasion", "bcdedit /set {default} recoveryenabled no", "Disable Windows Recovery Mode" });
    m_targets.push_back({ "Defense Evasion", "bootstatuspolicy ignoreallfailures", "Ignore Boot Failures" });
    m_targets.push_back({ "Defense Evasion", "wevtutil cl", "Windows Event Log wiping" });
    m_targets.push_back({ "Defense Evasion", "net stop WinDefend", "Stop Windows Defender service" });
    m_targets.push_back({ "Defense Evasion", "wmic shadowcopy delete", "WMIC Shadow Copies Deletion" });

    // Crypto Wallets
    m_targets.push_back({ "Crypto Wallets", "nkbihfbeogaeaoehlefnkodbefgpgknn", "MetaMask Wallet Extension ID" });
    m_targets.push_back({ "Crypto Wallets", "ibnejkighgahppapaganajkkpnagflqq", "Solana Wallet Extension ID" });
    m_targets.push_back({ "Crypto Wallets", "wallet.dat", "Bitcoin Core wallet file" });
    m_targets.push_back({ "Crypto Wallets", "Exodus\\exodus.wallet", "Exodus Crypto Wallet storage" });

    // Sandbox / Anti-Analysis Evasion
    m_targets.push_back({ "Sandbox Evasion", "SbieDll.dll", "Sandboxie detection artifact" });
    m_targets.push_back({ "Sandbox Evasion", "VBoxGuestAdditions", "VirtualBox VM artifact" });
    m_targets.push_back({ "Sandbox Evasion", "vmtoolsd.exe", "VMware Tools artifact" });
}

StringScanner::~StringScanner() = default;

static bool CaseInsensitiveContains(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) return false;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return ::tolower(ch1) == ::tolower(ch2); }
    );
    return it != haystack.end();
}

bool StringScanner::Scan(const PeReader& pe, TriageReport& report) {
    const uint8_t* base = pe.GetBaseAddress();
    const size_t fileSize = pe.GetFileSize();
    if (!base || fileSize < 4) return false;

    std::unordered_set<std::string> seenPatterns;

    // Determine if binary has legitimate browser identity or trusted publisher
    bool isTrustedBrowserContext = report.isLegitimateBrowser ||
        (report.signature.isValid && report.signature.isTrustedVendor);

    auto ProcessMatch = [&](const TargetPattern& target, size_t startOffset, bool isUtf16) {
        StringFinding sf;
        sf.category = target.category;
        sf.offset = startOffset;
        sf.isUtf16 = isUtf16;

        // Contextual disambiguation:
        // Legitimate browsers contain internal references to their own profiles and storage paths.
        // If the binary is verified as signed by a trusted vendor and is a browser component,
        // suppress credential scraping alerts for database paths (unless exfil endpoints exist).
        if (target.category == "Credential Scraping" && isTrustedBrowserContext) {
            sf.isSuppressedByContext = true;
            sf.matchedPattern = target.pattern + " (" + target.displayDescription + ") [Suppressed: Verified Publisher context]";
            report.AddLog("STRINGS", "INFO", "Suppressed browser internal profile path for verified publisher: " + target.pattern);
        } else {
            sf.isSuppressedByContext = false;
            sf.matchedPattern = target.pattern + " (" + target.displayDescription + ")";
            if (target.category == "Exfiltration C2") {
                report.AddLog("STRINGS", "CRIT", "C2 / Exfiltration endpoint string matched: " + target.pattern);
            } else if (target.category == "Defense Evasion") {
                report.AddLog("STRINGS", "CRIT", "Defense evasion command matched: " + target.pattern);
            } else {
                report.AddLog("STRINGS", "WARN", "Sensitive target string matched: " + target.pattern);
            }
        }

        report.sensitiveStrings.push_back(std::move(sf));
    };

    // Scan UTF-8 / ASCII strings
    {
        std::string currentStr;
        size_t startOffset = 0;

        for (size_t i = 0; i < fileSize; ++i) {
            char c = static_cast<char>(base[i]);
            if ((c >= 32 && c <= 126) || c == '\t') {
                if (currentStr.empty()) startOffset = i;
                currentStr.push_back(c);
            } else {
                if (currentStr.size() >= 4) {
                    for (const auto& target : m_targets) {
                        if (seenPatterns.find(target.pattern) == seenPatterns.end() &&
                            CaseInsensitiveContains(currentStr, target.pattern)) {
                            seenPatterns.insert(target.pattern);
                            ProcessMatch(target, startOffset, false);
                        }
                    }
                }
                currentStr.clear();
            }
        }
    }

    // Scan UTF-16LE strings (common in modern Windows binaries & malware)
    {
        std::string currentStr;
        size_t startOffset = 0;

        for (size_t i = 0; i + 1 < fileSize; i += 2) {
            uint8_t low = base[i];
            uint8_t high = base[i + 1];

            if (high == 0 && ((low >= 32 && low <= 126) || low == '\t')) {
                if (currentStr.empty()) startOffset = i;
                currentStr.push_back(static_cast<char>(low));
            } else {
                if (currentStr.size() >= 4) {
                    for (const auto& target : m_targets) {
                        if (seenPatterns.find(target.pattern) == seenPatterns.end() &&
                            CaseInsensitiveContains(currentStr, target.pattern)) {
                            seenPatterns.insert(target.pattern);
                            ProcessMatch(target, startOffset, true);
                        }
                    }
                }
                currentStr.clear();
            }
        }
    }

    report.AddLog("STRINGS", "PASS", "String inspection complete: " + std::to_string(report.sensitiveStrings.size()) + " signature patterns processed");

    return true;
}

} // namespace Koltzi
