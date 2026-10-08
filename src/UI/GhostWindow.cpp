#include "GhostWindow.h"
#include <algorithm>
#include <format>

namespace Koltzi {

constexpr UINT_PTR TIMER_ANIMATION_ID = 1001;

enum MenuCommands {
    IDM_SCAN_FILE = 2001,
    IDM_SAMPLE_CLEAN = 2002,
    IDM_SAMPLE_PACKED = 2003,
    IDM_SAMPLE_SYSCALL_PEB = 2004,
    IDM_SAMPLE_CRED_STEALER = 2005,
    IDM_SAMPLE_INJECTION = 2006,
    IDM_TOGGLE_HUD = 2007,
    IDM_RESET = 2008,
    IDM_EXIT = 2009,
    IDM_TOGGLE_AUDIT = 2010
};

GhostWindow::GhostWindow() = default;

GhostWindow::~GhostWindow() {
    Destroy();
}

bool GhostWindow::Create() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"KoltziMainWindowClass";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    // Initial position: Centered on screen
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int posX = (workArea.right - workArea.left - m_width) / 2;
    int posY = (workArea.bottom - workArea.top - m_height) / 2;
    if (posX < 20) posX = 20;
    if (posY < 20) posY = 20;

    m_hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        wc.lpszClassName,
        L"Koltzi - Malware Triage Agent",
        WS_OVERLAPPEDWINDOW,
        posX, posY, m_width, m_height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return false;

    DragAcceptFiles(m_hwnd, TRUE);

    if (!CreateDeviceIndependentResources()) return false;
    if (!CreateDeviceResources()) return false;

    // Start 60 FPS animation timer (~16 ms)
    SetTimer(m_hwnd, TIMER_ANIMATION_ID, 16, nullptr);

    return true;
}

void GhostWindow::Show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        Render();
    }
}

void GhostWindow::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void GhostWindow::Destroy() {
    if (m_hwnd) {
        KillTimer(m_hwnd, TIMER_ANIMATION_ID);
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
    DiscardDeviceResources();
    SafeRelease(m_titleFormat);
    SafeRelease(m_subtitleFormat);
    SafeRelease(m_headingFormat);
    SafeRelease(m_subheadingFormat);
    SafeRelease(m_bodyFormat);
    SafeRelease(m_codeFormat);
    SafeRelease(m_badgeFormat);
    SafeRelease(m_buttonFormat);
    m_fontManager.Shutdown();
    SafeRelease(m_dwriteFactory);
    SafeRelease(m_d2dFactory);
}

bool GhostWindow::CreateDeviceIndependentResources() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_dwriteFactory)
    );
    if (FAILED(hr)) return false;

    m_fontManager.Initialize(m_dwriteFactory);

    m_titleFormat = m_fontManager.GetTitleFormat();
    if (m_titleFormat) m_titleFormat->AddRef();

    m_headingFormat = m_fontManager.GetHeaderFormat();
    if (m_headingFormat) m_headingFormat->AddRef();

    m_subheadingFormat = m_fontManager.GetSubheadingFormat();
    if (m_subheadingFormat) m_subheadingFormat->AddRef();

    m_buttonFormat = m_fontManager.GetButtonFormat();
    if (m_buttonFormat) m_buttonFormat->AddRef();

    m_bodyFormat = m_fontManager.GetBodyFormat();
    if (m_bodyFormat) m_bodyFormat->AddRef();

    m_badgeFormat = m_fontManager.GetBadgeFormat();
    if (m_badgeFormat) m_badgeFormat->AddRef();

    m_codeFormat = m_fontManager.GetCodeFormat();
    if (m_codeFormat) m_codeFormat->AddRef();

    m_dwriteFactory->CreateTextFormat(
        m_fontManager.GetFontFamilyName().c_str(), nullptr,
        DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        10.5f, L"en-us", &m_subtitleFormat
    );

    return true;
}

bool GhostWindow::CreateDeviceResources() {
    if (!m_d2dFactory || !m_dwriteFactory || !m_hwnd) return false;

    if (!m_renderTarget) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        m_width = rc.right - rc.left;
        m_height = rc.bottom - rc.top;
        if (m_width < 100) m_width = 1140;
        if (m_height < 100) m_height = 760;

        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
            0, 0,
            D2D1_RENDER_TARGET_USAGE_NONE,
            D2D1_FEATURE_LEVEL_DEFAULT
        );

        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
            m_hwnd,
            D2D1::SizeU(m_width, m_height),
            D2D1_PRESENT_OPTIONS_IMMEDIATELY
        );

        HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &m_renderTarget);
        if (FAILED(hr)) return false;

        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.08f, 0.11f, 1.0f), &m_bgBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.09f, 0.11f, 0.16f, 1.0f), &m_panelBgBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.22f, 0.30f, 1.0f), &m_panelBorderBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.15f, 0.21f, 1.0f), &m_cardBgBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.97f, 1.00f, 1.0f), &m_textWhiteBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.60f, 0.67f, 0.76f, 1.0f), &m_textMutedBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.40f, 0.46f, 0.54f, 1.0f), &m_textDimBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.22f, 0.74f, 0.97f, 1.0f), &m_accentBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.94f, 0.27f, 0.27f, 1.0f), &m_redBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.62f, 0.04f, 1.0f), &m_yellowBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.73f, 0.51f, 1.0f), &m_greenBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.13f, 0.17f, 0.24f, 1.0f), &m_buttonBgBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.26f, 0.36f, 1.0f), &m_buttonHoverBrush);

        m_ghostRenderer.Initialize(m_renderTarget, m_dwriteFactory);
        m_speechBubble.Initialize(m_renderTarget, m_dwriteFactory, m_fontManager.GetFontFamilyName());
    }

    return true;
}

void GhostWindow::DiscardDeviceResources() {
    m_ghostRenderer.DiscardDeviceResources();
    m_speechBubble.DiscardDeviceResources();

    SafeRelease(m_bgBrush);
    SafeRelease(m_panelBgBrush);
    SafeRelease(m_panelBorderBrush);
    SafeRelease(m_cardBgBrush);
    SafeRelease(m_textWhiteBrush);
    SafeRelease(m_textMutedBrush);
    SafeRelease(m_textDimBrush);
    SafeRelease(m_accentBrush);
    SafeRelease(m_redBrush);
    SafeRelease(m_yellowBrush);
    SafeRelease(m_greenBrush);
    SafeRelease(m_buttonBgBrush);
    SafeRelease(m_buttonHoverBrush);

    SafeRelease(m_renderTarget);
}

