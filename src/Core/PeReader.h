#pragma once
#include "../Common.h"
#include "TriageReport.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Koltzi {

class PeReader {
public:
    PeReader();
    ~PeReader();

    // Prevent copying
    PeReader(const PeReader&) = delete;
    PeReader& operator=(const PeReader&) = delete;

    // Memory-map file from disk
    bool OpenFile(const std::wstring& filePath);

    // Ingest from memory buffer directly (for internal test suites / zero-copy)
    bool OpenMemory(const uint8_t* data, size_t size, const std::wstring& virtualPath = L"memory.exe");

    void Close();

    bool IsValid() const { return m_isValid; }
    const std::string& GetError() const { return m_errorMessage; }

    const uint8_t* GetBaseAddress() const { return m_baseAddress; }
    size_t GetFileSize() const { return m_fileSize; }
    bool Is64Bit() const { return m_is64Bit; }
    uint16_t GetMachine() const { return m_machine; }
    uint32_t GetEntryPointRva() const { return m_entryPointRva; }
    uint32_t GetTimestamp() const { return m_timestamp; }
    std::string GetSubsystemString() const;
    std::string GetMachineString() const;

    const std::vector<SectionInfo>& GetSections() const { return m_sections; }
    const std::vector<ImportEntry>& GetImports() const { return m_imports; }
    double GetOverallEntropy() const { return m_overallEntropy; }
    uint64_t GetImageBase() const { return m_imageBase; }
    const std::vector<uint32_t>& GetFunctionStarts() const { return m_functionStarts; }
    const std::vector<uint32_t>& GetExportRvas() const { return m_exportRvas; }

    // RVA to Raw pointer conversion with safe bounds check
    const uint8_t* RvaToPointer(uint32_t rva, uint32_t size = 1) const;

    // Section raw bytes accessor
    bool GetSectionBytes(const SectionInfo& sec, const uint8_t*& outBytes, size_t& outSize) const;

    // Detect legitimate installer runtimes (NSIS, Inno Setup, WiX, InstallShield, SFX)
    bool DetectInstaller(TriageReport& report) const;

    static double CalculateEntropy(const uint8_t* data, size_t length);

private:
    bool ParseHeaders();
    bool ParseSections();
    bool ParseImports();
    bool ParseExports();
    bool ParseExceptionDirectory();

    HANDLE m_hFile = INVALID_HANDLE_VALUE;
    HANDLE m_hMapping = NULL;
    const uint8_t* m_baseAddress = nullptr;
    size_t m_fileSize = 0;
    bool m_isMemoryMapped = false;

    bool m_isValid = false;
    std::string m_errorMessage;
    std::wstring m_filePath;

    bool m_is64Bit = false;
    uint16_t m_machine = 0;
    uint64_t m_imageBase = 0;
    uint32_t m_entryPointRva = 0;
    uint32_t m_timestamp = 0;
    uint16_t m_subsystem = 0;
    double m_overallEntropy = 0.0;

    const IMAGE_DOS_HEADER* m_dosHeader = nullptr;
    const IMAGE_NT_HEADERS32* m_ntHeaders32 = nullptr;
    const IMAGE_NT_HEADERS64* m_ntHeaders64 = nullptr;
    const IMAGE_SECTION_HEADER* m_sectionHeaders = nullptr;
    uint16_t m_numberOfSections = 0;

    std::vector<SectionInfo> m_sections;
    std::vector<ImportEntry> m_imports;
    std::vector<uint32_t> m_functionStarts;
    std::vector<uint32_t> m_exportRvas;
};

} // namespace Koltzi
