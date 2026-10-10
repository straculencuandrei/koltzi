#include "PeReader.h"
#include <cmath>
#include <cstring>

namespace Koltzi {

PeReader::PeReader() = default;

PeReader::~PeReader() {
    Close();
}

void PeReader::Close() {
    if (m_isMemoryMapped && m_baseAddress) {
        UnmapViewOfFile(m_baseAddress);
        m_baseAddress = nullptr;
    }
    if (m_hMapping) {
        CloseHandle(m_hMapping);
        m_hMapping = NULL;
    }
    if (m_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }

    m_baseAddress = nullptr;
    m_fileSize = 0;
    m_isMemoryMapped = false;
    m_isValid = false;
    m_sections.clear();
    m_imports.clear();
    m_functionStarts.clear();
    m_exportRvas.clear();
    m_imageBase = 0;
    m_dosHeader = nullptr;
    m_ntHeaders32 = nullptr;
    m_ntHeaders64 = nullptr;
    m_sectionHeaders = nullptr;
    m_numberOfSections = 0;
}

bool PeReader::OpenFile(const std::wstring& filePath) {
    Close();
    m_filePath = filePath;

    m_hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (m_hFile == INVALID_HANDLE_VALUE) {
        m_errorMessage = "Failed to open file: system error " + std::to_string(GetLastError());
        return false;
    }

    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(m_hFile, &liSize)) {
        m_errorMessage = "Failed to query file size.";
        Close();
        return false;
    }

    if (liSize.QuadPart <= 0) {
        m_errorMessage = "File is empty.";
        Close();
        return false;
    }

    if (liSize.QuadPart > 1024ULL * 1024ULL * 1024ULL) { // 1 GB safety cap for offline triage
        m_errorMessage = "File exceeds 1GB triage size limit.";
        Close();
        return false;
    }

    m_fileSize = static_cast<size_t>(liSize.QuadPart);

    m_hMapping = CreateFileMappingW(m_hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m_hMapping) {
        m_errorMessage = "CreateFileMapping failed: " + std::to_string(GetLastError());
        Close();
        return false;
    }

    m_baseAddress = static_cast<const uint8_t*>(MapViewOfFile(m_hMapping, FILE_MAP_READ, 0, 0, 0));
    if (!m_baseAddress) {
        m_errorMessage = "MapViewOfFile failed: " + std::to_string(GetLastError());
        Close();
        return false;
    }

    m_isMemoryMapped = true;
    return ParseHeaders();
}

bool PeReader::OpenMemory(const uint8_t* data, size_t size, const std::wstring& virtualPath) {
    Close();
    m_filePath = virtualPath;
    if (!data || size == 0) {
        m_errorMessage = "Invalid memory buffer.";
        return false;
    }

    m_baseAddress = data;
    m_fileSize = size;
    m_isMemoryMapped = false;

    return ParseHeaders();
}

double PeReader::CalculateEntropy(const uint8_t* data, size_t length) {
    if (!data || length == 0) return 0.0;

    uint64_t counts[256] = { 0 };
    for (size_t i = 0; i < length; ++i) {
        counts[data[i]]++;
    }

    const double invLen = 1.0 / static_cast<double>(length);
    constexpr double invLn2 = 1.4426950408889634; // 1 / ln(2)
    double entropy = 0.0;

    for (int i = 0; i < 256; ++i) {
        if (counts[i] > 0) {
            double p = static_cast<double>(counts[i]) * invLen;
            entropy -= p * (std::log(p) * invLn2);
        }
    }

    return entropy;
}