void GhostWindow::SetReport(std::shared_ptr<TriageReport> report) {
    m_currentReport = report;
    if (report) {
        SetMood(report->mood);
        SetDialogue(report->personalityDialogue, false);
    }
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void GhostWindow::SetMood(GhostMood mood) {
    m_currentMood = mood;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void GhostWindow::SetDialogue(const std::string& text, bool immediate) {
    m_speechBubble.SetDialogue(text, immediate);
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void GhostWindow::ToggleHUD() {
    m_speechBubble.ToggleExpandedDetails();
}

void GhostWindow::Update(float dt) {
    m_ghostRenderer.Update(dt, m_currentMood);
    m_speechBubble.Update(dt);
}

void GhostWindow::Render() {
    if (!CreateDeviceResources() || !m_renderTarget) return;

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(0.06f, 0.08f, 0.11f, 1.0f));

    RenderTopHeader(m_renderTarget);
    RenderLeftPanel(m_renderTarget);
    RenderRightDashboard(m_renderTarget);
    RenderFooter(m_renderTarget);

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

void GhostWindow::RenderTopHeader(ID2D1RenderTarget* rt) {
    float headerH = 54.0f;
    D2D1_RECT_F headerRect = D2D1::RectF(0.0f, 0.0f, static_cast<float>(m_width), headerH);

    // Header Background
    if (m_panelBgBrush) {
        rt->FillRectangle(headerRect, m_panelBgBrush);
    }
    if (m_panelBorderBrush) {
        rt->DrawLine(
            D2D1::Point2F(0.0f, headerH),
            D2D1::Point2F(static_cast<float>(m_width), headerH),
            m_panelBorderBrush, 1.0f
        );
    }

    // Left Branding
    if (m_accentBrush && m_titleFormat) {
        std::wstring title = L"KOLTZI";
        rt->DrawText(title.c_str(), (UINT32)title.size(), m_titleFormat, D2D1::RectF(18.0f, 14.0f, 110.0f, 44.0f), m_accentBrush);
    }
    if (m_textMutedBrush && m_subtitleFormat) {
        std::wstring sub = L"// OFFLINE PE STATIC TRIAGE & REVERSE ENGINEERING";
        rt->DrawText(sub.c_str(), (UINT32)sub.size(), m_subtitleFormat, D2D1::RectF(95.0f, 18.0f, 420.0f, 44.0f), m_textMutedBrush);
    }

    // Setup Toolbar Buttons
    m_buttons.clear();
    float btnRight = static_cast<float>(m_width) - 16.0f;
    float btnY = 12.0f;
    float btnH = 30.0f;

    struct BtnDef { int id; std::wstring label; float width; };
    std::vector<BtnDef> btnDefs = {
        { IDM_SCAN_FILE, L"Open PE...", 95.0f },
        { IDM_TOGGLE_AUDIT, m_showAuditLog ? L"PE Findings" : L"Audit Log", 100.0f },
        { IDM_SAMPLE_CLEAN, L"Clean PE", 75.0f },
        { IDM_SAMPLE_PACKED, L"Packed", 68.0f },
        { IDM_SAMPLE_SYSCALL_PEB, L"Syscall+PEB", 95.0f },
        { IDM_SAMPLE_CRED_STEALER, L"Cred Stealer", 95.0f },
        { IDM_SAMPLE_INJECTION, L"Injection", 78.0f }
    };

    // Layout buttons from right to left
    for (int i = static_cast<int>(btnDefs.size()) - 1; i >= 0; --i) {
        float btnW = btnDefs[i].width;
        float btnX = btnRight - btnW;
        ToolbarButton tb;
        tb.id = btnDefs[i].id;
        tb.label = btnDefs[i].label;
        tb.rect = D2D1::RectF(btnX, btnY, btnRight, btnY + btnH);
        m_buttons.push_back(tb);
        btnRight = btnX - 8.0f;
    }

    // Draw buttons
    for (size_t i = 0; i < m_buttons.size(); ++i) {
        const auto& b = m_buttons[i];
        bool isHover = (m_hoveredButton == b.id);
        D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(b.rect, 5.0f, 5.0f);

        ID2D1SolidColorBrush* bg = isHover ? m_buttonHoverBrush : m_buttonBgBrush;
        if (bg) rt->FillRoundedRectangle(rrect, bg);

        ID2D1SolidColorBrush* border = isHover ? m_accentBrush : m_panelBorderBrush;
        if (border) rt->DrawRoundedRectangle(rrect, border, isHover ? 1.5f : 1.0f);

        ID2D1SolidColorBrush* txt = isHover ? m_textWhiteBrush : m_textMutedBrush;
        if (txt && m_buttonFormat) {
            DWRITE_TEXT_ALIGNMENT oldAlign = m_buttonFormat->GetTextAlignment();
            m_buttonFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawText(b.label.c_str(), (UINT32)b.label.size(), m_buttonFormat,
                D2D1::RectF(b.rect.left, b.rect.top + 5.0f, b.rect.right, b.rect.bottom), txt);
            m_buttonFormat->SetTextAlignment(oldAlign);
        }
    }
}

void GhostWindow::RenderLeftPanel(ID2D1RenderTarget* rt) {
    float panelX = 16.0f;
    float panelY = 68.0f;
    float panelW = 320.0f;
    float panelH = static_cast<float>(m_height) - panelY - 38.0f;

    D2D1_ROUNDED_RECT panelRect = D2D1::RoundedRect(D2D1::RectF(panelX, panelY, panelX + panelW, panelY + panelH), 10.0f, 10.0f);

    if (m_panelBgBrush) rt->FillRoundedRectangle(panelRect, m_panelBgBrush);
    if (m_panelBorderBrush) rt->DrawRoundedRectangle(panelRect, m_panelBorderBrush, 1.0f);

    // 1. Mascot Stage
    float cx = panelX + panelW * 0.5f;
    float cy = panelY + 105.0f;

    if (m_panelBorderBrush) {
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + 45.0f), 65.0f, 18.0f), m_panelBorderBrush, 1.0f);
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + 45.0f), 85.0f, 24.0f), m_panelBorderBrush, 1.0f);
    }

    // Render Animated Ghost Mascot
    m_ghostRenderer.Render(rt, cx, cy);

    // 2. Dialogue Speech Card
    float cardY = panelY + 185.0f;
    float cardH = 150.0f;
    D2D1_RECT_F bubbleRect = D2D1::RectF(panelX + 16.0f, cardY, panelX + panelW - 16.0f, cardY + cardH);
    m_speechBubble.RenderAt(rt, bubbleRect, m_currentMood);

    // 3. Drag and Drop Zone
    float dropY = cardY + cardH + 16.0f;
    float dropH = panelH - (dropY - panelY) - 16.0f;
    m_dropZoneRect = D2D1::RectF(panelX + 16.0f, dropY, panelX + panelW - 16.0f, dropY + dropH);

    D2D1_ROUNDED_RECT dropRRect = D2D1::RoundedRect(m_dropZoneRect, 8.0f, 8.0f);

    ID2D1SolidColorBrush* dropBg = m_dropHover ? m_buttonHoverBrush : m_cardBgBrush;
    if (dropBg) rt->FillRoundedRectangle(dropRRect, dropBg);

    ID2D1SolidColorBrush* dropBorder = m_dropHover ? m_accentBrush : m_panelBorderBrush;
    if (dropBorder) rt->DrawRoundedRectangle(dropRRect, dropBorder, m_dropHover ? 2.0f : 1.0f);

    float centerY = dropY + dropH * 0.5f;
    if (m_subheadingFormat && m_textWhiteBrush) {
        std::wstring dropTitle = m_dropHover ? L"DROP PE BINARY HERE" : L"DRAG & DROP BINARY";
        DWRITE_TEXT_ALIGNMENT old = m_subheadingFormat->GetTextAlignment();
        m_subheadingFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawText(dropTitle.c_str(), (UINT32)dropTitle.size(), m_subheadingFormat,
            D2D1::RectF(m_dropZoneRect.left, centerY - 24.0f, m_dropZoneRect.right, centerY), m_textWhiteBrush);
        m_subheadingFormat->SetTextAlignment(old);
    }

    if (m_subtitleFormat && m_textMutedBrush) {
        std::wstring dropSub = L"Accepts .exe, .dll, .sys\nClick to browse from disk";
        DWRITE_TEXT_ALIGNMENT old = m_subtitleFormat->GetTextAlignment();
        m_subtitleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawText(dropSub.c_str(), (UINT32)dropSub.size(), m_subtitleFormat,
            D2D1::RectF(m_dropZoneRect.left, centerY + 2.0f, m_dropZoneRect.right, centerY + 36.0f), m_textMutedBrush);
        m_subtitleFormat->SetTextAlignment(old);
    }
}

