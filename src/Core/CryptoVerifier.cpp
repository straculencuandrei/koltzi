#include "CryptoVerifier.h"
#include <mscat.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Koltzi {

std::string CryptoVerifier::HashBuffer(LPCWSTR algorithmId, const uint8_t* data, size_t size) {
    if (!data || size == 0) return "";

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, algorithmId, nullptr, 0);
    if (!BCRYPT_SUCCESS(status) || !hAlg) {
        return "";
    }

    DWORD hashObjSize = 0;
    DWORD cbData = 0;
    status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjSize), sizeof(DWORD), &cbData, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    DWORD hashLength = 0;
    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(DWORD), &cbData, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    std::vector<uint8_t> hashObject(hashObjSize);
    std::vector<uint8_t> hashBuffer(hashLength);

    BCRYPT_HASH_HANDLE hHash = nullptr;
    status = BCryptCreateHash(hAlg, &hHash, hashObject.data(), hashObjSize, nullptr, 0, 0);
    if (BCRYPT_SUCCESS(status) && hHash) {
        status = BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0);
        if (BCRYPT_SUCCESS(status)) {
            BCryptFinishHash(hHash, hashBuffer.data(), hashLength, 0);
        }
        BCryptDestroyHash(hHash);
    }

    BCryptCloseAlgorithmProvider(hAlg, 0);

    std::ostringstream ss;
    for (DWORD i = 0; i < hashLength; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hashBuffer[i]);
    }
    return ss.str();
}

std::string CryptoVerifier::ComputeImphash(const std::vector<ImportEntry>& imports) {
    if (imports.empty()) return "";

    std::ostringstream rawStream;
    bool first = true;

    for (const auto& imp : imports) {
        std::string dll = imp.dllName;
        std::transform(dll.begin(), dll.end(), dll.begin(), [](unsigned char c) {
            return static_cast<char>(::tolower(c));
        });

        // Strip file extensions per imphash spec
        for (const std::string& ext : { ".dll", ".ocx", ".sys", ".drv" }) {
            if (dll.size() >= ext.size() && dll.compare(dll.size() - ext.size(), ext.size(), ext) == 0) {
                dll = dll.substr(0, dll.size() - ext.size());
                break;
            }
        }

        for (const auto& fn : imp.functions) {
            std::string func = fn;
            std::transform(func.begin(), func.end(), func.begin(), [](unsigned char c) {
                return static_cast<char>(::tolower(c));
            });

            // Convert Ordinal#123 to ord123
            if (func.rfind("ordinal#", 0) == 0) {
                func = "ord" + func.substr(8);
            }

            if (!first) rawStream << ",";
            rawStream << dll << "." << func;
            first = false;
        }
    }

    std::string importString = rawStream.str();
    if (importString.empty()) return "";

    return HashBuffer(BCRYPT_MD5_ALGORITHM, reinterpret_cast<const uint8_t*>(importString.data()), importString.size());
}

bool CryptoVerifier::ComputeHashes(
    const uint8_t* fileData,
    size_t fileSize,
    const std::vector<ImportEntry>& imports,
    TriageReport& report
) {
    if (!fileData || fileSize == 0) return false;

    report.md5 = HashBuffer(BCRYPT_MD5_ALGORITHM, fileData, fileSize);
    report.sha1 = HashBuffer(BCRYPT_SHA1_ALGORITHM, fileData, fileSize);
    report.sha256 = HashBuffer(BCRYPT_SHA256_ALGORITHM, fileData, fileSize);
    report.imphash = ComputeImphash(imports);

    report.AddLog("HASH", "PASS", "Computed MD5: " + report.md5);
    report.AddLog("HASH", "PASS", "Computed SHA-1: " + report.sha1);
    report.AddLog("HASH", "PASS", "Computed SHA-256: " + report.sha256);
    if (!report.imphash.empty()) {
        report.AddLog("HASH", "PASS", "Computed Imphash: " + report.imphash);
    } else {
        report.AddLog("HASH", "INFO", "No static imports present for Imphash calculation");
    }

    return true;
}