bool PeReader::ParseHeaders() {
    if (m_fileSize < sizeof(IMAGE_DOS_HEADER)) {
        m_errorMessage = "File too small to contain DOS header.";
        return false;
    }

    m_dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(m_baseAddress);
    if (m_dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        m_errorMessage = "Invalid DOS header signature (not MZ).";
        return false;
    }

    if (m_dosHeader->e_lfanew < 0 ||
        static_cast<size_t>(m_dosHeader->e_lfanew) + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) > m_fileSize) {
        m_errorMessage = "Invalid e_lfanew pointer: out of bounds.";
        return false;
    }

    const uint8_t* ntHeadersPtr = m_baseAddress + m_dosHeader->e_lfanew;
    const DWORD* ntSignature = reinterpret_cast<const DWORD*>(ntHeadersPtr);
    if (*ntSignature != IMAGE_NT_SIGNATURE) {
        m_errorMessage = "Invalid NT signature (not PE\\0\\0).";
        return false;
    }

    const IMAGE_FILE_HEADER* fileHeader = reinterpret_cast<const IMAGE_FILE_HEADER*>(ntHeadersPtr + sizeof(DWORD));
    m_machine = fileHeader->Machine;
    m_timestamp = fileHeader->TimeDateStamp;
    m_numberOfSections = fileHeader->NumberOfSections;

    const WORD* optionalMagic = reinterpret_cast<const WORD*>(
        ntHeadersPtr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER)
    );

    if (static_cast<size_t>(m_dosHeader->e_lfanew) + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + sizeof(WORD) > m_fileSize) {
        m_errorMessage = "NT Optional header out of bounds.";
        return false;
    }

    if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        m_is64Bit = true;
        if (static_cast<size_t>(m_dosHeader->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > m_fileSize) {
            m_errorMessage = "PE64 NT headers truncated.";
            return false;
        }
        m_ntHeaders64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(ntHeadersPtr);
        m_entryPointRva = m_ntHeaders64->OptionalHeader.AddressOfEntryPoint;
        m_imageBase = m_ntHeaders64->OptionalHeader.ImageBase;
        m_subsystem = m_ntHeaders64->OptionalHeader.Subsystem;
        m_sectionHeaders = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
            ntHeadersPtr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + fileHeader->SizeOfOptionalHeader
        );
    } else if (*optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        m_is64Bit = false;
        if (static_cast<size_t>(m_dosHeader->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > m_fileSize) {
            m_errorMessage = "PE32 NT headers truncated.";
            return false;
        }
        m_ntHeaders32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(ntHeadersPtr);
        m_entryPointRva = m_ntHeaders32->OptionalHeader.AddressOfEntryPoint;
        m_imageBase = m_ntHeaders32->OptionalHeader.ImageBase;
        m_subsystem = m_ntHeaders32->OptionalHeader.Subsystem;
        m_sectionHeaders = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
            ntHeadersPtr + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + fileHeader->SizeOfOptionalHeader
        );
    } else {
        m_errorMessage = "Unsupported Optional Header Magic: " + std::to_string(*optionalMagic);
        return false;
    }

    // Calculate overall file entropy
    m_overallEntropy = CalculateEntropy(m_baseAddress, m_fileSize);

    if (!ParseSections()) {
        return false;
    }

    ParseImports(); // Non-fatal if missing or packed
    ParseExports();
    ParseExceptionDirectory();

    if (m_entryPointRva != 0) {
        m_functionStarts.push_back(m_entryPointRva);
    }
    std::sort(m_functionStarts.begin(), m_functionStarts.end());
    m_functionStarts.erase(std::unique(m_functionStarts.begin(), m_functionStarts.end()), m_functionStarts.end());

    m_isValid = true;
    return true;
}