void GhostWindow::RenderRightDashboard(ID2D1RenderTarget* rt) {
    float dashX = 352.0f;
    float dashY = 68.0f;
    float dashW = static_cast<float>(m_width) - dashX - 16.0f;
    float dashH = static_cast<float>(m_height) - dashY - 38.0f;

    // --- CARD 1: Target Executable Identity & Security Verification ---
    float card1H = 202.0f;
    D2D1_ROUNDED_RECT card1Rect = D2D1::RoundedRect(D2D1::RectF(dashX, dashY, dashX + dashW, dashY + card1H), 10.0f, 10.0f);
    if (m_panelBgBrush) rt->FillRoundedRectangle(card1Rect, m_panelBgBrush);
    if (m_panelBorderBrush) rt->DrawRoundedRectangle(card1Rect, m_panelBorderBrush, 1.0f);

    if (m_currentReport && m_currentReport->parseSuccess) {
        // Line 1: File Name + Latency Pill + Threat Score Pill
        std::wstring fileName = Utf8ToWide(m_currentReport->fileName);
        if (m_titleFormat && m_textWhiteBrush) {
            rt->DrawText(fileName.c_str(), (UINT32)fileName.size(), m_titleFormat,
                D2D1::RectF(dashX + 16.0f, dashY + 10.0f, dashX + dashW - 220.0f, dashY + 34.0f), m_textWhiteBrush);
        }

        // Latency Badge
        std::wstring latency = std::format(L"{:.1f} ms", m_currentReport->analysisTimeMs);
        D2D1_ROUNDED_RECT latRect = D2D1::RoundedRect(D2D1::RectF(dashX + dashW - 210.0f, dashY + 10.0f, dashX + dashW - 120.0f, dashY + 30.0f), 4.0f, 4.0f);
        if (m_cardBgBrush) rt->FillRoundedRectangle(latRect, m_cardBgBrush);
        if (m_greenBrush) rt->DrawRoundedRectangle(latRect, m_greenBrush, 1.0f);
        if (m_badgeFormat && m_greenBrush) {
            DWRITE_TEXT_ALIGNMENT old = m_badgeFormat->GetTextAlignment();
            m_badgeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawText(latency.c_str(), (UINT32)latency.size(), m_badgeFormat,
                D2D1::RectF(latRect.rect.left, latRect.rect.top + 3.0f, latRect.rect.right, latRect.rect.bottom), m_greenBrush);
            m_badgeFormat->SetTextAlignment(old);
        }

        // Threat Verdict Pill
        std::wstring verdictPill = (m_currentReport->threatScore >= 60) ? L"MALICIOUS" :
                                   (m_currentReport->threatScore >= 20) ? L"SUSPICIOUS" : L"CLEAN";
        ID2D1SolidColorBrush* verdictColor = (m_currentReport->threatScore >= 60) ? m_redBrush :
                                             (m_currentReport->threatScore >= 20) ? m_yellowBrush : m_greenBrush;
        D2D1_ROUNDED_RECT verdRect = D2D1::RoundedRect(D2D1::RectF(dashX + dashW - 110.0f, dashY + 10.0f, dashX + dashW - 16.0f, dashY + 30.0f), 4.0f, 4.0f);
        if (m_cardBgBrush) rt->FillRoundedRectangle(verdRect, m_cardBgBrush);
        if (verdictColor) rt->DrawRoundedRectangle(verdRect, verdictColor, 1.0f);
        if (m_badgeFormat && verdictColor) {
            DWRITE_TEXT_ALIGNMENT old = m_badgeFormat->GetTextAlignment();
            m_badgeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawText(verdictPill.c_str(), (UINT32)verdictPill.size(), m_badgeFormat,
                D2D1::RectF(verdRect.rect.left, verdRect.rect.top + 3.0f, verdRect.rect.right, verdRect.rect.bottom), verdictColor);
            m_badgeFormat->SetTextAlignment(old);
        }

        // Line 2: Target File Path
        std::wstring filePath = m_currentReport->filePath;
        if (filePath.empty()) filePath = fileName;
        if (m_codeFormat && m_textDimBrush) {
            rt->DrawText(filePath.c_str(), (UINT32)filePath.size(), m_codeFormat,
                D2D1::RectF(dashX + 16.0f, dashY + 34.0f, dashX + dashW - 16.0f, dashY + 48.0f), m_textDimBrush);
        }

        // Line 3: 4 Metric Badges
        float chipW = (dashW - 32.0f - 30.0f) / 4.0f;
        float chipY = dashY + 52.0f;
        float chipH = 30.0f;

        struct MetricChip { std::wstring label; std::wstring value; };
        std::vector<MetricChip> chips = {
            { L"ARCH", Utf8ToWide(m_currentReport->machineType) },
            { L"SUBSYSTEM", Utf8ToWide(m_currentReport->subsystem) },
            { L"FILE SIZE", Utf8ToWide(FormatFileSize(m_currentReport->fileSize)) },
            { L"IMPORTS", std::to_wstring(m_currentReport->imports.size()) + L" DLLs" }
        };

        for (size_t i = 0; i < chips.size(); ++i) {
            float cX = dashX + 16.0f + i * (chipW + 10.0f);
            D2D1_ROUNDED_RECT cRect = D2D1::RoundedRect(D2D1::RectF(cX, chipY, cX + chipW, chipY + chipH), 4.0f, 4.0f);
            if (m_cardBgBrush) rt->FillRoundedRectangle(cRect, m_cardBgBrush);
            if (m_panelBorderBrush) rt->DrawRoundedRectangle(cRect, m_panelBorderBrush, 1.0f);

            if (m_badgeFormat && m_textDimBrush) {
                rt->DrawText(chips[i].label.c_str(), (UINT32)chips[i].label.size(), m_badgeFormat,
                    D2D1::RectF(cX + 8.0f, chipY + 2.0f, cX + chipW - 4.0f, chipY + 14.0f), m_textDimBrush);
            }
            if (m_subheadingFormat && m_textWhiteBrush) {
                rt->DrawText(chips[i].value.c_str(), (UINT32)chips[i].value.size(), m_subheadingFormat,
                    D2D1::RectF(cX + 8.0f, chipY + 13.0f, cX + chipW - 4.0f, chipY + 28.0f), m_textWhiteBrush);
            }
        }

        // Line 4: Cryptographic Hashes (SHA-256 & Imphash / MD5)
        float hashY = dashY + 88.0f;
        std::wstring sha256Str = L"SHA-256: " + Utf8ToWide(m_currentReport->sha256.empty() ? "N/A" : m_currentReport->sha256);
        std::wstring imphashStr = L"IMPHASH: " + Utf8ToWide(m_currentReport->imphash.empty() ? "N/A" : m_currentReport->imphash);

        if (m_codeFormat && m_textMutedBrush) {
            rt->DrawText(sha256Str.c_str(), (UINT32)sha256Str.size(), m_codeFormat,
                D2D1::RectF(dashX + 16.0f, hashY, dashX + dashW * 0.65f, hashY + 16.0f), m_textMutedBrush);
            rt->DrawText(imphashStr.c_str(), (UINT32)imphashStr.size(), m_codeFormat,
                D2D1::RectF(dashX + dashW * 0.66f, hashY, dashX + dashW - 16.0f, hashY + 16.0f), m_textMutedBrush);
        }

        // Line 5: Authenticode Digital Signature Status
        float certY = dashY + 108.0f;
        std::wstring certLabel;
        ID2D1SolidColorBrush* certBrush = m_textDimBrush;
        if (m_currentReport->signature.isValid) {
            certBrush = m_greenBrush;
            certLabel = L"[VALID CERT] Publisher: " + Utf8ToWide(m_currentReport->signature.signerSubject) +
                        L" | Issuer: " + Utf8ToWide(m_currentReport->signature.signerIssuer);
            if (m_currentReport->signature.isTrustedVendor) {
                certLabel += L" [TRUSTED VENDOR]";
            }
        } else if (m_currentReport->signature.isSigned) {
            certBrush = m_yellowBrush;
            certLabel = L"[UNTRUSTED/SELF-SIGNED] Subject: " + Utf8ToWide(m_currentReport->signature.signerSubject);
        } else {
            certBrush = m_textDimBrush;
            certLabel = L"[UNSIGNED BINARY] No Authenticode signature present";
        }

        if (m_codeFormat && certBrush) {
            rt->DrawText(certLabel.c_str(), (UINT32)certLabel.size(), m_codeFormat,
                D2D1::RectF(dashX + 16.0f, certY, dashX + dashW - 16.0f, certY + 16.0f), certBrush);
        }

        // Line 6: Threat Score & Shannon Entropy Progress Bars
        float meterY = dashY + 132.0f;
        float halfW = (dashW - 32.0f - 20.0f) * 0.5f;

        // Threat Score Meter
        std::wstring threatLabel = std::format(L"Threat Score: {} / 100 [{}]",
            m_currentReport->threatScore,
            (m_currentReport->threatScore >= 60 ? L"MALICIOUS" : m_currentReport->threatScore >= 20 ? L"SUSPICIOUS" : L"CLEAN"));

        ID2D1SolidColorBrush* threatColor = (m_currentReport->threatScore >= 60) ? m_redBrush :
                                            (m_currentReport->threatScore >= 20) ? m_yellowBrush : m_greenBrush;

        if (m_badgeFormat && threatColor) {
            rt->DrawText(threatLabel.c_str(), (UINT32)threatLabel.size(), m_badgeFormat,
                D2D1::RectF(dashX + 16.0f, meterY, dashX + 16.0f + halfW, meterY + 16.0f), threatColor);
        }

        D2D1_ROUNDED_RECT tTrack = D2D1::RoundedRect(D2D1::RectF(dashX + 16.0f, meterY + 18.0f, dashX + 16.0f + halfW, meterY + 26.0f), 3.0f, 3.0f);
        if (m_cardBgBrush) rt->FillRoundedRectangle(tTrack, m_cardBgBrush);
        float tFillW = halfW * (static_cast<float>(m_currentReport->threatScore) / 100.0f);
        if (tFillW > 3.0f && threatColor) {
            D2D1_ROUNDED_RECT tFill = D2D1::RoundedRect(D2D1::RectF(dashX + 16.0f, meterY + 18.0f, dashX + 16.0f + tFillW, meterY + 26.0f), 3.0f, 3.0f);
            rt->FillRoundedRectangle(tFill, threatColor);
        }

        // Entropy Meter
        float entX = dashX + 16.0f + halfW + 20.0f;
        std::wstring entLabel = std::format(L"Overall Shannon Entropy: {:.2f} / 8.00 (Threshold: 7.20)", m_currentReport->overallEntropy);
        ID2D1SolidColorBrush* entColor = (m_currentReport->overallEntropy > 7.2f) ? m_yellowBrush : m_accentBrush;

        if (m_badgeFormat && entColor) {
            rt->DrawText(entLabel.c_str(), (UINT32)entLabel.size(), m_badgeFormat,
                D2D1::RectF(entX, meterY, entX + halfW, meterY + 16.0f), entColor);
        }

        D2D1_ROUNDED_RECT eTrack = D2D1::RoundedRect(D2D1::RectF(entX, meterY + 18.0f, entX + halfW, meterY + 26.0f), 3.0f, 3.0f);
        if (m_cardBgBrush) rt->FillRoundedRectangle(eTrack, m_cardBgBrush);
        float eFillW = halfW * std::min(1.0f, static_cast<float>(m_currentReport->overallEntropy) / 8.0f);
        if (eFillW > 3.0f && entColor) {
            D2D1_ROUNDED_RECT eFill = D2D1::RoundedRect(D2D1::RectF(entX, meterY + 18.0f, entX + eFillW, meterY + 26.0f), 3.0f, 3.0f);
            rt->FillRoundedRectangle(eFill, entColor);
        }
    } else {
        // Empty State Banner
        if (m_headingFormat && m_textWhiteBrush) {
            std::wstring emptyTitle = L"READY FOR ANALYSIS // NO BINARY LOADED";
            rt->DrawText(emptyTitle.c_str(), (UINT32)emptyTitle.size(), m_headingFormat,
                D2D1::RectF(dashX + 16.0f, dashY + 40.0f, dashX + dashW - 16.0f, dashY + 65.0f), m_textWhiteBrush);
        }
        if (m_bodyFormat && m_textMutedBrush) {
            std::wstring emptyBody = L"Drag and drop an executable (.exe, .dll, .sys) into Koltzi or select a profile above to begin offline triage.\n"
                                     L"Computes standard AV hashes (MD5, SHA-1, SHA-256, Imphash) and performs WinVerifyTrust Authenticode verification.";
            rt->DrawText(emptyBody.c_str(), (UINT32)emptyBody.size(), m_bodyFormat,
                D2D1::RectF(dashX + 16.0f, dashY + 75.0f, dashX + dashW - 16.0f, dashY + 140.0f), m_textMutedBrush);
        }
    }

    // --- VIEW TABS: [PE Structure & Findings] vs [Audit & Triage Log] ---
    float tabY = dashY + card1H + 10.0f;
    float tabH = 26.0f;
    float tabW = 200.0f;

    m_tabFindingsRect = D2D1::RectF(dashX, tabY, dashX + tabW, tabY + tabH);
    m_tabAuditRect = D2D1::RectF(dashX + tabW + 8.0f, tabY, dashX + tabW * 2 + 8.0f, tabY + tabH);

    // Tab 1: Findings
    D2D1_ROUNDED_RECT t1R = D2D1::RoundedRect(m_tabFindingsRect, 4.0f, 4.0f);
    ID2D1SolidColorBrush* t1Bg = (!m_showAuditLog) ? m_panelBgBrush : m_cardBgBrush;
    ID2D1SolidColorBrush* t1Border = (!m_showAuditLog) ? m_accentBrush : m_panelBorderBrush;
    ID2D1SolidColorBrush* t1Txt = (!m_showAuditLog) ? m_accentBrush : m_textDimBrush;
    if (t1Bg) rt->FillRoundedRectangle(t1R, t1Bg);
    if (t1Border) rt->DrawRoundedRectangle(t1R, t1Border, 1.0f);
    if (m_badgeFormat && t1Txt) {
        DWRITE_TEXT_ALIGNMENT old = m_badgeFormat->GetTextAlignment();
        m_badgeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawText(L"1. PE FINDINGS & SECTIONS", 25, m_badgeFormat,
            D2D1::RectF(m_tabFindingsRect.left, m_tabFindingsRect.top + 5.0f, m_tabFindingsRect.right, m_tabFindingsRect.bottom), t1Txt);
        m_badgeFormat->SetTextAlignment(old);
    }

    // Tab 2: Audit Log
    size_t logCount = (m_currentReport) ? m_currentReport->logEntries.size() : 0;
    std::wstring t2Title = std::format(L"2. AUDIT LOG ({} EVENTS)", logCount);
    D2D1_ROUNDED_RECT t2R = D2D1::RoundedRect(m_tabAuditRect, 4.0f, 4.0f);
    ID2D1SolidColorBrush* t2Bg = (m_showAuditLog) ? m_panelBgBrush : m_cardBgBrush;
    ID2D1SolidColorBrush* t2Border = (m_showAuditLog) ? m_accentBrush : m_panelBorderBrush;
    ID2D1SolidColorBrush* t2Txt = (m_showAuditLog) ? m_accentBrush : m_textDimBrush;
    if (t2Bg) rt->FillRoundedRectangle(t2R, t2Bg);
    if (t2Border) rt->DrawRoundedRectangle(t2R, t2Border, 1.0f);
    if (m_badgeFormat && t2Txt) {
        DWRITE_TEXT_ALIGNMENT old = m_badgeFormat->GetTextAlignment();
        m_badgeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawText(t2Title.c_str(), (UINT32)t2Title.size(), m_badgeFormat,
            D2D1::RectF(m_tabAuditRect.left, m_tabAuditRect.top + 5.0f, m_tabAuditRect.right, m_tabAuditRect.bottom), t2Txt);
        m_badgeFormat->SetTextAlignment(old);
    }

    float contentY = tabY + tabH + 8.0f;
    float contentH = dashH - (contentY - dashY);

    if (m_showAuditLog) {
        // --- VIEW B: HIGH-DETAIL AUDIT & TRIAGE LOG ---
        D2D1_ROUNDED_RECT auditCardRect = D2D1::RoundedRect(D2D1::RectF(dashX, contentY, dashX + dashW, contentY + contentH), 10.0f, 10.0f);
        if (m_panelBgBrush) rt->FillRoundedRectangle(auditCardRect, m_panelBgBrush);
        if (m_panelBorderBrush) rt->DrawRoundedRectangle(auditCardRect, m_panelBorderBrush, 1.0f);

        std::wstring auditHeader = L"CHRONOLOGICAL STATIC REVERSE ENGINEERING AUDIT LOG";
        if (m_subheadingFormat && m_accentBrush) {
            rt->DrawText(auditHeader.c_str(), (UINT32)auditHeader.size(), m_subheadingFormat,
                D2D1::RectF(dashX + 16.0f, contentY + 10.0f, dashX + dashW - 16.0f, contentY + 28.0f), m_accentBrush);
        }

        IDWriteTextFormat* logFmt = m_fontManager.GetLogFormat();
        if (!logFmt) logFmt = m_codeFormat;

        float entryY = contentY + 34.0f;
        if (m_currentReport && !m_currentReport->logEntries.empty()) {
            size_t maxLogs = static_cast<size_t>(std::max(1, static_cast<int>((contentH - 44.0f) / 19.0f)));
            size_t count = std::min(m_currentReport->logEntries.size(), maxLogs);

            for (size_t i = 0; i < count; ++i) {
                const auto& entry = m_currentReport->logEntries[i];

                if (i % 2 == 1 && m_cardBgBrush) {
                    rt->FillRectangle(D2D1::RectF(dashX + 8.0f, entryY - 2.0f, dashX + dashW - 8.0f, entryY + 17.0f), m_cardBgBrush);
                }

                // Timestamp (Cyan)
                std::wstring ts = Utf8ToWide(entry.timestamp);
                if (logFmt && m_accentBrush) {
                    rt->DrawText(ts.c_str(), (UINT32)ts.size(), logFmt,
                        D2D1::RectF(dashX + 16.0f, entryY, dashX + 90.0f, entryY + 18.0f), m_accentBrush);
                }

                // Subsystem Badge (Muted)
                std::wstring subsys = L"[" + Utf8ToWide(entry.subsystem) + L"]";
                if (logFmt && m_textMutedBrush) {
                    rt->DrawText(subsys.c_str(), (UINT32)subsys.size(), logFmt,
                        D2D1::RectF(dashX + 92.0f, entryY, dashX + 165.0f, entryY + 18.0f), m_textMutedBrush);
                }

                // Level Color
                ID2D1SolidColorBrush* lvlBrush = m_textMutedBrush;
                if (entry.level == "CRIT") lvlBrush = m_redBrush;
                else if (entry.level == "WARN") lvlBrush = m_yellowBrush;
                else if (entry.level == "PASS") lvlBrush = m_greenBrush;
                else if (entry.level == "AUDIT") lvlBrush = m_textWhiteBrush;

                std::wstring lvl = Utf8ToWide(entry.level);
                if (logFmt && lvlBrush) {
                    rt->DrawText(lvl.c_str(), (UINT32)lvl.size(), logFmt,
                        D2D1::RectF(dashX + 170.0f, entryY, dashX + 220.0f, entryY + 18.0f), lvlBrush);
                }

                // Message Text
                std::wstring msg = Utf8ToWide(entry.message);
                if (logFmt && m_textWhiteBrush) {
                    rt->DrawText(msg.c_str(), (UINT32)msg.size(), logFmt,
                        D2D1::RectF(dashX + 225.0f, entryY, dashX + dashW - 16.0f, entryY + 18.0f), m_textWhiteBrush);
                }

                entryY += 19.0f;
            }
        } else {
            if (m_codeFormat && m_textDimBrush) {
                std::wstring noLogs = L"No triage audit logs recorded. Load a binary or run a sample test to view the log stream.";
                rt->DrawText(noLogs.c_str(), (UINT32)noLogs.size(), m_codeFormat,
                    D2D1::RectF(dashX + 16.0f, entryY, dashX + dashW - 16.0f, entryY + 20.0f), m_textDimBrush);
            }
        }
    } else {
        // --- VIEW A: PE SECTION TABLE & HEURISTIC FINDINGS ---
        float card2H = std::min(185.0f, contentH * 0.48f);
        D2D1_ROUNDED_RECT card2Rect = D2D1::RoundedRect(D2D1::RectF(dashX, contentY, dashX + dashW, contentY + card2H), 10.0f, 10.0f);
        if (m_panelBgBrush) rt->FillRoundedRectangle(card2Rect, m_panelBgBrush);
        if (m_panelBorderBrush) rt->DrawRoundedRectangle(card2Rect, m_panelBorderBrush, 1.0f);

        std::wstring card2Title = L"PE SECTION TABLE & SHANNON ENTROPY (256-BIN LOOKUP)";
        if (m_subheadingFormat && m_accentBrush) {
            rt->DrawText(card2Title.c_str(), (UINT32)card2Title.size(), m_subheadingFormat,
                D2D1::RectF(dashX + 16.0f, contentY + 10.0f, dashX + dashW - 16.0f, contentY + 28.0f), m_accentBrush);
        }

        // Table Column Headers
        float tableY = contentY + 32.0f;
        float colNameW = 100.0f;
        float colVSizeW = 110.0f;
        float colRSizeW = 110.0f;
        float colEntW = 180.0f;

        float col1 = dashX + 16.0f;
        float col2 = col1 + colNameW;
        float col3 = col2 + colVSizeW;
        float col4 = col3 + colRSizeW;
        float col5 = col4 + colEntW;

        if (m_codeFormat && m_textDimBrush) {
            rt->DrawText(L"SECTION", 7, m_codeFormat, D2D1::RectF(col1, tableY, col2, tableY + 18.0f), m_textDimBrush);
            rt->DrawText(L"VIRT_SIZE", 9, m_codeFormat, D2D1::RectF(col2, tableY, col3, tableY + 18.0f), m_textDimBrush);
            rt->DrawText(L"RAW_SIZE", 8, m_codeFormat, D2D1::RectF(col3, tableY, col4, tableY + 18.0f), m_textDimBrush);
            rt->DrawText(L"ENTROPY (0 - 8.0)", 17, m_codeFormat, D2D1::RectF(col4, tableY, col5, tableY + 18.0f), m_textDimBrush);
            rt->DrawText(L"VERDICT / FLAGS", 15, m_codeFormat, D2D1::RectF(col5, tableY, dashX + dashW - 16.0f, tableY + 18.0f), m_textDimBrush);
        }

        if (m_panelBorderBrush) {
            rt->DrawLine(D2D1::Point2F(dashX + 16.0f, tableY + 18.0f), D2D1::Point2F(dashX + dashW - 16.0f, tableY + 18.0f), m_panelBorderBrush, 1.0f);
        }

        float rowY = tableY + 22.0f;
        if (m_currentReport && !m_currentReport->sections.empty()) {
            size_t maxRows = std::min<size_t>(m_currentReport->sections.size(), 5);
            for (size_t i = 0; i < maxRows; ++i) {
                const auto& sec = m_currentReport->sections[i];
                std::wstring sName = Utf8ToWide(sec.name);
                std::wstring sVSize = std::format(L"0x{:X}", sec.virtualSize);
                std::wstring sRSize = std::format(L"{} B", sec.rawSize);
                std::wstring sEnt = std::format(L"{:.2f}", sec.entropy);

                if (i % 2 == 1 && m_cardBgBrush) {
                    rt->FillRectangle(D2D1::RectF(col1 - 4.0f, rowY - 2.0f, dashX + dashW - 16.0f, rowY + 18.0f), m_cardBgBrush);
                }

                if (m_codeFormat && m_textWhiteBrush) {
                    rt->DrawText(sName.c_str(), (UINT32)sName.size(), m_codeFormat, D2D1::RectF(col1, rowY, col2, rowY + 18.0f), m_textWhiteBrush);
                }
                if (m_codeFormat && m_textMutedBrush) {
                    rt->DrawText(sVSize.c_str(), (UINT32)sVSize.size(), m_codeFormat, D2D1::RectF(col2, rowY, col3, rowY + 18.0f), m_textMutedBrush);
                    rt->DrawText(sRSize.c_str(), (UINT32)sRSize.size(), m_codeFormat, D2D1::RectF(col3, rowY, col4, rowY + 18.0f), m_textMutedBrush);
                }

                // Entropy Mini Progress Bar
                float barTotalW = 100.0f;
                float barFilled = barTotalW * std::min(1.0f, static_cast<float>(sec.entropy) / 8.0f);
                D2D1_ROUNDED_RECT miniTrack = D2D1::RoundedRect(D2D1::RectF(col4, rowY + 4.0f, col4 + barTotalW, rowY + 12.0f), 2.0f, 2.0f);
                if (m_cardBgBrush) rt->FillRoundedRectangle(miniTrack, m_cardBgBrush);

                ID2D1SolidColorBrush* secColor = (sec.entropy > 7.2f) ? m_yellowBrush : m_accentBrush;
                if (secColor && barFilled > 2.0f) {
                    D2D1_ROUNDED_RECT miniFill = D2D1::RoundedRect(D2D1::RectF(col4, rowY + 4.0f, col4 + barFilled, rowY + 12.0f), 2.0f, 2.0f);
                    rt->FillRoundedRectangle(miniFill, secColor);
                }
                if (m_codeFormat && secColor) {
                    rt->DrawText(sEnt.c_str(), (UINT32)sEnt.size(), m_codeFormat, D2D1::RectF(col4 + barTotalW + 10.0f, rowY, col5, rowY + 18.0f), secColor);
                }

                // Flags / Verdict
                std::wstring flagStr;
                ID2D1SolidColorBrush* flagColor = m_textMutedBrush;
                if (sec.isRwx) {
                    flagStr = L"[RWX VIOLATION]";
                    flagColor = m_redBrush;
                } else if (sec.isSuspiciousEntropy) {
                    flagStr = L"[HIGH ENTROPY / PACKED]";
                    flagColor = m_yellowBrush;
                } else {
                    flagStr = L"OK";
                    flagColor = m_greenBrush;
                }

                if (m_codeFormat && flagColor) {
                    rt->DrawText(flagStr.c_str(), (UINT32)flagStr.size(), m_codeFormat, D2D1::RectF(col5, rowY, dashX + dashW - 16.0f, rowY + 18.0f), flagColor);
                }

                rowY += 21.0f;
            }
        }

        // --- CARD 3: Reverse Engineering Findings & Correlated Threat Analysis ---
        float card3Y = contentY + card2H + 10.0f;
        float card3H = contentH - (card3Y - contentY);
        if (card3H < 80.0f) card3H = 80.0f;

        D2D1_ROUNDED_RECT card3Rect = D2D1::RoundedRect(D2D1::RectF(dashX, card3Y, dashX + dashW, card3Y + card3H), 10.0f, 10.0f);
        if (m_panelBgBrush) rt->FillRoundedRectangle(card3Rect, m_panelBgBrush);
        if (m_panelBorderBrush) rt->DrawRoundedRectangle(card3Rect, m_panelBorderBrush, 1.0f);

        std::wstring card3Title = L"LOW-LEVEL HEURISTICS & REVERSE ENGINEERING FINDINGS";
        if (m_subheadingFormat && m_accentBrush) {
            rt->DrawText(card3Title.c_str(), (UINT32)card3Title.size(), m_subheadingFormat,
                D2D1::RectF(dashX + 16.0f, card3Y + 10.0f, dashX + dashW - 16.0f, card3Y + 28.0f), m_accentBrush);
        }

        float findY = card3Y + 32.0f;
        if (m_currentReport && !m_currentReport->technicalDetails.empty()) {
            size_t maxFindings = static_cast<size_t>(std::max(1, static_cast<int>((card3H - 40.0f) / 19.0f)));
            size_t count = std::min(m_currentReport->technicalDetails.size(), maxFindings);

            for (size_t i = 0; i < count; ++i) {
                const auto& detail = m_currentReport->technicalDetails[i];
                std::wstring wDetail = Utf8ToWide(detail);

                ID2D1SolidColorBrush* lineBrush = m_textMutedBrush;
                if (detail.starts_with("[CRITICAL]")) {
                    lineBrush = m_redBrush;
                } else if (detail.starts_with("[WARNING]")) {
                    lineBrush = m_yellowBrush;
                } else if (detail.starts_with("[INFO]")) {
                    lineBrush = m_greenBrush;
                }

                if (m_codeFormat && lineBrush) {
                    rt->DrawText(wDetail.c_str(), (UINT32)wDetail.size(), m_codeFormat,
                        D2D1::RectF(dashX + 16.0f, findY, dashX + dashW - 16.0f, findY + 18.0f), lineBrush);
                }
                findY += 19.0f;
            }
        } else {
            if (m_codeFormat && m_textDimBrush) {
                std::wstring noFindings = L"No heuristic red flags or warnings recorded.";
                rt->DrawText(noFindings.c_str(), (UINT32)noFindings.size(), m_codeFormat,
                    D2D1::RectF(dashX + 16.0f, findY, dashX + dashW - 16.0f, findY + 20.0f), m_textDimBrush);
            }
        }
    }
}