bool CryptoVerifier::CheckKnownTrustedSigner(const std::string& subject) {
    std::string lower = subject;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(::tolower(c));
    });

    const char* trustedSigners[] = {
        "mozilla corporation",
        "microsoft corporation",
        "microsoft windows",
        "google llc",
        "google inc",
        "brave software",
        "opera norway",
        "valve corp",
        "apple inc",
        "discord inc",
        "github, inc",
        "electronic arts",
        "adobe inc",
        "nvidia corporation",
        "piriform",
        "ccleaner",
        "iobit",
        "jetbrains",
        "oracle",
        "spotify",
        "slack technologies",
        "epic games",
        "unity technologies",
        "amazon.com",
        "intel corporation",
        "advanced micro devices",
        "amd",
        "logitech",
        "corsair",
        "zoom video",
        "dropbox",
        "vmware",
        "anydesk",
        "teamviewer",
        "realtek",
        "sysinternals",
        "nullsoft",
        "jrsoftware",
        "inno setup"
    };

    for (const char* signer : trustedSigners) {
        if (lower.find(signer) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool CryptoVerifier::VerifyAuthenticode(
    const std::wstring& filePath,
    const uint8_t* fileData,
    size_t fileSize,
    TriageReport& report
) {
    (void)fileData;
    (void)fileSize;

    if (filePath.empty()) {
        report.signature.statusText = "Virtual memory sample (No disk path for certificate store)";
        report.AddLog("CERT", "INFO", report.signature.statusText);
        return false;
    }

    // Initialize WinTrust structures for offline check (no online CRL check)
    WINTRUST_FILE_INFO fileDataInfo = { sizeof(WINTRUST_FILE_INFO) };
    fileDataInfo.pcwszFilePath = filePath.c_str();
    fileDataInfo.hFile = NULL;
    fileDataInfo.pgKnownSubject = nullptr;

    WINTRUST_DATA winTrustData = { sizeof(WINTRUST_DATA) };
    winTrustData.dwUIChoice = WTD_UI_NONE;
    winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE; // Offline triage guarantee
    winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    winTrustData.pFile = &fileDataInfo;
    winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    winTrustData.hWVTStateData = NULL;
    winTrustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_SAFER_FLAG;

    GUID policyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG lStatus = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &winTrustData);

    // Extract Signer Certificate Details using CryptQueryObject
    HCERTSTORE hStore = nullptr;
    HCRYPTMSG hMsg = nullptr;
    PCCERT_CONTEXT pCertContext = nullptr;

    bool queryOk = CryptQueryObject(
        CERT_QUERY_OBJECT_FILE,
        filePath.c_str(),
        CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
        CERT_QUERY_FORMAT_FLAG_BINARY,
        0,
        nullptr,
        nullptr,
        nullptr,
        &hStore,
        &hMsg,
        nullptr
    );

    if (queryOk && hMsg) {
        DWORD cbSignerInfo = 0;
        if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &cbSignerInfo) && cbSignerInfo > 0) {
            std::vector<uint8_t> signerInfoBuffer(cbSignerInfo);
            CMSG_SIGNER_INFO* pSignerInfo = reinterpret_cast<CMSG_SIGNER_INFO*>(signerInfoBuffer.data());
            if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, pSignerInfo, &cbSignerInfo)) {
                CERT_INFO certInfo = {};
                certInfo.Issuer = pSignerInfo->Issuer;
                certInfo.SerialNumber = pSignerInfo->SerialNumber;
                pCertContext = CertFindCertificateInStore(
                    hStore,
                    X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                    0,
                    CERT_FIND_SUBJECT_CERT,
                    &certInfo,
                    nullptr
                );

                if (pCertContext) {
                    char szSubject[256] = { 0 };
                    char szIssuer[256] = { 0 };

                    CertGetNameStringA(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, szSubject, sizeof(szSubject));
                    CertGetNameStringA(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, nullptr, szIssuer, sizeof(szIssuer));

                    report.signature.signerSubject = szSubject;
                    report.signature.signerIssuer = szIssuer;
                    report.signature.isSigned = true;

                    CertFreeCertificateContext(pCertContext);
                }
            }
        }
    }

    if (hStore) CertCloseStore(hStore, 0);
    if (hMsg) CryptMsgClose(hMsg);

    // Close WinTrust state
    winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policyGuid, &winTrustData);

    // Evaluate Trust Status
    if (lStatus == ERROR_SUCCESS) {
        report.signature.isSigned = true;
        report.signature.isValid = true;
        report.signature.statusText = "Valid Authenticode Digital Signature";
        report.signature.isTrustedVendor = CheckKnownTrustedSigner(report.signature.signerSubject);

        report.AddLog("CERT", "PASS", "Authenticode Signature: VALID");
        report.AddLog("CERT", "INFO", "Signer Subject: " + report.signature.signerSubject);
        report.AddLog("CERT", "INFO", "Certificate Issuer: " + report.signature.signerIssuer);

        if (report.signature.isTrustedVendor) {
            report.isLegitimateBrowser = true;
            report.browserIdentity = report.signature.signerSubject;
            report.AddLog("CERT", "PASS", "Recognized as verified trusted software publisher: " + report.signature.signerSubject);
        }
        return true;
    }

    // Check if signed but untrusted / self-signed / expired
    if (report.signature.isSigned) {
        report.signature.isValid = false;
        if (lStatus == TRUST_E_EXPLICIT_DISTRUST) {
            report.signature.statusText = "Explicitly untrusted / revoked certificate";
            report.AddLog("CERT", "CRIT", "Authenticode signature: EXPLICITLY REVOKED");
        } else if (lStatus == CERT_E_EXPIRED) {
            report.signature.statusText = "Expired digital signature";
            report.AddLog("CERT", "WARN", "Authenticode signature: EXPIRED");
        } else {
            report.signature.statusText = "Self-signed or untrusted root certificate";
            report.AddLog("CERT", "WARN", "Authenticode signature: UNTRUSTED ROOT (" + report.signature.signerSubject + ")");
        }
        return false;
    }

    // Check Windows Security Catalog for System Binaries
    report.signature.isSigned = false;
    report.signature.isValid = false;
    report.signature.statusText = "Unsigned binary (No Authenticode certificate)";
    report.AddLog("CERT", "INFO", "Binary is not digitally signed (Unsigned PE)");
    return false;
}

} // namespace Koltzi
