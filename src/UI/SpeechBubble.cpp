#include "SpeechBubble.h"
#include <algorithm>

namespace Koltzi {

SpeechBubble::SpeechBubble() {
    SetDialogue("Boo! I'm Koltzi! Drag and drop any PE binary (.exe / .dll) onto me and I'll sniff out hidden threats!", true);
}

SpeechBubble::~SpeechBubble() {
    DiscardDeviceResources();
}

HRESULT SpeechBubble::Initialize(ID2D1RenderTarget* rt, IDWriteFactory* dwriteFactory) {
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
        // Badge format
        m_dwriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            10.5f, L"en-us", &m_badgeFormat
        );

        // Dialogue format
        m_dwriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            13.5f, L"en-us", &m_dialogueFormat
        );

        // HUD Heading format
        m_dwriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            12.0f, L"en-us", &m_hudHeadingFormat
        );

        // HUD Body format
        m_dwriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr,
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

void SpeechBubble::Render(ID2D1RenderTarget* rt, const TriageReport* report, GhostMood mood) {
    if (!rt) return;

    // Determine mood color
    D2D1_COLOR_F glowColor;
    std::wstring badgeText;
    switch (mood) {
    case GhostMood::Alarmed:
        glowColor = D2D1::ColorF(0.94f, 0.27f, 0.27f, 0.90f);
        badgeText = (report && report->threatScore >= 60)
            ? L"[THREAT: MALICIOUS] (" + std::to_wstring(report->threatScore) + L"/100)"
            : L"[THREAT: SUSPICIOUS]";
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

    // Coordinates: window size is 580 x 440
    // Ghost is on left (cx = 100, cy = 160)
    // Speech bubble is on right: x from 180 to 555
    float bubbleX = 180.0f;
    float bubbleY = 40.0f;
    float bubbleW = 370.0f;

    // Dynamic height based on expanded HUD animation
    float baseH = 155.0f;
    float expandedExtraH = 220.0f;
    float bubbleH = baseH + m_expandedAnim * expandedExtraH;

    D2D1_RECT_F bubbleRect = D2D1::RectF(bubbleX, bubbleY, bubbleX + bubbleW, bubbleY + bubbleH);
    m_currentBounds = bubbleRect;

    // 1. Draw Glassmorphism background
    RenderGlassBackground(rt, bubbleRect, glowColor);

    // 2. Draw speech bubble tail pointing to the ghost
    RenderTail(rt, bubbleX, bubbleY + 60.0f, glowColor);

    // 3. Draw status pill badge
    D2D1_RECT_F badgeRect = D2D1::RectF(bubbleX + 16.0f, bubbleY + 14.0f, bubbleX + bubbleW - 16.0f, bubbleY + 34.0f);
    RenderBadge(rt, badgeRect, badgeText, glowColor);

    // 4. Draw Dialogue Text with typewriter reveal
    D2D1_RECT_F textRect = D2D1::RectF(bubbleX + 16.0f, bubbleY + 42.0f, bubbleX + bubbleW - 16.0f, bubbleY + 125.0f);
    RenderDialogueText(rt, textRect);

    // 5. Draw Expandable HUD toggle button hint
    float toggleY = bubbleY + 128.0f;
    if (m_accentBrush && m_badgeFormat) {
        m_accentBrush->SetColor(glowColor);
        std::wstring hintText = (m_showExpandedDetails)
            ? L"[-] Click to collapse Technical HUD"
            : (report ? L"[+] Click to expand Low-Level Findings HUD" : L"[Drop any PE binary (.exe / .dll) to inspect]");

        D2D1_RECT_F hintRect = D2D1::RectF(bubbleX + 16.0f, toggleY, bubbleX + bubbleW - 16.0f, toggleY + 20.0f);
        rt->DrawText(hintText.c_str(), (UINT32)hintText.size(), m_badgeFormat, hintRect, m_accentBrush);
    }

    // 6. If expanded, render technical details HUD
    if (m_expandedAnim > 0.05f && report) {
        D2D1_RECT_F hudRect = D2D1::RectF(
            bubbleX + 14.0f,
            bubbleY + 155.0f,
            bubbleX + bubbleW - 14.0f,
            bubbleY + bubbleH - 12.0f
        );
        RenderExpandedHUD(rt, hudRect, report);
    }
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
    if (!report || !m_hudHeadingFormat || !m_hudTextFormat || !m_hudCodeFormat) return;

    // Subtle dark divider line
    if (m_textMutedBrush) {
        m_textMutedBrush->SetColor(D2D1::ColorF(0.25f, 0.32f, 0.44f, 0.7f));
        rt->DrawLine(
            D2D1::Point2F(hudRect.left, hudRect.top - 6.0f),
            D2D1::Point2F(hudRect.right, hudRect.top - 6.0f),
            m_textMutedBrush,
            1.0f
        );
    }

    float curY = hudRect.top;

    // 1. File Metadata & Analysis Latency
    std::wstring meta = Utf8ToWide(report->fileName + " (" + report->machineType + ", " + FormatFileSize(report->fileSize) + ")");
    m_textWhiteBrush->SetColor(D2D1::ColorF(0.95f, 0.97f, 1.0f, 1.0f));
    rt->DrawText(
        meta.c_str(), (UINT32)meta.size(), m_hudHeadingFormat,
        D2D1::RectF(hudRect.left, curY, hudRect.right - 80.0f, curY + 18.0f),
        m_textWhiteBrush
    );

    // Latency badge: e.g. "18.2 ms"
    std::wstring latency = std::format(L"{:.1f} ms", report->analysisTimeMs);
    m_textMutedBrush->SetColor(D2D1::ColorF(0.38f, 0.85f, 0.65f, 1.0f));
    rt->DrawText(
        latency.c_str(), (UINT32)latency.size(), m_hudCodeFormat,
        D2D1::RectF(hudRect.right - 75.0f, curY, hudRect.right, curY + 18.0f),
        m_textMutedBrush
    );

    curY += 20.0f;

    // 2. Threat Meter Bar (0 to 100)
    float barW = hudRect.right - hudRect.left;
    float barH = 6.0f;
    D2D1_ROUNDED_RECT barTrack = D2D1::RoundedRect(D2D1::RectF(hudRect.left, curY, hudRect.left + barW, curY + barH), 3.0f, 3.0f);

    ID2D1SolidColorBrush* trackBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.23f, 0.32f, 0.8f), &trackBrush);
    if (trackBrush) {
        rt->FillRoundedRectangle(barTrack, trackBrush);
        SafeRelease(trackBrush);
    }

    float fillW = barW * (static_cast<float>(report->threatScore) / 100.0f);
    if (fillW > 4.0f) {
        D2D1_COLOR_F barFillColor = (report->threatScore >= 60) ? D2D1::ColorF(0.94f, 0.27f, 0.27f, 1.0f) :
                                    (report->threatScore >= 30) ? D2D1::ColorF(0.96f, 0.62f, 0.04f, 1.0f) :
                                                                  D2D1::ColorF(0.06f, 0.73f, 0.51f, 1.0f);

        ID2D1SolidColorBrush* fillBrush = nullptr;
        rt->CreateSolidColorBrush(barFillColor, &fillBrush);
        if (fillBrush) {
            D2D1_ROUNDED_RECT fillTrack = D2D1::RoundedRect(D2D1::RectF(hudRect.left, curY, hudRect.left + fillW, curY + barH), 3.0f, 3.0f);
            rt->FillRoundedRectangle(fillTrack, fillBrush);
            SafeRelease(fillBrush);
        }
    }

    curY += 12.0f;

    // 3. Section Breakdown Chips
    std::wstring secStr = L"Sections (" + std::to_wstring(report->sections.size()) + L"): ";
    for (size_t i = 0; i < std::min<size_t>(report->sections.size(), 4); ++i) {
        const auto& s = report->sections[i];
        secStr += Utf8ToWide(s.name) + std::format(L" [H={:.2f}{}] ", s.entropy, s.isSuspiciousEntropy ? L"!PACKED" : L"");
    }
    m_textMutedBrush->SetColor(D2D1::ColorF(0.65f, 0.72f, 0.84f, 1.0f));
    rt->DrawText(
        secStr.c_str(), (UINT32)secStr.size(), m_hudCodeFormat,
        D2D1::RectF(hudRect.left, curY, hudRect.right, curY + 16.0f),
        m_textMutedBrush
    );

    curY += 18.0f;

    // 4. Critical Findings Bullet Points
    size_t linesRendered = 0;
    for (const auto& detail : report->technicalDetails) {
        if (linesRendered >= 4) break;

        std::wstring line = Utf8ToWide(detail);
        D2D1_COLOR_F lineColor = D2D1::ColorF(0.85f, 0.90f, 0.98f, 1.0f);
        if (detail.starts_with("[CRITICAL]")) {
            lineColor = D2D1::ColorF(0.96f, 0.35f, 0.35f, 1.0f);
        } else if (detail.starts_with("[WARNING]")) {
            lineColor = D2D1::ColorF(0.98f, 0.75f, 0.25f, 1.0f);
        }

        ID2D1SolidColorBrush* lineBrush = nullptr;
        rt->CreateSolidColorBrush(lineColor, &lineBrush);
        if (lineBrush) {
            rt->DrawText(
                line.c_str(), (UINT32)line.size(), m_hudCodeFormat,
                D2D1::RectF(hudRect.left, curY, hudRect.right, curY + 16.0f),
                lineBrush
            );
            SafeRelease(lineBrush);
        }

        curY += 16.0f;
        linesRendered++;
    }

    // 5. Offline Triage Guarantee Footer
    m_textMutedBrush->SetColor(D2D1::ColorF(0.45f, 0.52f, 0.65f, 0.85f));
    std::wstring footer = L"Offline Triage | Zero-Cloud | Zydis Linear Sweeper";
    rt->DrawText(
        footer.c_str(), (UINT32)footer.size(), m_hudCodeFormat,
        D2D1::RectF(hudRect.left, curY + 4.0f, hudRect.right, curY + 20.0f),
        m_textMutedBrush
    );
}

} // namespace Koltzi
