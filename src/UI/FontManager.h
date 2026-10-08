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

    // Accessors for Creato Display and specialized typography
    IDWriteTextFormat* GetTitleFormat() const { return m_titleFormat; }
    IDWriteTextFormat* GetHeaderFormat() const { return m_headerFormat; }
    IDWriteTextFormat* GetSubheadingFormat() const { return m_subheadingFormat; }
    IDWriteTextFormat* GetButtonFormat() const { return m_buttonFormat; }
    IDWriteTextFormat* GetBodyFormat() const { return m_bodyFormat; }
    IDWriteTextFormat* GetDialogueFormat() const { return m_dialogueFormat; }
    IDWriteTextFormat* GetBadgeFormat() const { return m_badgeFormat; }
    IDWriteTextFormat* GetCodeFormat() const { return m_codeFormat; }
    IDWriteTextFormat* GetLogFormat() const { return m_logFormat; }

    bool HasCreatoDisplay() const { return m_creatoLoaded; }
    const std::wstring& GetFontFamilyName() const { return m_fontFamily; }

private:
    void LoadEmbeddedFonts();
    void LoadDiskFonts();

    IDWriteFactory* m_dwriteFactory = nullptr;
    bool m_creatoLoaded = false;
    std::wstring m_fontFamily = L"Creato Display";

    std::vector<HANDLE> m_memFontHandles;
    std::vector<std::wstring> m_registeredDiskFonts;

    IDWriteTextFormat* m_titleFormat = nullptr;
    IDWriteTextFormat* m_headerFormat = nullptr;
    IDWriteTextFormat* m_subheadingFormat = nullptr;
    IDWriteTextFormat* m_buttonFormat = nullptr;
    IDWriteTextFormat* m_bodyFormat = nullptr;
    IDWriteTextFormat* m_dialogueFormat = nullptr;
    IDWriteTextFormat* m_badgeFormat = nullptr;
    IDWriteTextFormat* m_codeFormat = nullptr;
    IDWriteTextFormat* m_logFormat = nullptr;
};

} // namespace Koltzi
