#include "SpeechBubble.h"
#include <algorithm>

namespace Koltzi {

SpeechBubble::SpeechBubble() {
    SetDialogue("Boo! I'm Koltzi! Drag and drop any PE binary (.exe / .dll) onto me and I'll sniff out hidden threats!", true);
}

SpeechBubble::~SpeechBubble() {
    DiscardDeviceResources();
}

HRESULT SpeechBubble::Initialize(ID2D1RenderTarget* rt, IDWriteFactory* dwriteFactory, const std::wstring& fontFamily) {
    DiscardDeviceResources();
    m_dwriteFactory = dwriteFactory;
    rt->GetFactory(&m_d2dFactory);

    HRESULT hr = S_OK;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &m_textWhiteBrush);
    if (FAILED(hr)) return hr;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(0.58f, 0.65f, 0.75f, 1.0f), &m_textMutedBrush);
    if (FAILED(hr)) return hr;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.74f, 0.97f, 1.0f), &m_accentBrush);
    if (FAILED(hr)) return hr;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.09f, 0.16f, 0.93f), &m_bgBrush);
    if (FAILED(hr)) return hr;

    hr = rt->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.85f), &m_borderBrush);
    if (FAILED(hr)) return hr;

    if (m_dwriteFactory) {
        const wchar_t* family = fontFamily.empty() ? L"Segoe UI" : fontFamily.c_str();

        // Badge format
        m_dwriteFactory->CreateTextFormat(
            family, nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            10.5f, L"en-us", &m_badgeFormat
        );

        // Dialogue format
        m_dwriteFactory->CreateTextFormat(
            family, nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            13.5f, L"en-us", &m_dialogueFormat
        );

        // HUD Heading format
        m_dwriteFactory->CreateTextFormat(
            family, nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            12.0f, L"en-us", &m_hudHeadingFormat
        );

        // HUD Body format
        m_dwriteFactory->CreateTextFormat(
            family, nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            11.0f, L"en-us", &m_hudTextFormat
        );

        // HUD Code format
        m_dwriteFactory->CreateTextFormat(
            L"Consolas", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            10.5f, L"en-us", &m_hudCodeFormat
        );
    }

    return S_OK;
}

void SpeechBubble::DiscardDeviceResources() {
    SafeRelease(m_textWhiteBrush);
    SafeRelease(m_textMutedBrush);
    SafeRelease(m_accentBrush);
    SafeRelease(m_bgBrush);
    SafeRelease(m_borderBrush);

    SafeRelease(m_badgeFormat);
    SafeRelease(m_dialogueFormat);
    SafeRelease(m_hudHeadingFormat);
    SafeRelease(m_hudTextFormat);
    SafeRelease(m_hudCodeFormat);

    SafeRelease(m_d2dFactory);
}

void SpeechBubble::SetDialogue(const std::string& text, bool immediate) {
    m_fullDialogue = text;
    m_fullDialogueWide = Utf8ToWide(text);
    m_charTimer = 0.0f;
    m_cursorBlink = 0.0f;

    if (immediate) {
        m_visibleChars = m_fullDialogueWide.size();
        m_isTyping = false;
    } else {
        m_visibleChars = 0;
        m_isTyping = true;
    }
}

void SpeechBubble::ToggleExpandedDetails() {
    m_showExpandedDetails = !m_showExpandedDetails;
}

bool SpeechBubble::HitTest(float x, float y) const {
    return (x >= m_currentBounds.left && x <= m_currentBounds.right &&
            y >= m_currentBounds.top  && y <= m_currentBounds.bottom);
}

void SpeechBubble::Update(float dt) {
    // Typewriter step: ~42 chars per second
    if (m_isTyping) {
        m_charTimer += dt;
        while (m_charTimer >= 0.024f && m_visibleChars < m_fullDialogueWide.size()) {
            m_visibleChars++;
            m_charTimer -= 0.024f;
        }
        if (m_visibleChars >= m_fullDialogueWide.size()) {
            m_isTyping = false;
        }
    }

    // Cursor blink timer
    m_cursorBlink += dt;
    if (m_cursorBlink > 0.8f) m_cursorBlink = 0.0f;

    // Smooth animation for expanding HUD
    float targetExpanded = m_showExpandedDetails ? 1.0f : 0.0f;
    m_expandedAnim += (targetExpanded - m_expandedAnim) * std::min(1.0f, dt * 10.0f);
}

void SpeechBubble::Render(ID2D1RenderTarget* rt, const TriageReport* currentReport, GhostMood currentMood) {
    (void)currentReport;
    RenderAt(rt, D2D1::RectF(24.0f, 250.0f, 324.0f, 420.0f), currentMood);
}