bool PeReader::ParseSections() {
    m_sections.clear();

    const size_t sectionTableEnd = reinterpret_cast<const uint8_t*>(m_sectionHeaders + m_numberOfSections) - m_baseAddress;
    if (sectionTableEnd > m_fileSize) {
        m_errorMessage = "Section table exceeds file size.";
        return false;
    }

    for (uint16_t i = 0; i < m_numberOfSections; ++i) {
        const IMAGE_SECTION_HEADER& sh = m_sectionHeaders[i];
        SectionInfo info;

        char nameBuf[9] = { 0 };
        std::memcpy(nameBuf, sh.Name, 8);
        info.name = nameBuf;

        info.virtualAddress = sh.VirtualAddress;
        info.virtualSize = sh.Misc.VirtualSize;
        info.rawOffset = sh.PointerToRawData;
        info.rawSize = sh.SizeOfRawData;
        info.characteristics = sh.Characteristics;

        info.isExecutable = (sh.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        info.isWritable = (sh.Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        info.isRwx = info.isExecutable && info.isWritable;

        // Calculate section entropy safely
        if (info.rawOffset < m_fileSize && info.rawSize > 0) {
            size_t availableRaw = std::min<size_t>(info.rawSize, m_fileSize - info.rawOffset);
            info.entropy = CalculateEntropy(m_baseAddress + info.rawOffset, availableRaw);
        } else {
            info.entropy = 0.0;
        }

        // Flag high entropy on executable sections (> 7.2) or abnormally high on any section (> 7.8)
        if (info.isExecutable && info.entropy > 7.2) {
            info.isSuspiciousEntropy = true;
        } else if (info.entropy > 7.8) {
            info.isSuspiciousEntropy = true;
        }

        m_sections.push_back(info);
    }

    return true;
}

const uint8_t* PeReader::RvaToPointer(uint32_t rva, uint32_t size) const {
    if (!m_baseAddress || rva == 0) return nullptr;

    for (const auto& sec : m_sections) {
        uint32_t vSize = sec.virtualSize ? sec.virtualSize : sec.rawSize;
        if (rva >= sec.virtualAddress && rva < sec.virtualAddress + vSize) {
            uint32_t offsetIntoSec = rva - sec.virtualAddress;
            if (offsetIntoSec >= sec.rawSize) return nullptr;

            uint32_t fileOffset = sec.rawOffset + offsetIntoSec;
            if (fileOffset + size <= m_fileSize) {
                return m_baseAddress + fileOffset;
            }
            return nullptr;
        }
    }
    return nullptr;
}

bool PeReader::GetSectionBytes(const SectionInfo& sec, const uint8_t*& outBytes, size_t& outSize) const {
    if (!m_baseAddress || sec.rawOffset >= m_fileSize || sec.rawSize == 0) {
        outBytes = nullptr;
        outSize = 0;
        return false;
    }

    outBytes = m_baseAddress + sec.rawOffset;
    outSize = std::min<size_t>(sec.rawSize, m_fileSize - sec.rawOffset);
    return true;
}

bool PeReader::ParseImports() {
    m_imports.clear();

    const IMAGE_DATA_DIRECTORY* importDir = nullptr;
    if (m_is64Bit && m_ntHeaders64) {
        if (IMAGE_DIRECTORY_ENTRY_IMPORT < m_ntHeaders64->OptionalHeader.NumberOfRvaAndSizes) {
            importDir = &m_ntHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        }
    } else if (!m_is64Bit && m_ntHeaders32) {
        if (IMAGE_DIRECTORY_ENTRY_IMPORT < m_ntHeaders32->OptionalHeader.NumberOfRvaAndSizes) {
            importDir = &m_ntHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        }
    }

    if (!importDir || importDir->VirtualAddress == 0 || importDir->Size == 0) {
        return true; // No imports (normal in packed binaries or shellcode stubs)
    }

    const uint8_t* importData = RvaToPointer(importDir->VirtualAddress, sizeof(IMAGE_IMPORT_DESCRIPTOR));
    if (!importData) return false;

    const IMAGE_IMPORT_DESCRIPTOR* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(importData);

    // Limit maximum descriptors to 512 for safety against malformed headers
    for (size_t d = 0; d < 512; ++d) {
        if (desc->Characteristics == 0 && desc->Name == 0 && desc->FirstThunk == 0) {
            break; // Terminal entry
        }

        const char* dllNamePtr = reinterpret_cast<const char*>(RvaToPointer(desc->Name, 1));
        if (dllNamePtr) {
            // Find length safely within bounds
            std::string dllName;
            const uint8_t* maxPtr = m_baseAddress + m_fileSize;
            const char* cur = dllNamePtr;
            while (reinterpret_cast<const uint8_t*>(cur) < maxPtr && *cur != '\0' && dllName.size() < 260) {
                dllName.push_back(*cur++);
            }

            ImportEntry entry;
            entry.dllName = dllName;

            uint32_t thunkRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
            if (thunkRva != 0) {
                if (m_is64Bit) {
                    const IMAGE_THUNK_DATA64* thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(
                        RvaToPointer(thunkRva, sizeof(IMAGE_THUNK_DATA64))
                    );
                    for (size_t f = 0; thunk && f < 4096; ++f) {
                        if (thunk->u1.AddressOfData == 0) break;
                        uint32_t iatRva = desc->FirstThunk + static_cast<uint32_t>(f * sizeof(IMAGE_THUNK_DATA64));

                        if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) {
                            entry.functions.push_back("Ordinal#" + std::to_string(IMAGE_ORDINAL64(thunk->u1.Ordinal)));
                            entry.iatRvas.push_back(iatRva);
                        } else {
                            uint32_t nameRva = static_cast<uint32_t>(thunk->u1.AddressOfData);
                            const IMAGE_IMPORT_BY_NAME* ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                                RvaToPointer(nameRva, sizeof(IMAGE_IMPORT_BY_NAME))
                            );
                            if (ibn) {
                                std::string fnName;
                                const char* fnCur = reinterpret_cast<const char*>(ibn->Name);
                                while (reinterpret_cast<const uint8_t*>(fnCur) < maxPtr && *fnCur != '\0' && fnName.size() < 260) {
                                    fnName.push_back(*fnCur++);
                                }
                                if (!fnName.empty()) {
                                    entry.functions.push_back(fnName);
                                    entry.iatRvas.push_back(iatRva);
                                }
                            }
                        }
                        thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(
                            RvaToPointer(thunkRva + static_cast<uint32_t>((f + 1) * sizeof(IMAGE_THUNK_DATA64)), sizeof(IMAGE_THUNK_DATA64))
                        );
                    }
                } else {
                    const IMAGE_THUNK_DATA32* thunk = reinterpret_cast<const IMAGE_THUNK_DATA32*>(
                        RvaToPointer(thunkRva, sizeof(IMAGE_THUNK_DATA32))
                    );
                    for (size_t f = 0; thunk && f < 4096; ++f) {
                        if (thunk->u1.AddressOfData == 0) break;
                        uint32_t iatRva = desc->FirstThunk + static_cast<uint32_t>(f * sizeof(IMAGE_THUNK_DATA32));

                        if (IMAGE_SNAP_BY_ORDINAL32(thunk->u1.Ordinal)) {
                            entry.functions.push_back("Ordinal#" + std::to_string(IMAGE_ORDINAL32(thunk->u1.Ordinal)));
                            entry.iatRvas.push_back(iatRva);
                        } else {
                            uint32_t nameRva = static_cast<uint32_t>(thunk->u1.AddressOfData);
                            const IMAGE_IMPORT_BY_NAME* ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                                RvaToPointer(nameRva, sizeof(IMAGE_IMPORT_BY_NAME))
                            );
                            if (ibn) {
                                std::string fnName;
                                const char* fnCur = reinterpret_cast<const char*>(ibn->Name);
                                while (reinterpret_cast<const uint8_t*>(fnCur) < maxPtr && *fnCur != '\0' && fnName.size() < 260) {
                                    fnName.push_back(*fnCur++);
                                }
                                if (!fnName.empty()) {
                                    entry.functions.push_back(fnName);
                                    entry.iatRvas.push_back(iatRva);
                                }
                            }
                        }
                        thunk = reinterpret_cast<const IMAGE_THUNK_DATA32*>(
                            RvaToPointer(thunkRva + static_cast<uint32_t>((f + 1) * sizeof(IMAGE_THUNK_DATA32)), sizeof(IMAGE_THUNK_DATA32))
                        );
                    }
                }
            }

            m_imports.push_back(std::move(entry));
        }

        desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(
            RvaToPointer(importDir->VirtualAddress + static_cast<uint32_t>((d + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR)), sizeof(IMAGE_IMPORT_DESCRIPTOR))
        );
        if (!desc) break;
    }

    return true;
}

