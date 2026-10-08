#include "FontManager.h"
#include "../../res/resource.h"
#include <filesystem>

namespace Koltzi {

FontManager::FontManager() = default;

FontManager::~FontManager() {
    Shutdown();
}

void FontManager::LoadDiskFonts() {
    // Possible paths for FONT/creato_display
    std::vector<std::filesystem::path> searchDirs = {
        std::filesystem::current_path() / "FONT" / "creato_display",
        std::filesystem::current_path() / ".." / "FONT" / "creato_display",
        std::filesystem::path(L"FONT") / L"creato_display"
    };

    // Get executable path directory as another candidate
    wchar_t exePathBuf[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH) > 0) {
        std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();
        searchDirs.push_back(exeDir / "FONT" / "creato_display");
        searchDirs.push_back(exeDir / ".." / "FONT" / "creato_display");
    }

    for (const auto& dir : searchDirs) {
        if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
            bool anyAdded = false;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".otf") {
                    std::wstring fullPath = entry.path().wstring();
                    int res = AddFontResourceExW(fullPath.c_str(), FR_PRIVATE, 0);
                    if (res > 0) {
                        m_registeredDiskFonts.push_back(fullPath);
                        anyAdded = true;
                    }
                }
            }
            if (anyAdded) {
                m_creatoLoaded = true;
                return;
            }
        }
    }
}

void FontManager::LoadEmbeddedFonts() {
    HMODULE hMod = GetModuleHandle(nullptr);
    if (!hMod) return;

    int fontResIds[] = {
        IDR_FONT_THIN,
        IDR_FONT_LIGHT,
        IDR_FONT_REGULAR,
        IDR_FONT_REGULAR_ITAL,
        IDR_FONT_MEDIUM,
        IDR_FONT_BOLD,
        IDR_FONT_EXTRABOLD,
        IDR_FONT_BLACK
    };

    const wchar_t* fontNames[] = {
        L"CreatoDisplay-Thin.otf",
        L"CreatoDisplay-Light.otf",
        L"CreatoDisplay-Regular.otf",
        L"CreatoDisplay-RegularItalic.otf",
        L"CreatoDisplay-Medium.otf",
        L"CreatoDisplay-Bold.otf",
        L"CreatoDisplay-ExtraBold.otf",
        L"CreatoDisplay-Black.otf"
    };

    wchar_t tempDirBuf[MAX_PATH] = { 0 };
    if (GetTempPathW(MAX_PATH, tempDirBuf) == 0) return;

    std::filesystem::path tempFontDir = std::filesystem::path(tempDirBuf) / L"KoltziFonts";
    std::error_code ec;
    std::filesystem::create_directories(tempFontDir, ec);

    for (size_t i = 0; i < sizeof(fontResIds) / sizeof(fontResIds[0]); ++i) {
        HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(fontResIds[i]), MAKEINTRESOURCEW(10));
        if (!hRes) continue;

        HGLOBAL hMem = LoadResource(hMod, hRes);
        if (!hMem) continue;

        void* pData = LockResource(hMem);
        DWORD size = SizeofResource(hMod, hRes);
        if (!pData || size == 0) continue;

        // Also add to memory font table for GDI
        DWORD cFonts = 0;
        HANDLE hMemFont = AddFontMemResourceEx(pData, size, nullptr, &cFonts);
        if (hMemFont) {
            m_memFontHandles.push_back(hMemFont);
        }

        // Write to temp file for DirectWrite system font collection
        std::filesystem::path targetFile = tempFontDir / fontNames[i];
        HANDLE hFile = CreateFileW(
            targetFile.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hFile, pData, size, &written, nullptr);
            CloseHandle(hFile);

            int res = AddFontResourceExW(targetFile.c_str(), FR_PRIVATE, 0);
            if (res > 0) {
                m_registeredDiskFonts.push_back(targetFile.wstring());
                m_creatoLoaded = true;
            }
        }
    }
}

bool FontManager::Initialize(IDWriteFactory* dwriteFactory) {
    if (!dwriteFactory) return false;
    m_dwriteFactory = dwriteFactory;

    // First try disk fonts
    LoadDiskFonts();

    // If not found on disk, extract embedded resources
    if (!m_creatoLoaded) {
        LoadEmbeddedFonts();
    }

    // Set font family
    const wchar_t* family = m_creatoLoaded ? L"Creato Display" : L"Segoe UI";
    m_fontFamily = family;

    // 1. Title format: Black / ExtraBold, 16pt (for Koltzi branding, big labels)
    m_dwriteFactory->CreateTextFormat(
        family,
        nullptr,
        DWRITE_FONT_WEIGHT_BLACK,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        16.0f,
        L"en-us",
        &m_titleFormat
    );

    // 2. Header format: ExtraBold, 13.5pt (for dashboard card headings)
    m_dwriteFactory->CreateTextFormat(
        family,
        nullptr,
        DWRITE_FONT_WEIGHT_EXTRA_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.5f,
        L"en-us",
        &m_headerFormat
    );

    // 3. Subheading format: Bold, 11.5pt (for card subheads, metric values)
    m_dwriteFactory->CreateTextFormat(
        family,
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        11.5f,
        L"en-us",
        &m_subheadingFormat
    );

    // 4. Button format: Medium, 10.5pt (for toolbar buttons)
    m_dwriteFactory->CreateTextFormat(
        family,
        nullptr,
        DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        10.5f,
        L"en-us",
        &m_buttonFormat
    );

    // 5. Body format: Regular, 12.0f (for general explanations)
    m_dwriteFactory->CreateTextFormat(
        family,
        nullptr,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f,
        L"en-us",
        &m_bodyFormat
    );

    // 6. Dialogue format: Medium / Normal, 13.0f (for mascot dialogue bubble)
    m_dwriteFactory->CreateTextFormat(
        family,
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.0f,
        L"en-us",
        &m_dialogueFormat
    );

    // 7. Badge format: Bold, 10.0f (for pills, status indicators)
    m_dwriteFactory->CreateTextFormat(
        family,
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        10.0f,
        L"en-us",
        &m_badgeFormat
    );

    // 8. Code format: Consolas (Monospace is essential for hashes, addresses, hex, disassembly)
    m_dwriteFactory->CreateTextFormat(
        L"Consolas",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        10.5f,
        L"en-us",
        &m_codeFormat
    );

    // 9. Log format: Consolas, 9.5pt (for dense, high-detail audit logs)
    m_dwriteFactory->CreateTextFormat(
        L"Consolas",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        9.5f,
        L"en-us",
        &m_logFormat
    );

    return true;
}

void FontManager::Shutdown() {
    SafeRelease(m_titleFormat);
    SafeRelease(m_headerFormat);
    SafeRelease(m_subheadingFormat);
    SafeRelease(m_buttonFormat);
    SafeRelease(m_bodyFormat);
    SafeRelease(m_dialogueFormat);
    SafeRelease(m_badgeFormat);
    SafeRelease(m_codeFormat);
    SafeRelease(m_logFormat);

    for (const auto& path : m_registeredDiskFonts) {
        RemoveFontResourceExW(path.c_str(), FR_PRIVATE, 0);
    }
    m_registeredDiskFonts.clear();

    for (HANDLE h : m_memFontHandles) {
        RemoveFontMemResourceEx(h);
    }
    m_memFontHandles.clear();

    m_creatoLoaded = false;
    m_dwriteFactory = nullptr;
}

} // namespace Koltzi