void SpeechBubble::RenderAt(ID2D1RenderTarget* rt, const D2D1_RECT_F& cardRect, GhostMood mood) {
    if (!rt) return;

    // Determine mood color
    D2D1_COLOR_F glowColor;
    std::wstring badgeText;
    switch (mood) {
    case GhostMood::Alarmed:
        glowColor = D2D1::ColorF(0.94f, 0.27f, 0.27f, 0.90f);
        badgeText = L"[THREAT: MALICIOUS]";
        break;
    case GhostMood::Puzzled:
        glowColor = D2D1::ColorF(0.96f, 0.62f, 0.04f, 0.90f);
        badgeText = L"[STATUS: SUSPICIOUS / PACKED]";
        break;
    case GhostMood::Happy:
        glowColor = D2D1::ColorF(0.06f, 0.73f, 0.51f, 0.90f);
        badgeText = L"[STATUS: ALL CLEAR / BENIGN]";
        break;
    case GhostMood::Sniffing:
        glowColor = D2D1::ColorF(0.18f, 0.83f, 0.75f, 0.90f);
        badgeText = L"[ANALYZING PE INSTRUCTIONS...]";
        break;
    case GhostMood::Idle:
    default:
        glowColor = D2D1::ColorF(0.22f, 0.74f, 0.97f, 0.80f);
        badgeText = L"[KOLTZI // OFFLINE TRIAGE]";
        break;
    }

    m_currentBounds = cardRect;

    // 1. Draw rounded container background
    RenderGlassBackground(rt, cardRect, glowColor);

    // 2. Draw status pill badge
    D2D1_RECT_F badgeRect = D2D1::RectF(cardRect.left + 14.0f, cardRect.top + 12.0f, cardRect.right - 14.0f, cardRect.top + 32.0f);
    RenderBadge(rt, badgeRect, badgeText, glowColor);

    // 3. Draw Dialogue Text with typewriter reveal inside safe bounds
    D2D1_RECT_F textRect = D2D1::RectF(cardRect.left + 14.0f, cardRect.top + 38.0f, cardRect.right - 14.0f, cardRect.bottom - 12.0f);
    RenderDialogueText(rt, textRect);
}

void SpeechBubble::RenderGlassBackground(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, D2D1_COLOR_F glowColor) {
    if (!m_bgBrush || !m_borderBrush) return;

    D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(rect, 14.0f, 14.0f);

    // Semi-transparent dark obsidian fill
    m_bgBrush->SetColor(D2D1::ColorF(0.06f, 0.09f, 0.16f, 0.94f));
    rt->FillRoundedRectangle(rrect, m_bgBrush);

    // Subtle inner gradient or glowing border
    m_borderBrush->SetColor(glowColor);
    rt->DrawRoundedRectangle(rrect, m_borderBrush, 1.8f);

    // Soft outer glow accent line at top
    ID2D1SolidColorBrush* topGlow = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(glowColor.r, glowColor.g, glowColor.b, 0.4f), &topGlow);
    if (topGlow) {
        rt->DrawLine(
            D2D1::Point2F(rect.left + 16.0f, rect.top + 1.0f),
            D2D1::Point2F(rect.right - 16.0f, rect.top + 1.0f),
            topGlow,
            2.5f
        );
        SafeRelease(topGlow);
    }
}

void SpeechBubble::RenderTail(ID2D1RenderTarget* rt, float tailStartX, float tailStartY, D2D1_COLOR_F glowColor) {
    if (!m_d2dFactory || !m_bgBrush || !m_borderBrush) return;

    ID2D1PathGeometry* tail = nullptr;
    if (SUCCEEDED(m_d2dFactory->CreatePathGeometry(&tail))) {
        ID2D1GeometrySink* sink = nullptr;
        if (SUCCEEDED(tail->Open(&sink))) {
            // Pointer tail reaching towards the ghost
            sink->BeginFigure(D2D1::Point2F(tailStartX + 1.0f, tailStartY - 10.0f), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(tailStartX - 16.0f, tailStartY + 4.0f));
            sink->AddLine(D2D1::Point2F(tailStartX + 1.0f, tailStartY + 14.0f));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->Close();
            SafeRelease(sink);

            rt->FillGeometry(tail, m_bgBrush);
            m_borderBrush->SetColor(glowColor);
            rt->DrawGeometry(tail, m_borderBrush, 1.8f);
        }
        SafeRelease(tail);
    }
}

void SpeechBubble::RenderBadge(ID2D1RenderTarget* rt, const D2D1_RECT_F& badgeRect, const std::wstring& text, D2D1_COLOR_F color) {
    if (!m_badgeFormat || !m_accentBrush) return;

    // Pill background
    ID2D1SolidColorBrush* pillBg = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(color.r, color.g, color.b, 0.16f), &pillBg);
    if (pillBg) {
        D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(
            D2D1::RectF(badgeRect.left, badgeRect.top, badgeRect.left + 240.0f, badgeRect.bottom),
            4.0f, 4.0f
        );
        rt->FillRoundedRectangle(pill, pillBg);
        SafeRelease(pillBg);
    }

    m_accentBrush->SetColor(color);
    rt->DrawText(
        text.c_str(),
        (UINT32)text.size(),
        m_badgeFormat,
        D2D1::RectF(badgeRect.left + 6.0f, badgeRect.top + 2.0f, badgeRect.right, badgeRect.bottom),
        m_accentBrush
    );
}

void SpeechBubble::RenderDialogueText(ID2D1RenderTarget* rt, const D2D1_RECT_F& textRect) {
    if (!m_dialogueFormat || !m_textWhiteBrush) return;

    std::wstring revealed = m_fullDialogueWide.substr(0, m_visibleChars);

    // Append typewriter cursor if still typing or blinking
    if (m_isTyping || (m_cursorBlink < 0.4f && m_visibleChars < m_fullDialogueWide.size())) {
        revealed += L" ▌";
    }

    rt->DrawText(
        revealed.c_str(),
        (UINT32)revealed.size(),
        m_dialogueFormat,
        textRect,
        m_textWhiteBrush
    );
}

void SpeechBubble::RenderExpandedHUD(ID2D1RenderTarget* rt, const D2D1_RECT_F& hudRect, const TriageReport* report) {
    (void)rt; (void)hudRect; (void)report;
}

} // namespace Koltzi
