#pragma once
#include "Common.h"
#include "Core/TriageReport.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <string>

namespace Koltzi {

class SpeechBubble {
public:
    SpeechBubble();
    ~SpeechBubble();

    HRESULT Initialize(ID2D1RenderTarget* rt, IDWriteFactory* dwriteFactory, const std::wstring& fontFamily = L"Creato Display");
    void DiscardDeviceResources();

    void Update(float deltaSeconds);
    void Render(ID2D1RenderTarget* rt, const TriageReport* currentReport, GhostMood currentMood);
    void RenderAt(ID2D1RenderTarget* rt, const D2D1_RECT_F& cardRect, GhostMood currentMood);

    void SetDialogue(const std::string& text, bool immediate = false);
    void ToggleExpandedDetails();
    bool IsExpanded() const { return m_showExpandedDetails; }

    bool HitTest(float x, float y) const;

private:
    void RenderGlassBackground(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, D2D1_COLOR_F glowColor);
    void RenderTail(ID2D1RenderTarget* rt, float tailStartX, float tailStartY, D2D1_COLOR_F glowColor);
    void RenderBadge(ID2D1RenderTarget* rt, const D2D1_RECT_F& badgeRect, const std::wstring& text, D2D1_COLOR_F color);
    void RenderDialogueText(ID2D1RenderTarget* rt, const D2D1_RECT_F& textRect);
    void RenderExpandedHUD(ID2D1RenderTarget* rt, const D2D1_RECT_F& hudRect, const TriageReport* report);

    IDWriteFactory* m_dwriteFactory = nullptr;
    ID2D1Factory* m_d2dFactory = nullptr;

    IDWriteTextFormat* m_badgeFormat = nullptr;
    IDWriteTextFormat* m_dialogueFormat = nullptr;
    IDWriteTextFormat* m_hudHeadingFormat = nullptr;
    IDWriteTextFormat* m_hudTextFormat = nullptr;
    IDWriteTextFormat* m_hudCodeFormat = nullptr;

    ID2D1SolidColorBrush* m_textWhiteBrush = nullptr;
    ID2D1SolidColorBrush* m_textMutedBrush = nullptr;
    ID2D1SolidColorBrush* m_accentBrush = nullptr;
    ID2D1SolidColorBrush* m_bgBrush = nullptr;
    ID2D1SolidColorBrush* m_borderBrush = nullptr;

    // Typewriter effect state
    std::string m_fullDialogue;
    std::wstring m_fullDialogueWide;
    size_t m_visibleChars = 0;
    float m_charTimer = 0.0f;
    float m_cursorBlink = 0.0f;
    bool m_isTyping = false;

    // Expandable details toggle
    bool m_showExpandedDetails = false;
    float m_expandedAnim = 0.0f; // 0 to 1 smooth expansion

    D2D1_RECT_F m_currentBounds = { 0, 0, 0, 0 };
};

} // namespace Koltzi