bool PeReader::ParseExports() {
    m_exportRvas.clear();

    const IMAGE_DATA_DIRECTORY* exportDir = nullptr;
    if (m_is64Bit && m_ntHeaders64) {
        if (IMAGE_DIRECTORY_ENTRY_EXPORT < m_ntHeaders64->OptionalHeader.NumberOfRvaAndSizes) {
            exportDir = &m_ntHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        }
    } else if (!m_is64Bit && m_ntHeaders32) {
        if (IMAGE_DIRECTORY_ENTRY_EXPORT < m_ntHeaders32->OptionalHeader.NumberOfRvaAndSizes) {
            exportDir = &m_ntHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        }
    }

    if (!exportDir || exportDir->VirtualAddress == 0 || exportDir->Size == 0) {
        return true;
    }

    const uint8_t* expData = RvaToPointer(exportDir->VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY));
    if (!expData) return false;

    const IMAGE_EXPORT_DIRECTORY* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(expData);
    if (exp->NumberOfFunctions == 0 || exp->AddressOfFunctions == 0) return true;

    uint32_t numFuncs = std::min<uint32_t>(exp->NumberOfFunctions, 65536);
    const uint32_t* funcTable = reinterpret_cast<const uint32_t*>(
        RvaToPointer(exp->AddressOfFunctions, numFuncs * sizeof(uint32_t))
    );
    if (!funcTable) return false;

    for (uint32_t i = 0; i < numFuncs; ++i) {
        uint32_t fnRva = funcTable[i];
        if (fnRva != 0) {
            // Check if forwarded export (within export directory range)
            if (fnRva >= exportDir->VirtualAddress && fnRva < exportDir->VirtualAddress + exportDir->Size) {
                continue;
            }
            m_exportRvas.push_back(fnRva);
            m_functionStarts.push_back(fnRva);
        }
    }

    return true;
}

