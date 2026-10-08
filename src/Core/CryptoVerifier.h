#pragma once
#include "../Common.h"
#include "TriageReport.h"
#include <string>
#include <vector>

namespace Koltzi {

class CryptoVerifier {
public:
    // Compute MD5, SHA-1, SHA-256 and Import Hash (imphash)
    static bool ComputeHashes(
        const uint8_t* fileData,
        size_t fileSize,
        const std::vector<ImportEntry>& imports,
        TriageReport& report
    );

    // Verify Authenticode Digital Signature (embedded PKCS#7 or Windows Security Catalog)
    static bool VerifyAuthenticode(
        const std::wstring& filePath,
        const uint8_t* fileData,
        size_t fileSize,
        TriageReport& report
    );

private:
    static std::string HashBuffer(LPCWSTR algorithmId, const uint8_t* data, size_t size);
    static std::string ComputeImphash(const std::vector<ImportEntry>& imports);
    static bool CheckKnownTrustedSigner(const std::string& subject);
};

} // namespace Koltzi
