#pragma once
#include "../Common.h"
#include <dwrite.h>
#include <string>
#include <vector>

namespace Koltzi {

class FontManager {
public:
    FontManager();
    ~FontManager();

    bool Initialize(IDWriteFactory* dwriteFactory);
    void Shutdown();

    // Typography accessors
    IDWriteTextFormat* GetUiTitleFormat() const { return m_uiTitleFormat; }
    IDWriteTextFormat* GetUiHeaderFormat() const { return m_uiHeaderFormat; }
    IDWriteTextFormat* GetUiSubheadingFormat() const { return m_uiSubheadingFormat; }
    IDWriteTextFormat* GetUiLabelFormat() const { return m_uiLabelFormat; }
    IDWriteTextFormat* GetUiButtonFormat() const { return m_uiButtonFormat; }
    IDWriteTextFormat* GetUiBadgeFormat() const { return m_uiBadgeFormat; }
    IDWriteTextFormat* GetUiMetricFormat() const { return m_uiMetricFormat; }

    IDWriteTextFormat* GetMonoCodeFormat() const { return m_monoCodeFormat; }
    IDWriteTextFormat* GetMonoLogFormat() const { return m_monoLogFormat; }
    IDWriteTextFormat* GetMonoHeaderFormat() const { return m_monoHeaderFormat; }
    IDWriteTextFormat* GetMonoBadgeFormat() const { return m_monoBadgeFormat; }

    // Backward compatibility accessors
    IDWriteTextFormat* GetTitleFormat() const { return m_uiTitleFormat; }
    IDWriteTextFormat* GetHeaderFormat() const { return m_uiHeaderFormat; }
    IDWriteTextFormat* GetSubheadingFormat() const { return m_uiSubheadingFormat; }
    IDWriteTextFormat* GetButtonFormat() const { return m_uiButtonFormat; }
    IDWriteTextFormat* GetBodyFormat() const { return m_uiLabelFormat; }
    IDWriteTextFormat* GetDialogueFormat() const { return m_uiLabelFormat; }
    IDWriteTextFormat* GetBadgeFormat() const { return m_uiBadgeFormat; }
    IDWriteTextFormat* GetCodeFormat() const { return m_monoCodeFormat; }
    IDWriteTextFormat* GetLogFormat() const { return m_monoLogFormat; }

    bool HasJetBrainsMono() const { return m_fontLoaded; }
    const std::wstring& GetFontFamilyName() const { return m_fontFamily; }
    const std::wstring& GetUiFontFamilyName() const { return m_uiFamily; }
    IDWriteFontCollection* GetFontCollection() const { return m_fontCollection; }

private:
    void LoadJetBrainsFonts();
    void InitUiFontFamily();

    IDWriteFactory* m_dwriteFactory = nullptr;
    IDWriteFontCollection* m_fontCollection = nullptr;
    bool m_fontLoaded = false;
    std::wstring m_fontFamily = L"JetBrains Mono";
    std::wstring m_uiFamily = L"Segoe UI";

    std::vector<HANDLE> m_memFontHandles;
    std::vector<std::wstring> m_registeredDiskFonts;

    // UI Sans-serif formats
    IDWriteTextFormat* m_uiTitleFormat = nullptr;
    IDWriteTextFormat* m_uiHeaderFormat = nullptr;
    IDWriteTextFormat* m_uiSubheadingFormat = nullptr;
    IDWriteTextFormat* m_uiLabelFormat = nullptr;
    IDWriteTextFormat* m_uiButtonFormat = nullptr;
    IDWriteTextFormat* m_uiBadgeFormat = nullptr;
    IDWriteTextFormat* m_uiMetricFormat = nullptr;

    // Monospace formats
    IDWriteTextFormat* m_monoCodeFormat = nullptr;
    IDWriteTextFormat* m_monoLogFormat = nullptr;
    IDWriteTextFormat* m_monoHeaderFormat = nullptr;
    IDWriteTextFormat* m_monoBadgeFormat = nullptr;
};

} // namespace Koltzi