void GhostWindow::RenderFooter(ID2D1RenderTarget* rt) {
    float footerH = 26.0f;
    float footerY = static_cast<float>(m_height) - footerH;
    D2D1_RECT_F footerRect = D2D1::RectF(0.0f, footerY, static_cast<float>(m_width), static_cast<float>(m_height));

    if (m_bgBrush) rt->FillRectangle(footerRect, m_bgBrush);
    if (m_panelBorderBrush) {
        rt->DrawLine(D2D1::Point2F(0.0f, footerY), D2D1::Point2F(static_cast<float>(m_width), footerY), m_panelBorderBrush, 1.0f);
    }

    if (m_subtitleFormat && m_textDimBrush) {
        std::wstring leftInfo = L"Koltzi Engine v1.0 | Offline Static PE Triage | Creato Display Typography | /MT";
        rt->DrawText(leftInfo.c_str(), (UINT32)leftInfo.size(), m_subtitleFormat,
            D2D1::RectF(16.0f, footerY + 4.0f, 600.0f, static_cast<float>(m_height)), m_textDimBrush);

        std::wstring rightInfo = std::format(L"Direct2D Hardware Accelerated | {} x {}", m_width, m_height);
        DWRITE_TEXT_ALIGNMENT old = m_subtitleFormat->GetTextAlignment();
        m_subtitleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawText(rightInfo.c_str(), (UINT32)rightInfo.size(), m_subtitleFormat,
            D2D1::RectF(static_cast<float>(m_width) - 400.0f, footerY + 4.0f, static_cast<float>(m_width) - 16.0f, static_cast<float>(m_height)), m_textDimBrush);
        m_subtitleFormat->SetTextAlignment(old);
    }
}