bool PeReader::ParseExceptionDirectory() {
    if (!m_is64Bit || !m_ntHeaders64) return true;

    const IMAGE_DATA_DIRECTORY* exceptionDir = nullptr;
    if (IMAGE_DIRECTORY_ENTRY_EXCEPTION < m_ntHeaders64->OptionalHeader.NumberOfRvaAndSizes) {
        exceptionDir = &m_ntHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    }

    if (!exceptionDir || exceptionDir->VirtualAddress == 0 || exceptionDir->Size == 0) {
        return true;
    }

    struct RuntimeFunctionEntry {
        uint32_t beginAddress;
        uint32_t endAddress;
        uint32_t unwindData;
    };

    size_t count = exceptionDir->Size / sizeof(RuntimeFunctionEntry);
    count = std::min<size_t>(count, 200000);

    const RuntimeFunctionEntry* rfTable = reinterpret_cast<const RuntimeFunctionEntry*>(
        RvaToPointer(exceptionDir->VirtualAddress, static_cast<uint32_t>(count * sizeof(RuntimeFunctionEntry)))
    );
    if (!rfTable) return false;

    for (size_t i = 0; i < count; ++i) {
        if (rfTable[i].beginAddress != 0) {
            m_functionStarts.push_back(rfTable[i].beginAddress);
        }
    }

    return true;
}

std::string PeReader::GetSubsystemString() const {
    switch (m_subsystem) {
    case IMAGE_SUBSYSTEM_WINDOWS_GUI: return "Windows GUI";
    case IMAGE_SUBSYSTEM_WINDOWS_CUI: return "Windows Console";
    case IMAGE_SUBSYSTEM_NATIVE:      return "Native Driver";
    case IMAGE_SUBSYSTEM_POSIX_CUI:   return "POSIX Console";
    case IMAGE_SUBSYSTEM_EFI_APPLICATION: return "EFI Application";
    default: return "Unknown (" + std::to_string(m_subsystem) + ")";
    }
}

std::string PeReader::GetMachineString() const {
    switch (m_machine) {
    case IMAGE_FILE_MACHINE_AMD64: return "x64 (AMD64)";
    case IMAGE_FILE_MACHINE_I386:  return "x86 (Intel 386)";
    case IMAGE_FILE_MACHINE_ARM64: return "ARM64";
    default: return "Arch 0x" + std::format("{:04X}", m_machine);
    }
}

} // namespace Koltzi
