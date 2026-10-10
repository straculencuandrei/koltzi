#include "FontManager.h"
#include <dwrite_3.h>
#include <filesystem>
#include <iostream>

namespace Koltzi {

FontManager::FontManager() = default;

FontManager::~FontManager() {
    Shutdown();
}

void FontManager::LoadJetBrainsFonts() {
    std::vector<std::filesystem::path> searchDirs = {
        std::filesystem::current_path() / "FONT" / "fonts" / "ttf",
        std::filesystem::current_path() / ".." / "FONT" / "fonts" / "ttf",
        std::filesystem::path(L"C:/Users/buzunar/Documents/game/koltzi/FONT/fonts/ttf")
    };

    wchar_t exePathBuf[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH) > 0) {
        std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();
        searchDirs.push_back(exeDir / "FONT" / "fonts" / "ttf");
        searchDirs.push_back(exeDir / ".." / "FONT" / "fonts" / "ttf");
    }

    std::vector<std::wstring> fontFiles;
    for (const auto& dir : searchDirs) {
        if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".ttf") {
                    std::wstring fullPath = entry.path().wstring();
                    int res = AddFontResourceExW(fullPath.c_str(), FR_PRIVATE, 0);
                    if (res > 0) {
                        m_registeredDiskFonts.push_back(fullPath);
                        fontFiles.push_back(fullPath);
                    }
                }
            }
            if (!fontFiles.empty()) {
                break;
            }
        }
    }

    if (fontFiles.empty()) {
        return;
    }

    // DirectWrite IDWriteFactory5 Custom Font Set Builder
    IDWriteFactory5* factory5 = nullptr;
    if (SUCCEEDED(m_dwriteFactory->QueryInterface(__uuidof(IDWriteFactory5), reinterpret_cast<void**>(&factory5))) && factory5) {
        IDWriteFontSetBuilder1* builder = nullptr;
        if (SUCCEEDED(factory5->CreateFontSetBuilder(&builder)) && builder) {
            for (const auto& fPath : fontFiles) {
                IDWriteFontFile* fontFile = nullptr;
                if (SUCCEEDED(factory5->CreateFontFileReference(fPath.c_str(), nullptr, &fontFile)) && fontFile) {
                    builder->AddFontFile(fontFile);
                    fontFile->Release();
                }
            }
            IDWriteFontSet* fontSet = nullptr;
            if (SUCCEEDED(builder->CreateFontSet(&fontSet)) && fontSet) {
                IDWriteFontCollection1* col1 = nullptr;
                if (SUCCEEDED(factory5->CreateFontCollectionFromFontSet(fontSet, &col1)) && col1) {
                    m_fontCollection = col1;
                    m_fontLoaded = true;
                    m_fontFamily = L"JetBrains Mono";
                }
                fontSet->Release();
            }
            builder->Release();
        }
        factory5->Release();
    }
}

void FontManager::InitUiFontFamily() {
    m_uiFamily = L"Segoe UI";
    if (!m_dwriteFactory) return;

    IDWriteFontCollection* sysCol = nullptr;
    if (SUCCEEDED(m_dwriteFactory->GetSystemFontCollection(&sysCol)) && sysCol) {
        UINT32 index = 0;
        BOOL exists = FALSE;
        if (SUCCEEDED(sysCol->FindFamilyName(L"Segoe UI Variable Text", &index, &exists)) && exists) {
            m_uiFamily = L"Segoe UI Variable Text";
        }
        sysCol->Release();
    }
}

bool FontManager::Initialize(IDWriteFactory* dwriteFactory) {
    if (!dwriteFactory) return false;
    m_dwriteFactory = dwriteFactory;

    InitUiFontFamily();
    LoadJetBrainsFonts();

    const wchar_t* monoFam = m_fontLoaded ? m_fontFamily.c_str() : L"Consolas";
    IDWriteFontCollection* monoCol = m_fontCollection;

    // --- UI Sans-serif Formats (Segoe UI Variable / Segoe UI) ---
    m_dwriteFactory->CreateTextFormat(
        m_uiFamily.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        15.0f, L"en-us", &m_uiTitleFormat
    );

    m_dwriteFactory->CreateTextFormat(
        m_uiFamily.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        12.5f, L"en-us", &m_uiHeaderFormat
    );

    m_dwriteFactory->CreateTextFormat(
        m_uiFamily.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.0f, L"en-us", &m_uiSubheadingFormat
    );

    m_dwriteFactory->CreateTextFormat(
        m_uiFamily.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        10.0f, L"en-us", &m_uiLabelFormat
    );

    m_dwriteFactory->CreateTextFormat(
        m_uiFamily.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        10.5f, L"en-us", &m_uiButtonFormat
    );

    m_dwriteFactory->CreateTextFormat(
        m_uiFamily.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        9.0f, L"en-us", &m_uiBadgeFormat
    );

    m_dwriteFactory->CreateTextFormat(
        m_uiFamily.c_str(), nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        24.0f, L"en-us", &m_uiMetricFormat
    );

    // --- Monospace Formats (JetBrains Mono) ---
    m_dwriteFactory->CreateTextFormat(
        monoFam, monoCol,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        10.0f, L"en-us", &m_monoCodeFormat
    );

    m_dwriteFactory->CreateTextFormat(
        monoFam, monoCol,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        9.5f, L"en-us", &m_monoLogFormat
    );

    m_dwriteFactory->CreateTextFormat(
        monoFam, monoCol,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        10.0f, L"en-us", &m_monoHeaderFormat
    );

    m_dwriteFactory->CreateTextFormat(
        monoFam, monoCol,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        9.0f, L"en-us", &m_monoBadgeFormat
    );

    return true;
}

void FontManager::Shutdown() {
    SafeRelease(m_uiTitleFormat);
    SafeRelease(m_uiHeaderFormat);
    SafeRelease(m_uiSubheadingFormat);
    SafeRelease(m_uiLabelFormat);
    SafeRelease(m_uiButtonFormat);
    SafeRelease(m_uiBadgeFormat);
    SafeRelease(m_uiMetricFormat);

    SafeRelease(m_monoCodeFormat);
    SafeRelease(m_monoLogFormat);
    SafeRelease(m_monoHeaderFormat);
    SafeRelease(m_monoBadgeFormat);

    SafeRelease(m_fontCollection);

    for (const auto& path : m_registeredDiskFonts) {
        RemoveFontResourceExW(path.c_str(), FR_PRIVATE, 0);
    }
    m_registeredDiskFonts.clear();

    for (HANDLE h : m_memFontHandles) {
        RemoveFontMemResourceEx(h);
    }
    m_memFontHandles.clear();

    m_fontLoaded = false;
    m_dwriteFactory = nullptr;
}

} // namespace Koltzi