void GhostWindow::ShowContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_SCAN_FILE, L"Analyze PE Binary File...");
    AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE_AUDIT, m_showAuditLog ? L"Switch to PE Findings View" : L"Switch to Audit & Triage Log View");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_CLEAN, L"Test Profile: Clean PE (Standard Imports)");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_PACKED, L"Test Profile: High Entropy / Packed Code");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_SYSCALL_PEB, L"Test Profile: Direct Syscalls + PEB Hashing");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_CRED_STEALER, L"Test Profile: Credential Scraping Artifacts");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_INJECTION, L"Test Profile: Process Injection Chain");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_RESET, L"Reset Mascot to Idle");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit Koltzi");

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == IDM_TOGGLE_AUDIT) {
        m_showAuditLog = !m_showAuditLog;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    } else if (cmd != 0 && m_onCommand) {
        m_onCommand(cmd);
    }
}

LRESULT CALLBACK GhostWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GhostWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<GhostWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<GhostWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT GhostWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        if (wParam == TIMER_ANIMATION_ID) {
            Update(0.016f);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE: {
        UINT w = LOWORD(lParam);
        UINT h = HIWORD(lParam);
        if (w > 0 && h > 0) {
            m_width = w;
            m_height = h;
            if (m_renderTarget) {
                m_renderTarget->Resize(D2D1::SizeU(m_width, m_height));
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 880;
        mmi->ptMinTrackSize.y = 580;
        return 0;
    }

    case WM_MOUSEMOVE: {
        float x = static_cast<float>(LOWORD(lParam));
        float y = static_cast<float>(HIWORD(lParam));

        int prevHover = m_hoveredButton;
        m_hoveredButton = -1;
        for (const auto& b : m_buttons) {
            if (x >= b.rect.left && x <= b.rect.right && y >= b.rect.top && y <= b.rect.bottom) {
                m_hoveredButton = b.id;
                break;
            }
        }

        bool prevDropHover = m_dropHover;
        m_dropHover = (x >= m_dropZoneRect.left && x <= m_dropZoneRect.right &&
                       y >= m_dropZoneRect.top && y <= m_dropZoneRect.bottom);

        bool isTabHover = (x >= m_tabFindingsRect.left && x <= m_tabFindingsRect.right &&
                           y >= m_tabFindingsRect.top && y <= m_tabFindingsRect.bottom) ||
                          (x >= m_tabAuditRect.left && x <= m_tabAuditRect.right &&
                           y >= m_tabAuditRect.top && y <= m_tabAuditRect.bottom);

        if (m_hoveredButton != -1 || m_dropHover || isTabHover) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
        } else {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }

        if (prevHover != m_hoveredButton || prevDropHover != m_dropHover) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float x = static_cast<float>(LOWORD(lParam));
        float y = static_cast<float>(HIWORD(lParam));

        // Check Toolbar Button clicks
        for (const auto& b : m_buttons) {
            if (x >= b.rect.left && x <= b.rect.right && y >= b.rect.top && y <= b.rect.bottom) {
                if (b.id == IDM_TOGGLE_AUDIT) {
                    m_showAuditLog = !m_showAuditLog;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (m_onCommand) m_onCommand(b.id);
                return 0;
            }
        }

        // Check Tab Clicks
        if (x >= m_tabFindingsRect.left && x <= m_tabFindingsRect.right &&
            y >= m_tabFindingsRect.top && y <= m_tabFindingsRect.bottom) {
            m_showAuditLog = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (x >= m_tabAuditRect.left && x <= m_tabAuditRect.right &&
            y >= m_tabAuditRect.top && y <= m_tabAuditRect.bottom) {
            m_showAuditLog = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Check Drop Zone click
        if (x >= m_dropZoneRect.left && x <= m_dropZoneRect.right &&
            y >= m_dropZoneRect.top && y <= m_dropZoneRect.bottom) {
            if (m_onCommand) m_onCommand(IDM_SCAN_FILE);
            return 0;
        }

        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ClientToScreen(hwnd, &pt);
        ShowContextMenu(pt.x, pt.y);
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        wchar_t szFile[MAX_PATH] = { 0 };
        if (DragQueryFileW(hDrop, 0, szFile, MAX_PATH)) {
            if (m_onFileDrop) {
                m_onFileDrop(szFile);
            }
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace Koltzi
