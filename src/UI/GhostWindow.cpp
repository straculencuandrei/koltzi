#include "GhostWindow.h"
#include "../../res/resource.h"
#include <algorithm>
#include <format>
#include <cmath>

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
    IDM_EXIT = 2009
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
    wc.lpszClassName = L"KoltziLiquidMorphismWindowClass";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int posX = (workArea.right - workArea.left - m_width) / 2;
    int posY = (workArea.bottom - workArea.top - m_height) / 2;
    if (posX < 20) posX = 20;
    if (posY < 20) posY = 20;

    m_hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        wc.lpszClassName,
        L"Koltzi - Institutional Malware Triage Workbench",
        WS_OVERLAPPEDWINDOW,
        posX, posY, m_width, m_height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return false;

    DragAcceptFiles(m_hwnd, TRUE);

    if (!CreateDeviceIndependentResources()) return false;
    if (!CreateDeviceResources()) return false;

    QueryPerformanceFrequency(&m_perfFreq);
    QueryPerformanceCounter(&m_lastPerfCounter);

    // 60 FPS animation timer (~16 ms)
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
    SafeRelease(m_dashedStroke);
    SafeRelease(m_dwriteFactory);
    SafeRelease(m_d2dFactory);
}

bool GhostWindow::CreateDeviceIndependentResources() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_dwriteFactory));
    if (FAILED(hr)) return false;

    m_fontManager.Initialize(m_dwriteFactory);

    // Create dashed stroke style for drop zone
    D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_CAP_STYLE_ROUND,
        D2D1_LINE_JOIN_ROUND,
        10.0f,
        D2D1_DASH_STYLE_DASH,
        0.0f
    );
    m_d2dFactory->CreateStrokeStyle(&strokeProps, nullptr, 0, &m_dashedStroke);

    return true;
}

bool GhostWindow::CreateDeviceResources() {
    if (!m_d2dFactory || !m_dwriteFactory || !m_hwnd) return false;

    if (!m_renderTarget) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        m_width = rc.right - rc.left;
        m_height = rc.bottom - rc.top;
        if (m_width < 100) m_width = 1280;
        if (m_height < 100) m_height = 820;

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

        // ---------------------------------------------------------------------
        // Liquid Morphism Color Palette & Materials (Ember & Slate)
        // ---------------------------------------------------------------------

        // Base background vertical gradient: #100d0a (top) to #1c1510 (bottom)
        D2D1_GRADIENT_STOP baseStops[2];
        baseStops[0].position = 0.0f;
        baseStops[0].color = D2D1::ColorF(0.063f, 0.051f, 0.039f, 1.0f); // #100d0a
        baseStops[1].position = 1.0f;
        baseStops[1].color = D2D1::ColorF(0.110f, 0.082f, 0.063f, 1.0f); // #1c1510
        ID2D1GradientStopCollection* baseStopColl = nullptr;
        m_renderTarget->CreateGradientStopCollection(baseStops, 2, &baseStopColl);
        if (baseStopColl) {
            m_renderTarget->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.0f), D2D1::Point2F(0.0f, static_cast<float>(m_height))),
                baseStopColl, &m_bgBaseGradient);
            SafeRelease(baseStopColl);
        }

        // Ambient Background Blobs
        // Blob 1: Warm Amber (#f59e0b)
        D2D1_GRADIENT_STOP blob1Stops[2];
        blob1Stops[0].position = 0.0f;
        blob1Stops[0].color = D2D1::ColorF(0.961f, 0.620f, 0.043f, 0.32f);
        blob1Stops[1].position = 1.0f;
        blob1Stops[1].color = D2D1::ColorF(0.961f, 0.620f, 0.043f, 0.0f);
        ID2D1GradientStopCollection* b1Coll = nullptr;
        m_renderTarget->CreateGradientStopCollection(blob1Stops, 2, &b1Coll);
        if (b1Coll) {
            m_renderTarget->CreateRadialGradientBrush(
                D2D1::RadialGradientBrushProperties(D2D1::Point2F(250.0f, 250.0f), D2D1::Point2F(0, 0), 320.0f, 320.0f),
                b1Coll, &m_blobBrush1);
            SafeRelease(b1Coll);
        }

        // Blob 2: Burnt Sienna (#c2410c)
        D2D1_GRADIENT_STOP blob2Stops[2];
        blob2Stops[0].position = 0.0f;
        blob2Stops[0].color = D2D1::ColorF(0.761f, 0.255f, 0.047f, 0.28f);
        blob2Stops[1].position = 1.0f;
        blob2Stops[1].color = D2D1::ColorF(0.761f, 0.255f, 0.047f, 0.0f);
        ID2D1GradientStopCollection* b2Coll = nullptr;
        m_renderTarget->CreateGradientStopCollection(blob2Stops, 2, &b2Coll);
        if (b2Coll) {
            m_renderTarget->CreateRadialGradientBrush(
                D2D1::RadialGradientBrushProperties(D2D1::Point2F(850.0f, 500.0f), D2D1::Point2F(0, 0), 280.0f, 280.0f),
                b2Coll, &m_blobBrush2);
            SafeRelease(b2Coll);
        }

        // Blob 3: Deep Amber-Brown (#78350f)
        D2D1_GRADIENT_STOP blob3Stops[2];
        blob3Stops[0].position = 0.0f;
        blob3Stops[0].color = D2D1::ColorF(0.471f, 0.208f, 0.059f, 0.24f);
        blob3Stops[1].position = 1.0f;
        blob3Stops[1].color = D2D1::ColorF(0.471f, 0.208f, 0.059f, 0.0f);
        ID2D1GradientStopCollection* b3Coll = nullptr;
        m_renderTarget->CreateGradientStopCollection(blob3Stops, 2, &b3Coll);
        if (b3Coll) {
            m_renderTarget->CreateRadialGradientBrush(
                D2D1::RadialGradientBrushProperties(D2D1::Point2F(550.0f, 350.0f), D2D1::Point2F(0, 0), 240.0f, 240.0f),
                b3Coll, &m_blobBrush3);
            SafeRelease(b3Coll);
        }

        // Liquid Glass Panels
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f), &m_glassFillBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f), &m_glassFillHoverBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.20f), &m_glassFillActiveBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), &m_glassBorderBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.24f), &m_glassBorderHoverBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.20f), &m_glassSpecularBrush);

        // Accent Brushes
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.961f, 0.620f, 0.043f, 1.0f), &m_accentPrimaryBrush);      // #f59e0b
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.984f, 0.749f, 0.141f, 1.0f), &m_accentPrimaryHoverBrush); // #fbbf24
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.761f, 0.255f, 0.047f, 1.0f), &m_accentSecondaryBrush);    // #c2410c
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.220f, 0.741f, 0.973f, 1.0f), &m_cyanAccentBrush);         // #38bdf8

        // Semantic Status Brushes
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.063f, 0.725f, 0.506f, 1.0f), &m_passBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.063f, 0.725f, 0.506f, 0.14f), &m_passBgBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.961f, 0.620f, 0.043f, 1.0f), &m_warnBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.961f, 0.620f, 0.043f, 0.14f), &m_warnBgBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.937f, 0.267f, 0.267f, 1.0f), &m_threatBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.937f, 0.267f, 0.267f, 0.14f), &m_threatBgBrush);

        // Typography Warm Off-White Brushes
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.973f, 0.941f, 0.95f), &m_textPrimaryBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.922f, 0.824f, 0.60f), &m_textSecondaryBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.863f, 0.706f, 0.35f), &m_textMutedBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.863f, 0.706f, 0.20f), &m_textDimBrush);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &m_textWhiteBrush);
    }

    return true;
}

void GhostWindow::DiscardDeviceResources() {
    SafeRelease(m_bgBaseGradient);
    SafeRelease(m_blobBrush1);
    SafeRelease(m_blobBrush2);
    SafeRelease(m_blobBrush3);

    SafeRelease(m_glassFillBrush);
    SafeRelease(m_glassFillHoverBrush);
    SafeRelease(m_glassFillActiveBrush);
    SafeRelease(m_glassBorderBrush);
    SafeRelease(m_glassBorderHoverBrush);
    SafeRelease(m_glassSpecularBrush);

    SafeRelease(m_accentPrimaryBrush);
    SafeRelease(m_accentPrimaryHoverBrush);
    SafeRelease(m_accentSecondaryBrush);
    SafeRelease(m_cyanAccentBrush);

    SafeRelease(m_passBrush);
    SafeRelease(m_passBgBrush);
    SafeRelease(m_warnBrush);
    SafeRelease(m_warnBgBrush);
    SafeRelease(m_threatBrush);
    SafeRelease(m_threatBgBrush);

    SafeRelease(m_textPrimaryBrush);
    SafeRelease(m_textSecondaryBrush);
    SafeRelease(m_textMutedBrush);
    SafeRelease(m_textDimBrush);
    SafeRelease(m_textWhiteBrush);

    SafeRelease(m_renderTarget);
}

void GhostWindow::SetReport(std::shared_ptr<TriageReport> report) {
    m_currentReport = report;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void GhostWindow::SetMood(GhostMood mood) {
    m_currentMood = mood;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void GhostWindow::SetDialogue(const std::string& text, bool immediate) {
    (void)text;
    (void)immediate;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void GhostWindow::ToggleHUD() {
    m_activeTab = (m_activeTab + 1) % 3;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void GhostWindow::Update(float dt) {
    m_uiTime += dt;

    float targetScore = m_currentReport ? static_cast<float>(m_currentReport->threatScore) : 0.0f;
    m_animatedThreatScore += (targetScore - m_animatedThreatScore) * std::min(1.0f, dt * 9.0f);

    float targetEnt = m_currentReport ? static_cast<float>(m_currentReport->overallEntropy) : 0.0f;
    m_animatedEntropy += (targetEnt - m_animatedEntropy) * std::min(1.0f, dt * 9.0f);

    for (auto& b : m_buttons) {
        float target = (m_hoveredButton == b.id) ? 1.0f : 0.0f;
        b.hoverAlpha += (target - b.hoverAlpha) * std::min(1.0f, dt * 14.0f);
    }

    for (int i = 0; i < 3; ++i) {
        float target = (m_hoveredButton == -(10 + i)) ? 1.0f : 0.0f;
        m_tabHoverAlphas[i] += (target - m_tabHoverAlphas[i]) * std::min(1.0f, dt * 14.0f);
    }

    float targetDrop = m_dropHover ? 1.0f : 0.0f;
    m_dropHoverAlpha += (targetDrop - m_dropHoverAlpha) * std::min(1.0f, dt * 12.0f);
}

void GhostWindow::DrawGlassPanel(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float radius,
                                 bool isHovered, float hoverFactor) {
    D2D1_ROUNDED_RECT rRect = D2D1::RoundedRect(rect, radius, radius);

    // Glass Fill (white neutral)
    ID2D1SolidColorBrush* fill = (hoverFactor > 0.05f || isHovered) ? m_glassFillHoverBrush : m_glassFillBrush;
    if (fill) rt->FillRoundedRectangle(rRect, fill);

    // Glass Border (1px solid)
    ID2D1SolidColorBrush* border = (isHovered || hoverFactor > 0.2f) ? m_glassBorderHoverBrush : m_glassBorderBrush;
    if (border) rt->DrawRoundedRectangle(rRect, border, 1.0f);

    // Apple Liquid Glass Specular Highlight (1px line along top inner rim)
    if (m_glassSpecularBrush && (rect.right - rect.left > radius * 2.0f)) {
        float specularStart = rect.left + radius * 0.7f;
        float specularEnd = rect.right - radius * 0.7f;
        float specularY = rect.top + 1.0f;
        rt->DrawLine(D2D1::Point2F(specularStart, specularY), D2D1::Point2F(specularEnd, specularY), m_glassSpecularBrush, 1.0f);
    }
}

void GhostWindow::DrawGlassPill(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect,
                               bool isHovered, float hoverFactor) {
    float pillRadius = (rect.bottom - rect.top) * 0.5f;
    DrawGlassPanel(rt, rect, pillRadius, isHovered, hoverFactor);
}

void GhostWindow::DrawSolidPill(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect,
                               ID2D1Brush* fillBrush, ID2D1Brush* borderBrush) {
    float pillRadius = (rect.bottom - rect.top) * 0.5f;
    D2D1_ROUNDED_RECT rRect = D2D1::RoundedRect(rect, pillRadius, pillRadius);
    if (fillBrush) rt->FillRoundedRectangle(rRect, fillBrush);
    if (borderBrush) rt->DrawRoundedRectangle(rRect, borderBrush, 1.0f);
}

void GhostWindow::DrawVectorUploadIcon(ID2D1RenderTarget* rt, float cx, float cy, float size, ID2D1Brush* brush) {
    if (!brush) return;
    float half = size * 0.5f;
    // Up arrow shaft
    rt->DrawLine(D2D1::Point2F(cx, cy - half), D2D1::Point2F(cx, cy + half * 0.35f), brush, 1.5f);
    // Up arrow chevron
    rt->DrawLine(D2D1::Point2F(cx - half * 0.55f, cy - half * 0.45f), D2D1::Point2F(cx, cy - half), brush, 1.5f);
    rt->DrawLine(D2D1::Point2F(cx + half * 0.55f, cy - half * 0.45f), D2D1::Point2F(cx, cy - half), brush, 1.5f);
    // Open tray
    float trayY = cy + half * 0.65f;
    rt->DrawLine(D2D1::Point2F(cx - half, trayY), D2D1::Point2F(cx + half, trayY), brush, 1.5f);
}

void GhostWindow::RenderBackgroundBlobs(ID2D1RenderTarget* rt) {
    float w = static_cast<float>(m_width);
    float h = static_cast<float>(m_height);

    // 1. Dark base gradient: #100d0a to #1c1510
    if (m_bgBaseGradient) {
        m_bgBaseGradient->SetStartPoint(D2D1::Point2F(0.0f, 0.0f));
        m_bgBaseGradient->SetEndPoint(D2D1::Point2F(0.0f, h));
        rt->FillRectangle(D2D1::RectF(0.0f, 0.0f, w, h), m_bgBaseGradient);
    } else {
        rt->Clear(D2D1::ColorF(0.063f, 0.051f, 0.039f, 1.0f));
    }

    // 2. Animated Ambient Blobs floating softly under the glass panels
    // Blob 1: Amber (#f59e0b) drifting near top-left / center
    if (m_blobBrush1) {
        float b1X = w * 0.22f + sinf(m_uiTime * 0.35f) * 110.0f;
        float b1Y = h * 0.28f + cosf(m_uiTime * 0.28f) * 80.0f;
        float b1Rx = 360.0f + sinf(m_uiTime * 0.20f) * 40.0f;
        float b1Ry = 330.0f + cosf(m_uiTime * 0.20f) * 30.0f;
        m_blobBrush1->SetCenter(D2D1::Point2F(b1X, b1Y));
        m_blobBrush1->SetRadiusX(b1Rx);
        m_blobBrush1->SetRadiusY(b1Ry);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(b1X, b1Y), b1Rx, b1Ry), m_blobBrush1);
    }

    // Blob 2: Burnt Sienna (#c2410c) drifting near bottom-right
    if (m_blobBrush2) {
        float b2X = w * 0.78f + cosf(m_uiTime * 0.32f) * 100.0f;
        float b2Y = h * 0.68f + sinf(m_uiTime * 0.40f) * 75.0f;
        float b2Rx = 290.0f + cosf(m_uiTime * 0.25f) * 30.0f;
        float b2Ry = 270.0f + sinf(m_uiTime * 0.25f) * 25.0f;
        m_blobBrush2->SetCenter(D2D1::Point2F(b2X, b2Y));
        m_blobBrush2->SetRadiusX(b2Rx);
        m_blobBrush2->SetRadiusY(b2Ry);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(b2X, b2Y), b2Rx, b2Ry), m_blobBrush2);
    }

    // Blob 3: Deep Amber-Brown (#78350f) drifting near bottom-left
    if (m_blobBrush3) {
        float b3X = w * 0.50f + sinf(m_uiTime * 0.25f + 2.0f) * 120.0f;
        float b3Y = h * 0.48f + cosf(m_uiTime * 0.30f + 1.2f) * 70.0f;
        float b3Rx = 260.0f;
        float b3Ry = 240.0f;
        m_blobBrush3->SetCenter(D2D1::Point2F(b3X, b3Y));
        m_blobBrush3->SetRadiusX(b3Rx);
        m_blobBrush3->SetRadiusY(b3Ry);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(b3X, b3Y), b3Rx, b3Ry), m_blobBrush3);
    }
}

void GhostWindow::Render() {
    if (!CreateDeviceResources() || !m_renderTarget) return;

    m_renderTarget->BeginDraw();

    // 1. Ambient Background Layer with animated liquid blobs
    RenderBackgroundBlobs(m_renderTarget);

    // 2. Liquid Glass Top Navigation Bar (Height: 52px)
    RenderTopHeader(m_renderTarget);

    // 3. Two-Column Floating Liquid Glass Layout
    float topY = 52.0f;
    float footerH = 30.0f;
    float contentY = topY + 12.0f;
    float contentH = static_cast<float>(m_height) - contentY - footerH - 12.0f;

    float sidebarX = 16.0f;
    float sidebarW = 340.0f;

    RenderExecutiveInspectionPanel(m_renderTarget, sidebarX, contentY, sidebarW, contentH);

    float workbenchX = sidebarX + sidebarW + 16.0f;
    float workbenchW = static_cast<float>(m_width) - workbenchX - 16.0f;

    RenderWorkbench(m_renderTarget, workbenchX, contentY, workbenchW, contentH);

    // 4. Liquid Glass Footer Status Rail (Height: 30px)
    RenderFooter(m_renderTarget);

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

void GhostWindow::RenderTopHeader(ID2D1RenderTarget* rt) {
    float headerH = 52.0f;
    D2D1_RECT_F headerRect = D2D1::RectF(0.0f, 0.0f, static_cast<float>(m_width), headerH);

    // Glass Navbar Surface
    if (m_glassFillBrush) rt->FillRectangle(headerRect, m_glassFillBrush);
    if (m_glassBorderBrush) {
        rt->DrawLine(D2D1::Point2F(0.0f, headerH), D2D1::Point2F(static_cast<float>(m_width), headerH), m_glassBorderBrush, 1.0f);
    }
    if (m_glassSpecularBrush) {
        rt->DrawLine(D2D1::Point2F(0.0f, 1.0f), D2D1::Point2F(static_cast<float>(m_width), 1.0f), m_glassSpecularBrush, 1.0f);
    }

    // --- Left: Brand + Target Info ---
    float curX = 20.0f;

    // Brand: Koltzi
    IDWriteTextFormat* uiHeaderFmt = m_fontManager.GetUiHeaderFormat();
    if (uiHeaderFmt && m_textPrimaryBrush) {
        std::wstring title = L"Koltzi";
        rt->DrawText(title.c_str(), (UINT32)title.size(), uiHeaderFmt,
            D2D1::RectF(curX, 14.0f, curX + 70.0f, 40.0f), m_textPrimaryBrush);
        curX += 76.0f;
    }

    // Version Glass Pill
    IDWriteTextFormat* uiBadgeFmt = m_fontManager.GetUiBadgeFormat();
    if (uiBadgeFmt && m_textMutedBrush) {
        D2D1_RECT_F vRect = D2D1::RectF(curX, 16.0f, curX + 44.0f, 36.0f);
        DrawGlassPill(rt, vRect);

        DWRITE_TEXT_ALIGNMENT old = uiBadgeFmt->GetTextAlignment();
        uiBadgeFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawText(L"v2.0", 4, uiBadgeFmt, D2D1::RectF(vRect.left, vRect.top + 2.0f, vRect.right, vRect.bottom), m_textMutedBrush);
        uiBadgeFmt->SetTextAlignment(old);
        curX += 54.0f;
    }

    // Breadcrumb Separator
    if (m_fontManager.GetUiLabelFormat() && m_textDimBrush) {
        rt->DrawText(L"/", 1, m_fontManager.GetUiLabelFormat(), D2D1::RectF(curX, 17.0f, curX + 10.0f, 35.0f), m_textDimBrush);
        curX += 14.0f;
    }

    // Target Label & Name
    std::wstring targetName = m_currentReport ? Utf8ToWide(m_currentReport->fileName) : L"No target binary loaded";
    if (targetName.size() > 28) targetName = targetName.substr(0, 26) + L"..";
    if (m_fontManager.GetMonoCodeFormat() && m_textSecondaryBrush) {
        rt->DrawText(targetName.c_str(), (UINT32)targetName.size(), m_fontManager.GetMonoCodeFormat(),
            D2D1::RectF(curX, 17.0f, curX + 240.0f, 37.0f), m_textSecondaryBrush);
        curX += std::min(250.0f, static_cast<float>(targetName.size() * 8.5f + 16.0f));
    }

    // Neutral Tag Pills
    if (m_currentReport && curX < 560.0f) {
        std::wstring pill1 = m_currentReport->is64Bit ? L"x64 PE" : L"x86 PE";
        std::wstring pill2 = m_currentReport->isInstaller ? Utf8ToWide(m_currentReport->installerType) : Utf8ToWide(m_currentReport->subsystem);
        if (pill2.size() > 14) pill2 = pill2.substr(0, 12) + L"..";
        std::wstring pill3 = Utf8ToWide(FormatFileSize(m_currentReport->fileSize));

        std::vector<std::pair<std::wstring, ID2D1SolidColorBrush*>> pills = {
            { pill1, m_textMutedBrush },
            { pill2, m_currentReport->isInstaller ? m_cyanAccentBrush : m_textMutedBrush },
            { pill3, m_textDimBrush }
        };

        for (const auto& p : pills) {
            float pW = static_cast<float>(p.first.size() * 7.5f + 18.0f);
            D2D1_RECT_F pRect = D2D1::RectF(curX, 16.0f, curX + pW, 36.0f);
            DrawGlassPill(rt, pRect);

            if (uiBadgeFmt && p.second) {
                DWRITE_TEXT_ALIGNMENT old = uiBadgeFmt->GetTextAlignment();
                uiBadgeFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                rt->DrawText(p.first.c_str(), (UINT32)p.first.size(), uiBadgeFmt,
                    D2D1::RectF(pRect.left, pRect.top + 2.0f, pRect.right, pRect.bottom), p.second);
                uiBadgeFmt->SetTextAlignment(old);
            }
            curX += (pW + 8.0f);
        }
    }

    // --- Right: Profile Glass Pills & Solid Primary CTA ---
    if (m_buttons.empty()) {
        m_buttons = {
            { IDM_SCAN_FILE, L"+ Open Binary", D2D1::RectF(0,0,0,0), ButtonStyle::Primary },
            { IDM_SAMPLE_CLEAN, L"Clean", D2D1::RectF(0,0,0,0), ButtonStyle::Segment },
            { IDM_SAMPLE_PACKED, L"Packed", D2D1::RectF(0,0,0,0), ButtonStyle::Segment },
            { IDM_SAMPLE_SYSCALL_PEB, L"Syscalls", D2D1::RectF(0,0,0,0), ButtonStyle::Segment },
            { IDM_SAMPLE_CRED_STEALER, L"Stealer", D2D1::RectF(0,0,0,0), ButtonStyle::Segment },
            { IDM_SAMPLE_INJECTION, L"Injection", D2D1::RectF(0,0,0,0), ButtonStyle::Segment }
        };
    }

    float btnRight = static_cast<float>(m_width) - 20.0f;
    float btnY = 10.0f;
    float btnH = 32.0f;

    // 1. Primary Solid CTA: + Open Binary (Amber Pill)
    float primaryW = 142.0f;
    m_buttons[0].rect = D2D1::RectF(btnRight - primaryW, btnY, btnRight, btnY + btnH);
    btnRight -= (primaryW + 16.0f);

    // 2. Segmented Sample Presets
    float segW[5] = { 60.0f, 68.0f, 76.0f, 70.0f, 78.0f };
    float totalSegW = 0.0f;
    for (float w : segW) totalSegW += (w + 6.0f);
    float segLeft = btnRight - totalSegW;

    if (segLeft > curX + 16.0f) {
        float curSegX = segLeft;
        for (size_t i = 0; i < 5; ++i) {
            m_buttons[1 + i].rect = D2D1::RectF(curSegX, btnY, curSegX + segW[i], btnY + btnH);
            curSegX += (segW[i] + 6.0f);
        }
    }

    // Render Buttons
    for (const auto& b : m_buttons) {
        if (b.rect.right <= b.rect.left) continue;
        float pressOffset = (b.isPressed) ? 1.0f : 0.0f;
        D2D1_RECT_F drawRect = D2D1::RectF(b.rect.left, b.rect.top + pressOffset, b.rect.right, b.rect.bottom + pressOffset);

        if (b.style == ButtonStyle::Primary) {
            ID2D1SolidColorBrush* bg = (b.hoverAlpha > 0.1f) ? m_accentPrimaryHoverBrush : m_accentPrimaryBrush;
            DrawSolidPill(rt, drawRect, bg, nullptr);

            if (m_fontManager.GetUiButtonFormat() && m_textWhiteBrush) {
                DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetUiButtonFormat()->GetTextAlignment();
                m_fontManager.GetUiButtonFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                rt->DrawText(b.label.c_str(), (UINT32)b.label.size(), m_fontManager.GetUiButtonFormat(),
                    D2D1::RectF(drawRect.left, drawRect.top + 6.0f, drawRect.right, drawRect.bottom), m_textWhiteBrush);
                m_fontManager.GetUiButtonFormat()->SetTextAlignment(old);
            }
        } else if (b.style == ButtonStyle::Segment) {
            bool isActive = (m_activePreset == b.id);
            DrawGlassPill(rt, drawRect, isActive, b.hoverAlpha);

            ID2D1SolidColorBrush* txt = isActive ? m_accentPrimaryBrush :
                                        (b.hoverAlpha > 0.1f) ? m_textPrimaryBrush : m_textSecondaryBrush;

            if (uiBadgeFmt && txt) {
                DWRITE_TEXT_ALIGNMENT old = uiBadgeFmt->GetTextAlignment();
                uiBadgeFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                rt->DrawText(b.label.c_str(), (UINT32)b.label.size(), uiBadgeFmt,
                    D2D1::RectF(drawRect.left, drawRect.top + 7.0f, drawRect.right, drawRect.bottom), txt);
                uiBadgeFmt->SetTextAlignment(old);
            }
        }
    }
}

void GhostWindow::RenderExecutiveInspectionPanel(ID2D1RenderTarget* rt, float x, float y, float w, float h) {
    // Floating Liquid Glass Sidebar Panel (24px radius)
    DrawGlassPanel(rt, D2D1::RectF(x, y, x + w, y + h), 24.0f);

    float innerX = x + 18.0f;
    float innerW = w - 36.0f;
    float curY = y + 18.0f;

    // Header: Executive triage assessment (sentence case per unslop)
    if (m_fontManager.GetUiBadgeFormat() && m_textMutedBrush) {
        rt->DrawText(L"Executive triage assessment", 27, m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(innerX, curY, innerX + innerW, curY + 18.0f), m_textMutedBrush);
        curY += 24.0f;
    }

    // Verdict Hero Glass Unit (20px radius)
    float heroH = 92.0f;
    D2D1_RECT_F heroRect = D2D1::RectF(innerX, curY, innerX + innerW, curY + heroH);
    DrawGlassPanel(rt, heroRect, 18.0f);

    int score = m_currentReport ? m_currentReport->threatScore : 0;
    ID2D1SolidColorBrush* verdictAccent = (score >= 60) ? m_threatBrush :
                                          (score >= 20) ? m_warnBrush :
                                          (m_currentReport && m_currentReport->isInstaller) ? m_cyanAccentBrush : m_passBrush;
    ID2D1SolidColorBrush* verdictBg = (score >= 60) ? m_threatBgBrush :
                                      (score >= 20) ? m_warnBgBrush :
                                      (m_currentReport && m_currentReport->isInstaller) ? m_glassFillActiveBrush : m_passBgBrush;

    // Left side: Threat Score Number
    float numBoxW = 90.0f;
    std::wstring scoreStr = std::to_wstring(static_cast<int>(m_animatedThreatScore + 0.5f));
    if (m_fontManager.GetUiMetricFormat() && m_textPrimaryBrush) {
        rt->DrawText(scoreStr.c_str(), (UINT32)scoreStr.size(), m_fontManager.GetUiMetricFormat(),
            D2D1::RectF(innerX + 14.0f, curY + 14.0f, innerX + 70.0f, curY + 50.0f), m_textPrimaryBrush);
    }
    if (m_fontManager.GetUiLabelFormat() && m_textDimBrush) {
        rt->DrawText(L"/ 100", 5, m_fontManager.GetUiLabelFormat(),
            D2D1::RectF(innerX + 54.0f, curY + 24.0f, innerX + 95.0f, curY + 42.0f), m_textDimBrush);
        rt->DrawText(L"Threat score", 12, m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(innerX + 14.0f, curY + 54.0f, innerX + numBoxW, curY + 72.0f), m_textDimBrush);
    }

    // Right side: Verdict Badge Pill + Description
    float badgeX = innerX + numBoxW + 8.0f;
    float badgeW = innerW - numBoxW - 16.0f;

    std::wstring verdictBadgeText = !m_currentReport ? L"Awaiting binary" :
                                    (score >= 60) ? L"Critical threat" :
                                    (score >= 20) ? L"Suspicious" :
                                    (m_currentReport && m_currentReport->isInstaller) ? L"Safe installer" : L"Clean / pass";

    D2D1_RECT_F vPillRect = D2D1::RectF(badgeX, curY + 16.0f, badgeX + badgeW, curY + 38.0f);
    DrawSolidPill(rt, vPillRect, verdictBg, verdictAccent);

    if (m_fontManager.GetUiBadgeFormat() && verdictAccent) {
        DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetUiBadgeFormat()->GetTextAlignment();
        m_fontManager.GetUiBadgeFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawText(verdictBadgeText.c_str(), (UINT32)verdictBadgeText.size(), m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(vPillRect.left, vPillRect.top + 3.0f, vPillRect.right, vPillRect.bottom), verdictAccent);
        m_fontManager.GetUiBadgeFormat()->SetTextAlignment(old);
    }

    std::wstring verdictSub = !m_currentReport ? L"Select a sample or drop a PE binary." :
                              (score >= 60) ? L"Malicious patterns triggered." :
                              (score >= 20) ? L"Heuristic discrepancies found." :
                              (m_currentReport && m_currentReport->isInstaller) ? L"Valid installation archive." : L"Zero threat flags detected.";

    if (m_fontManager.GetUiLabelFormat() && m_textSecondaryBrush) {
        rt->DrawText(verdictSub.c_str(), (UINT32)verdictSub.size(), m_fontManager.GetUiLabelFormat(),
            D2D1::RectF(badgeX, curY + 46.0f, badgeX + badgeW, curY + 74.0f), m_textSecondaryBrush);
    }

    curY += (heroH + 18.0f);

    // Divider
    if (m_glassBorderBrush) {
        rt->DrawLine(D2D1::Point2F(innerX, curY), D2D1::Point2F(innerX + innerW, curY), m_glassBorderBrush, 1.0f);
        curY += 14.0f;
    }

    // Telemetry Key-Value Rows (Unslop: plain, active facts)
    auto DrawTelemetryRow = [&](const std::wstring& label, const std::wstring& value, ID2D1SolidColorBrush* valBrush = nullptr) {
        if (!valBrush) valBrush = m_textPrimaryBrush;
        if (m_fontManager.GetUiBadgeFormat() && m_textDimBrush) {
            rt->DrawText(label.c_str(), (UINT32)label.size(), m_fontManager.GetUiBadgeFormat(),
                D2D1::RectF(innerX, curY, innerX + innerW, curY + 16.0f), m_textDimBrush);
        }
        if (m_fontManager.GetUiLabelFormat() && valBrush) {
            rt->DrawText(value.c_str(), (UINT32)value.size(), m_fontManager.GetUiLabelFormat(),
                D2D1::RectF(innerX, curY + 16.0f, innerX + innerW, curY + 34.0f), valBrush);
        }
        curY += 38.0f;
    };

    // Row 1: Digital signature
    bool hasValidCert = (m_currentReport && m_currentReport->signature.isValid);
    bool isSigned = (m_currentReport && m_currentReport->signature.isSigned);
    std::wstring sigStr = !m_currentReport ? L"Standby for PE file" :
                          hasValidCert ? (L"Verified (" + Utf8ToWide(m_currentReport->signature.signerSubject) + L")") :
                          isSigned ? L"Unverified / Self-signed certificate" : L"Unsigned / Missing Authenticode";
    ID2D1SolidColorBrush* sigColor = hasValidCert ? m_passBrush : isSigned ? m_warnBrush : m_textMutedBrush;
    DrawTelemetryRow(L"Digital signature", sigStr, sigColor);

    // Row 2: Shannon entropy + 4px smooth progress bar
    float ent = m_currentReport ? static_cast<float>(m_currentReport->overallEntropy) : 0.0f;
    bool isHighEnt = (ent > 7.20f);
    std::wstring entStr = !m_currentReport ? L"0.00 / 8.00 (Unloaded)" :
                          std::format(L"{:.2f} / 8.00  ({})", m_animatedEntropy, isHighEnt ? L"Packed / High" : L"Normal distribution");
    ID2D1SolidColorBrush* entColor = isHighEnt ? m_warnBrush : m_cyanAccentBrush;

    if (m_fontManager.GetUiBadgeFormat() && m_textDimBrush) {
        rt->DrawText(L"Shannon entropy", 15, m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(innerX, curY, innerX + innerW, curY + 16.0f), m_textDimBrush);
    }
    if (m_fontManager.GetUiLabelFormat() && entColor) {
        rt->DrawText(entStr.c_str(), (UINT32)entStr.size(), m_fontManager.GetUiLabelFormat(),
            D2D1::RectF(innerX, curY + 16.0f, innerX + innerW, curY + 32.0f), entColor);
    }

    // 4px Entropy Progress Track
    float barY = curY + 34.0f;
    float barH = 4.0f;
    D2D1_RECT_F barRect = D2D1::RectF(innerX, barY, innerX + innerW, barY + barH);
    DrawGlassPill(rt, barRect);

    float eFill = innerW * std::min(1.0f, m_animatedEntropy / 8.0f);
    if (eFill > 2.0f && entColor) {
        DrawSolidPill(rt, D2D1::RectF(innerX, barY, innerX + eFill, barY + barH), entColor, nullptr);
    }
    // 7.20 threshold marker
    float tickX = innerX + innerW * (7.20f / 8.00f);
    if (m_warnBrush) {
        rt->DrawLine(D2D1::Point2F(tickX, barY - 1.0f), D2D1::Point2F(tickX, barY + barH + 1.0f), m_warnBrush, 1.0f);
    }
    curY += 46.0f;

    // Row 3: Architecture
    std::wstring archStr = m_currentReport ? (Utf8ToWide(m_currentReport->machineType) + L", " + Utf8ToWide(m_currentReport->subsystem)) : L"Unloaded";
    DrawTelemetryRow(L"Architecture", archStr);

    // Row 4: Dependencies
    size_t impCount = m_currentReport ? m_currentReport->imports.size() : 0;
    std::wstring impStr = std::format(L"{} resolved dependencies", impCount);
    DrawTelemetryRow(L"Imports", impStr);

    // Row 5: Latency
    double lat = m_currentReport ? m_currentReport->analysisTimeMs : 0.0;
    std::wstring latStr = m_currentReport ? std::format(L"{:.1f} ms (Offline static analysis)", lat) : L"Ready";
    DrawTelemetryRow(L"Triage latency", latStr, m_passBrush);

    // Drop Zone Target at Bottom
    float dropH = 96.0f;
    float dropY = (y + h) - dropH - 18.0f;
    m_dropZoneRect = D2D1::RectF(innerX, dropY, innerX + innerW, dropY + dropH);

    D2D1_ROUNDED_RECT dropR = D2D1::RoundedRect(m_dropZoneRect, 16.0f, 16.0f);
    ID2D1SolidColorBrush* dropBg = (m_dropHoverAlpha > 0.05f) ? m_glassFillHoverBrush : m_glassFillBrush;
    if (dropBg) rt->FillRoundedRectangle(dropR, dropBg);

    ID2D1SolidColorBrush* dropBorder = (m_dropHoverAlpha > 0.05f) ? m_accentPrimaryBrush : m_glassBorderBrush;
    if (dropBorder && m_dashedStroke) {
        rt->DrawRoundedRectangle(dropR, dropBorder, 1.0f, m_dashedStroke);
    }

    float dropCenterX = innerX + innerW * 0.5f;
    float iconY = dropY + 24.0f;
    ID2D1SolidColorBrush* glyphBrush = (m_dropHoverAlpha > 0.05f) ? m_accentPrimaryBrush : m_textMutedBrush;

    DrawVectorUploadIcon(rt, dropCenterX, iconY, 16.0f, glyphBrush);

    if (m_fontManager.GetUiButtonFormat() && m_textPrimaryBrush) {
        std::wstring dText = m_dropHover ? L"Drop binary to analyze" : L"Drop PE binary target";
        DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetUiButtonFormat()->GetTextAlignment();
        m_fontManager.GetUiButtonFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawText(dText.c_str(), (UINT32)dText.size(), m_fontManager.GetUiButtonFormat(),
            D2D1::RectF(innerX, dropY + 42.0f, innerX + innerW, dropY + 60.0f), m_textPrimaryBrush);
        m_fontManager.GetUiButtonFormat()->SetTextAlignment(old);
    }

    if (m_fontManager.GetUiBadgeFormat() && m_textMutedBrush) {
        DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetUiBadgeFormat()->GetTextAlignment();
        m_fontManager.GetUiBadgeFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        rt->DrawText(L"Accepts .exe, .dll, and .sys files. Click to browse.", 51, m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(innerX, dropY + 66.0f, innerX + innerW, dropY + 84.0f), m_textMutedBrush);
        m_fontManager.GetUiBadgeFormat()->SetTextAlignment(old);
    }
}

void GhostWindow::RenderWorkbench(ID2D1RenderTarget* rt, float x, float y, float w, float h) {
    // Main Workbench Floating Glass Container (24px radius)
    DrawGlassPanel(rt, D2D1::RectF(x, y, x + w, y + h), 24.0f);

    // Segmented Tab Header Bar (Height: 40px)
    float tabH = 40.0f;
    float tabW[3] = { 110.0f, 175.0f, 185.0f };
    std::wstring tabLabels[3] = {
        L"Overview",
        L"Sections and headers",
        L"Telemetry log"
    };

    size_t logCount = m_currentReport ? m_currentReport->logEntries.size() : 0;
    tabLabels[2] = std::format(L"Telemetry log ({})", logCount);

    float curTabX = x + 18.0f;
    for (int i = 0; i < 3; ++i) {
        m_tabRects[i] = D2D1::RectF(curTabX, y + 10.0f, curTabX + tabW[i], y + 10.0f + 32.0f);

        bool isActive = (m_activeTab == i);
        DrawGlassPill(rt, m_tabRects[i], isActive, m_tabHoverAlphas[i]);

        ID2D1SolidColorBrush* txtBrush = isActive ? m_accentPrimaryBrush :
                                         (m_tabHoverAlphas[i] > 0.1f) ? m_textPrimaryBrush : m_textSecondaryBrush;

        if (m_fontManager.GetUiButtonFormat() && txtBrush) {
            DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetUiButtonFormat()->GetTextAlignment();
            m_fontManager.GetUiButtonFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            rt->DrawText(tabLabels[i].c_str(), (UINT32)tabLabels[i].size(), m_fontManager.GetUiButtonFormat(),
                D2D1::RectF(m_tabRects[i].left, m_tabRects[i].top + 6.0f, m_tabRects[i].right, m_tabRects[i].bottom), txtBrush);
            m_fontManager.GetUiButtonFormat()->SetTextAlignment(old);
        }

        curTabX += (tabW[i] + 10.0f);
    }

    // Right side of tab bar: Quick Metrics Status
    if (m_currentReport && m_fontManager.GetUiBadgeFormat() && m_textMutedBrush) {
        std::wstring quickChip = std::format(L"Sections: {} | Imports: {} | Entropy: {:.2f}",
            m_currentReport->sections.size(),
            m_currentReport->imports.size(),
            m_currentReport->overallEntropy
        );
        DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetUiBadgeFormat()->GetTextAlignment();
        m_fontManager.GetUiBadgeFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawText(quickChip.c_str(), (UINT32)quickChip.size(), m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(x + w - 360.0f, y + 16.0f, x + w - 20.0f, y + tabH + 6.0f), m_textMutedBrush);
        m_fontManager.GetUiBadgeFormat()->SetTextAlignment(old);
    }

    // Divider
    if (m_glassBorderBrush) {
        rt->DrawLine(D2D1::Point2F(x, y + tabH + 12.0f), D2D1::Point2F(x + w, y + tabH + 12.0f), m_glassBorderBrush, 1.0f);
    }

    // Tab Canvas
    float canvasY = y + tabH + 20.0f;
    float canvasH = h - tabH - 32.0f;
    float canvasW = w - 36.0f;
    float canvasX = x + 18.0f;

    if (m_activeTab == 0) {
        RenderTabOverview(rt, canvasX, canvasY, canvasW, canvasH);
    } else if (m_activeTab == 1) {
        RenderTabSections(rt, canvasX, canvasY, canvasW, canvasH);
    } else {
        RenderTabTelemetry(rt, canvasX, canvasY, canvasW, canvasH);
    }
}

void GhostWindow::RenderTabOverview(ID2D1RenderTarget* rt, float x, float y, float w, float h) {
    // --- Sub-panel 1: Static Heuristics and Findings ---
    float topH = std::min(310.0f, h * 0.60f);
    D2D1_RECT_F box1 = D2D1::RectF(x, y, x + w, y + topH);
    DrawGlassPanel(rt, box1, 18.0f);

    if (m_fontManager.GetUiBadgeFormat() && m_textMutedBrush) {
        rt->DrawText(L"Static heuristics and findings", 30, m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(x + 16.0f, y + 14.0f, x + w - 16.0f, y + 30.0f), m_textMutedBrush);
    }

    float findY = y + 40.0f;
    if (m_currentReport && !m_currentReport->technicalDetails.empty()) {
        size_t maxRows = static_cast<size_t>(std::max(1, static_cast<int>((topH - 50.0f) / 26.0f)));
        size_t count = std::min(m_currentReport->technicalDetails.size(), maxRows);

        for (size_t i = 0; i < count; ++i) {
            const auto& detail = m_currentReport->technicalDetails[i];
            std::wstring wDetail = Utf8ToWide(detail);

            std::wstring tagStr = L"INFO";
            std::wstring msgStr = wDetail;
            ID2D1SolidColorBrush* tagColor = m_passBrush;
            ID2D1SolidColorBrush* tagBg = m_passBgBrush;
            ID2D1SolidColorBrush* msgColor = m_textSecondaryBrush;

            if (detail.starts_with("[CRITICAL]")) {
                tagStr = L"CRITICAL";
                tagColor = m_threatBrush;
                tagBg = m_threatBgBrush;
                msgColor = m_textPrimaryBrush;
                msgStr = Utf8ToWide(detail.substr(10));
            } else if (detail.starts_with("[WARNING]")) {
                tagStr = L"WARNING";
                tagColor = m_warnBrush;
                tagBg = m_warnBgBrush;
                msgColor = m_textPrimaryBrush;
                msgStr = Utf8ToWide(detail.substr(9));
            } else if (detail.starts_with("[INFO]")) {
                tagStr = L"INFO";
                tagColor = m_passBrush;
                tagBg = m_passBgBrush;
                msgColor = m_textSecondaryBrush;
                msgStr = Utf8ToWide(detail.substr(6));
            }

            size_t firstNonSpace = msgStr.find_first_not_of(L" \t");
            if (firstNonSpace != std::wstring::npos) msgStr = msgStr.substr(firstNonSpace);

            // Subtle row striping
            if (i % 2 == 1 && m_glassFillBrush) {
                rt->FillRectangle(D2D1::RectF(x + 8.0f, findY - 2.0f, x + w - 8.0f, findY + 22.0f), m_glassFillBrush);
            }

            // Status Pill
            float pillW = 72.0f;
            float pillH = 18.0f;
            D2D1_RECT_F pRect = D2D1::RectF(x + 14.0f, findY, x + 14.0f + pillW, findY + pillH);
            DrawSolidPill(rt, pRect, tagBg, tagColor);

            if (m_fontManager.GetUiBadgeFormat() && tagColor) {
                DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetUiBadgeFormat()->GetTextAlignment();
                m_fontManager.GetUiBadgeFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                rt->DrawText(tagStr.c_str(), (UINT32)tagStr.size(), m_fontManager.GetUiBadgeFormat(),
                    D2D1::RectF(pRect.left, pRect.top + 2.0f, pRect.right, pRect.bottom), tagColor);
                m_fontManager.GetUiBadgeFormat()->SetTextAlignment(old);
            }

            // Message Text
            float msgX = x + 14.0f + pillW + 12.0f;
            if (m_fontManager.GetMonoCodeFormat() && msgColor) {
                rt->DrawText(msgStr.c_str(), (UINT32)msgStr.size(), m_fontManager.GetMonoCodeFormat(),
                    D2D1::RectF(msgX, findY + 1.0f, x + w - 16.0f, findY + 23.0f), msgColor);
            }

            findY += 26.0f;
        }
    } else {
        if (m_fontManager.GetMonoCodeFormat() && m_textMutedBrush) {
            std::wstring cleanMsg = m_currentReport ?
                L"No static threat indicators detected. Headers and sections match standard Windows binaries." :
                L"Select a preset sample above or drop a PE executable to inspect headers, section entropy, and disassembled instructions.";
            rt->DrawText(cleanMsg.c_str(), (UINT32)cleanMsg.size(), m_fontManager.GetMonoCodeFormat(),
                D2D1::RectF(x + 16.0f, findY + 12.0f, x + w - 16.0f, findY + 36.0f), m_textMutedBrush);
        }
    }

    // --- Sub-panel 2: Cryptographic Identifiers ---
    float botY = y + topH + 14.0f;
    float botH = h - (botY - y);
    if (botH > 60.0f) {
        D2D1_RECT_F box2 = D2D1::RectF(x, botY, x + w, botY + botH);
        DrawGlassPanel(rt, box2, 18.0f);

        if (m_fontManager.GetUiBadgeFormat() && m_textMutedBrush) {
            rt->DrawText(L"Cryptographic hashes and identifiers", 36, m_fontManager.GetUiBadgeFormat(),
                D2D1::RectF(x + 16.0f, botY + 12.0f, x + w - 16.0f, botY + 28.0f), m_textMutedBrush);
        }

        float colW = (w - 32.0f - 36.0f) / 4.0f;
        std::wstring hLabels[4] = { L"SHA-256", L"IMPHASH", L"MD5", L"SHA-1" };
        std::wstring hValues[4] = {
            m_currentReport ? Utf8ToWide(m_currentReport->sha256) : L"Pending ingestion",
            m_currentReport ? Utf8ToWide(m_currentReport->imphash.empty() ? "N/A" : m_currentReport->imphash) : L"Pending ingestion",
            m_currentReport ? Utf8ToWide(m_currentReport->md5) : L"Pending ingestion",
            m_currentReport ? Utf8ToWide(m_currentReport->sha1) : L"Pending ingestion"
        };

        for (size_t i = 0; i < 4; ++i) {
            float hX = x + 16.0f + i * (colW + 12.0f);
            float hY = botY + 32.0f;
            float hH = botH - 46.0f;

            D2D1_RECT_F hcR = D2D1::RectF(hX, hY, hX + colW, hY + hH);
            DrawGlassPanel(rt, hcR, 12.0f);

            if (m_fontManager.GetUiBadgeFormat() && m_textDimBrush) {
                rt->DrawText(hLabels[i].c_str(), (UINT32)hLabels[i].size(), m_fontManager.GetUiBadgeFormat(),
                    D2D1::RectF(hX + 12.0f, hY + 8.0f, hX + colW - 8.0f, hY + 22.0f), m_textDimBrush);
            }

            std::wstring displayHash = hValues[i];
            if (displayHash.size() > 22) displayHash = displayHash.substr(0, 20) + L"..";
            if (m_fontManager.GetMonoCodeFormat() && m_textPrimaryBrush) {
                rt->DrawText(displayHash.c_str(), (UINT32)displayHash.size(), m_fontManager.GetMonoCodeFormat(),
                    D2D1::RectF(hX + 12.0f, hY + 26.0f, hX + colW - 8.0f, hY + hH - 6.0f), m_textPrimaryBrush);
            }
        }
    }
}

void GhostWindow::RenderTabSections(ID2D1RenderTarget* rt, float x, float y, float w, float h) {
    DrawGlassPanel(rt, D2D1::RectF(x, y, x + w, y + h), 18.0f);

    if (m_fontManager.GetUiBadgeFormat() && m_textMutedBrush) {
        rt->DrawText(L"Section memory layout and entropy density", 41, m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(x + 16.0f, y + 14.0f, x + 380.0f, y + 30.0f), m_textMutedBrush);
    }

    // Legend
    float legX = x + w - 330.0f;
    if (m_fontManager.GetUiBadgeFormat() && m_textMutedBrush) {
        if (m_passBrush) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(legX, y + 20.0f), 3.5f, 3.5f), m_passBrush);
        rt->DrawText(L"Code (<6.0)", 11, m_fontManager.GetUiBadgeFormat(), D2D1::RectF(legX + 8.0f, y + 13.0f, legX + 85.0f, y + 27.0f), m_textMutedBrush);

        if (m_cyanAccentBrush) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(legX + 95.0f, y + 20.0f), 3.5f, 3.5f), m_cyanAccentBrush);
        rt->DrawText(L"Data (6.0-7.2)", 14, m_fontManager.GetUiBadgeFormat(), D2D1::RectF(legX + 103.0f, y + 13.0f, legX + 200.0f, y + 27.0f), m_textMutedBrush);

        if (m_threatBrush) rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(legX + 205.0f, y + 20.0f), 3.5f, 3.5f), m_threatBrush);
        rt->DrawText(L"Packed (>7.2)", 13, m_fontManager.GetUiBadgeFormat(), D2D1::RectF(legX + 213.0f, y + 13.0f, legX + 310.0f, y + 27.0f), m_textMutedBrush);
    }

    // Visual Heatmap Bar (Height: 26px)
    float mapBarY = y + 36.0f;
    float mapBarW = w - 32.0f;
    DrawGlassPill(rt, D2D1::RectF(x + 16.0f, mapBarY, x + 16.0f + mapBarW, mapBarY + 26.0f));

    if (m_currentReport && !m_currentReport->sections.empty()) {
        uint32_t totalVirt = 0;
        for (const auto& s : m_currentReport->sections) totalVirt += std::max(1u, s.virtualSize);
        if (totalVirt == 0) totalVirt = 1;

        float curBlockX = x + 16.0f;
        for (size_t i = 0; i < m_currentReport->sections.size(); ++i) {
            const auto& s = m_currentReport->sections[i];
            float blockPct = static_cast<float>(s.virtualSize) / static_cast<float>(totalVirt);
            float blockW = std::max(46.0f, mapBarW * blockPct);
            if (curBlockX + blockW > x + 16.0f + mapBarW || i == m_currentReport->sections.size() - 1) {
                blockW = (x + 16.0f + mapBarW) - curBlockX;
            }
            if (blockW <= 0) break;

            ID2D1SolidColorBrush* bColor = (s.entropy > 7.2f) ? m_threatBrush : (s.entropy >= 6.0f) ? m_cyanAccentBrush : m_passBrush;
            D2D1_RECT_F blockR = D2D1::RectF(curBlockX, mapBarY + 1.0f, curBlockX + blockW - 2.0f, mapBarY + 25.0f);
            DrawSolidPill(rt, blockR, bColor, nullptr);

            std::wstring label = Utf8ToWide(s.name) + std::format(L" ({:.1f})", s.entropy);
            if (blockW >= 55.0f && m_fontManager.GetMonoBadgeFormat() && m_textWhiteBrush) {
                DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetMonoBadgeFormat()->GetTextAlignment();
                m_fontManager.GetMonoBadgeFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                rt->DrawText(label.c_str(), (UINT32)label.size(), m_fontManager.GetMonoBadgeFormat(),
                    D2D1::RectF(curBlockX, mapBarY + 6.0f, curBlockX + blockW - 2.0f, mapBarY + 23.0f), m_textWhiteBrush);
                m_fontManager.GetMonoBadgeFormat()->SetTextAlignment(old);
            }
            curBlockX += blockW;
        }
    }

    // Table Column Headers
    float tblY = mapBarY + 38.0f;
    float cCol1 = x + 18.0f;
    float cCol2 = cCol1 + 100.0f;
    float cCol3 = cCol2 + 110.0f;
    float cCol4 = cCol3 + 105.0f;
    float cCol5 = cCol4 + 180.0f;

    if (m_fontManager.GetMonoBadgeFormat() && m_textDimBrush) {
        rt->DrawText(L"Section", 7, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(cCol1, tblY, cCol2, tblY + 16.0f), m_textDimBrush);
        rt->DrawText(L"Virtual Addr", 12, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(cCol2, tblY, cCol3, tblY + 16.0f), m_textDimBrush);
        rt->DrawText(L"Virtual Size", 12, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(cCol3, tblY, cCol4, tblY + 16.0f), m_textDimBrush);
        rt->DrawText(L"Entropy", 7, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(cCol4, tblY, cCol5, tblY + 16.0f), m_textDimBrush);
        rt->DrawText(L"Permissions and Verdict", 23, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(cCol5, tblY, x + w - 18.0f, tblY + 16.0f), m_textDimBrush);
    }

    if (m_glassBorderBrush) {
        rt->DrawLine(D2D1::Point2F(x + 16.0f, tblY + 18.0f), D2D1::Point2F(x + w - 16.0f, tblY + 18.0f), m_glassBorderBrush, 1.0f);
    }

    // Table Rows
    float rY = tblY + 24.0f;
    if (m_currentReport && !m_currentReport->sections.empty()) {
        size_t maxRows = static_cast<size_t>(std::max(1, static_cast<int>((h - (rY - y) - 10.0f) / 25.0f)));
        size_t count = std::min(m_currentReport->sections.size(), maxRows);

        for (size_t i = 0; i < count; ++i) {
            const auto& sec = m_currentReport->sections[i];

            if (i % 2 == 1 && m_glassFillBrush) {
                rt->FillRectangle(D2D1::RectF(x + 10.0f, rY - 2.0f, x + w - 10.0f, rY + 21.0f), m_glassFillBrush);
            }

            std::wstring sName = Utf8ToWide(sec.name);
            std::wstring sAddr = std::format(L"0x{:08X}", sec.virtualAddress);
            std::wstring sSize = std::format(L"0x{:X} ({})", sec.virtualSize, Utf8ToWide(FormatFileSize(sec.virtualSize)));
            std::wstring sEnt = std::format(L"{:.2f}", sec.entropy);

            if (m_fontManager.GetMonoCodeFormat() && m_textPrimaryBrush) {
                rt->DrawText(sName.c_str(), (UINT32)sName.size(), m_fontManager.GetMonoCodeFormat(), D2D1::RectF(cCol1, rY, cCol2, rY + 18.0f), m_textPrimaryBrush);
            }
            if (m_fontManager.GetMonoCodeFormat() && m_textSecondaryBrush) {
                rt->DrawText(sAddr.c_str(), (UINT32)sAddr.size(), m_fontManager.GetMonoCodeFormat(), D2D1::RectF(cCol2, rY, cCol3, rY + 18.0f), m_textSecondaryBrush);
                rt->DrawText(sSize.c_str(), (UINT32)sSize.size(), m_fontManager.GetMonoCodeFormat(), D2D1::RectF(cCol3, rY, cCol4, rY + 18.0f), m_textSecondaryBrush);
            }

            // Entropy Mini Progress Bar (4px)
            float miniW = 85.0f;
            float miniFilled = miniW * std::min(1.0f, static_cast<float>(sec.entropy) / 8.0f);
            DrawGlassPill(rt, D2D1::RectF(cCol4, rY + 7.0f, cCol4 + miniW, rY + 11.0f));

            ID2D1SolidColorBrush* secColor = (sec.entropy > 7.2f) ? m_threatBrush : (sec.entropy >= 6.0f) ? m_cyanAccentBrush : m_passBrush;
            if (secColor && miniFilled > 2.0f) {
                DrawSolidPill(rt, D2D1::RectF(cCol4, rY + 7.0f, cCol4 + miniFilled, rY + 11.0f), secColor, nullptr);
            }
            if (m_fontManager.GetMonoCodeFormat() && secColor) {
                rt->DrawText(sEnt.c_str(), (UINT32)sEnt.size(), m_fontManager.GetMonoCodeFormat(), D2D1::RectF(cCol4 + miniW + 10.0f, rY, cCol5, rY + 18.0f), secColor);
            }

            // Permissions / Verdict
            std::wstring flagStr;
            ID2D1SolidColorBrush* flagColor = m_passBrush;
            if (sec.isRwx) {
                flagStr = L"[CRIT] RWX executable and writable";
                flagColor = m_threatBrush;
            } else if (sec.isSuspiciousEntropy) {
                flagStr = (m_currentReport->isInstaller) ? L"[PASS] Compressed archive" : L"[WARN] High entropy / packed";
                flagColor = (m_currentReport->isInstaller) ? m_cyanAccentBrush : m_warnBrush;
            } else {
                flagStr = sec.isExecutable ? L"Executable code [R-X]" : L"Data segment [R--]";
                flagColor = m_textSecondaryBrush;
            }

            if (m_fontManager.GetMonoCodeFormat() && flagColor) {
                rt->DrawText(flagStr.c_str(), (UINT32)flagStr.size(), m_fontManager.GetMonoCodeFormat(), D2D1::RectF(cCol5, rY, x + w - 18.0f, rY + 18.0f), flagColor);
            }

            rY += 25.0f;
        }
    } else {
        if (m_fontManager.GetMonoCodeFormat() && m_textDimBrush) {
            std::wstring noSec = L"No sections loaded. Standby for binary ingestion.";
            rt->DrawText(noSec.c_str(), (UINT32)noSec.size(), m_fontManager.GetMonoCodeFormat(),
                D2D1::RectF(x + 18.0f, rY + 10.0f, x + w - 18.0f, rY + 30.0f), m_textDimBrush);
        }
    }
}

void GhostWindow::RenderTabTelemetry(ID2D1RenderTarget* rt, float x, float y, float w, float h) {
    DrawGlassPanel(rt, D2D1::RectF(x, y, x + w, y + h), 18.0f);

    float colTimeW = 90.0f;
    float colSubsysW = 110.0f;
    float colStatusW = 90.0f;

    float c1X = x + 18.0f;
    float c2X = c1X + colTimeW;
    float c3X = c2X + colSubsysW;
    float c4X = c3X + colStatusW;

    if (m_fontManager.GetMonoBadgeFormat() && m_textDimBrush) {
        rt->DrawText(L"Time", 4, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(c1X, y + 12.0f, c2X, y + 28.0f), m_textDimBrush);
        rt->DrawText(L"Subsystem", 9, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(c2X, y + 12.0f, c3X, y + 28.0f), m_textDimBrush);
        rt->DrawText(L"Status", 6, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(c3X, y + 12.0f, c4X, y + 28.0f), m_textDimBrush);
        rt->DrawText(L"Telemetry details", 17, m_fontManager.GetMonoBadgeFormat(), D2D1::RectF(c4X, y + 12.0f, x + w - 18.0f, y + 28.0f), m_textDimBrush);
    }

    if (m_glassBorderBrush) {
        rt->DrawLine(D2D1::Point2F(x + 16.0f, y + 30.0f), D2D1::Point2F(x + w - 16.0f, y + 30.0f), m_glassBorderBrush, 1.0f);
    }

    float entryY = y + 38.0f;
    if (m_currentReport && !m_currentReport->logEntries.empty()) {
        size_t maxLogs = static_cast<size_t>(std::max(1, static_cast<int>((h - 48.0f) / 23.0f)));
        size_t count = std::min(m_currentReport->logEntries.size(), maxLogs);

        for (size_t i = 0; i < count; ++i) {
            const auto& entry = m_currentReport->logEntries[i];

            if (i % 2 == 1 && m_glassFillBrush) {
                rt->FillRectangle(D2D1::RectF(x + 8.0f, entryY - 2.0f, x + w - 8.0f, entryY + 20.0f), m_glassFillBrush);
            }

            // Col 1: Time
            std::wstring ts = Utf8ToWide(entry.timestamp);
            if (m_fontManager.GetMonoLogFormat() && m_textDimBrush) {
                rt->DrawText(ts.c_str(), (UINT32)ts.size(), m_fontManager.GetMonoLogFormat(),
                    D2D1::RectF(c1X, entryY, c2X - 8.0f, entryY + 18.0f), m_textDimBrush);
            }

            // Col 2: Subsystem
            std::wstring subsys = L"[" + Utf8ToWide(entry.subsystem) + L"]";
            if (m_fontManager.GetMonoLogFormat() && m_textSecondaryBrush) {
                rt->DrawText(subsys.c_str(), (UINT32)subsys.size(), m_fontManager.GetMonoLogFormat(),
                    D2D1::RectF(c2X, entryY, c3X - 8.0f, entryY + 18.0f), m_textSecondaryBrush);
            }

            // Col 3: Status Badge
            ID2D1SolidColorBrush* statusBrush = m_textSecondaryBrush;
            ID2D1SolidColorBrush* statusBg = nullptr;
            std::wstring statusText = Utf8ToWide(entry.level);

            if (entry.level == "CRIT") {
                statusBrush = m_threatBrush;
                statusBg = m_threatBgBrush;
                statusText = L"CRITICAL";
            } else if (entry.level == "WARN") {
                statusBrush = m_warnBrush;
                statusBg = m_warnBgBrush;
                statusText = L"WARN";
            } else if (entry.level == "PASS") {
                statusBrush = m_passBrush;
                statusBg = m_passBgBrush;
                statusText = L"PASS";
            } else if (entry.level == "AUDIT" || entry.level == "INFO") {
                statusBrush = m_cyanAccentBrush;
            }

            if (statusBg) {
                DrawSolidPill(rt, D2D1::RectF(c3X, entryY + 1.0f, c3X + 62.0f, entryY + 17.0f), statusBg, nullptr);
            }
            if (m_fontManager.GetMonoBadgeFormat() && statusBrush) {
                rt->DrawText(statusText.c_str(), (UINT32)statusText.size(), m_fontManager.GetMonoBadgeFormat(),
                    D2D1::RectF(c3X + (statusBg ? 4.0f : 0.0f), entryY + 2.0f, c4X - 6.0f, entryY + 18.0f), statusBrush);
            }

            // Col 4: Message Text
            std::wstring msg = Utf8ToWide(entry.message);
            if (m_fontManager.GetMonoLogFormat() && m_textPrimaryBrush) {
                rt->DrawText(msg.c_str(), (UINT32)msg.size(), m_fontManager.GetMonoLogFormat(),
                    D2D1::RectF(c4X, entryY, x + w - 18.0f, entryY + 18.0f), m_textPrimaryBrush);
            }

            entryY += 23.0f;
        }
    } else {
        if (m_fontManager.GetMonoCodeFormat() && m_textDimBrush) {
            std::wstring noLogs = L"Engine idle. Direct2D renderer initialized with Liquid Morphism theme.";
            rt->DrawText(noLogs.c_str(), (UINT32)noLogs.size(), m_fontManager.GetMonoCodeFormat(),
                D2D1::RectF(x + 18.0f, entryY + 10.0f, x + w - 18.0f, entryY + 30.0f), m_textDimBrush);
        }
    }
}

void GhostWindow::RenderFooter(ID2D1RenderTarget* rt) {
    float footerH = 30.0f;
    float footerY = static_cast<float>(m_height) - footerH;
    D2D1_RECT_F footerRect = D2D1::RectF(0.0f, footerY, static_cast<float>(m_width), static_cast<float>(m_height));

    if (m_glassFillBrush) rt->FillRectangle(footerRect, m_glassFillBrush);
    if (m_glassBorderBrush) {
        rt->DrawLine(D2D1::Point2F(0.0f, footerY), D2D1::Point2F(static_cast<float>(m_width), footerY), m_glassBorderBrush, 1.0f);
    }
    if (m_glassSpecularBrush) {
        rt->DrawLine(D2D1::Point2F(0.0f, footerY + 1.0f), D2D1::Point2F(static_cast<float>(m_width), footerY + 1.0f), m_glassSpecularBrush, 1.0f);
    }

    // Engine Indicator Dot
    float dotX = 24.0f;
    float dotY = footerY + 15.0f;
    if (m_passBrush) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), 3.5f, 3.5f), m_passBrush);
    }

    if (m_fontManager.GetUiBadgeFormat() && m_textDimBrush) {
        std::wstring leftInfo = L"Engine: Active triage | Direct2D hardware accelerated | Air-gap isolated offline mode";
        rt->DrawText(leftInfo.c_str(), (UINT32)leftInfo.size(), m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(36.0f, footerY + 8.0f, 680.0f, static_cast<float>(m_height)), m_textDimBrush);

        std::wstring rightInfo = std::format(L"Native Win32 subsystem | {} x {}", m_width, m_height);
        DWRITE_TEXT_ALIGNMENT old = m_fontManager.GetUiBadgeFormat()->GetTextAlignment();
        m_fontManager.GetUiBadgeFormat()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        rt->DrawText(rightInfo.c_str(), (UINT32)rightInfo.size(), m_fontManager.GetUiBadgeFormat(),
            D2D1::RectF(static_cast<float>(m_width) - 350.0f, footerY + 8.0f, static_cast<float>(m_width) - 20.0f, static_cast<float>(m_height)), m_textDimBrush);
        m_fontManager.GetUiBadgeFormat()->SetTextAlignment(old);
    }
}

void GhostWindow::ShowContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_SCAN_FILE, L"Open Target PE Binary (Ctrl+O)...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_CLEAN, L"Load sample: Clean binary");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_PACKED, L"Load sample: High entropy packed mummy");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_SYSCALL_PEB, L"Load sample: Direct syscalls and PEB hashing");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_CRED_STEALER, L"Load sample: Credential stealer artifacts");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_INJECTION, L"Load sample: Process injection chain");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit Koltzi");

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screenX, screenY, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd != 0 && m_onCommand) {
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
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            float dt = 0.016f;
            if (m_perfFreq.QuadPart > 0 && m_lastPerfCounter.QuadPart > 0) {
                dt = static_cast<float>(now.QuadPart - m_lastPerfCounter.QuadPart) / static_cast<float>(m_perfFreq.QuadPart);
            }
            m_lastPerfCounter = now;
            dt = std::clamp(dt, 0.005f, 0.033f);
            Update(dt);
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
        mmi->ptMinTrackSize.x = 1024;
        mmi->ptMinTrackSize.y = 660;
        return 0;
    }

    case WM_KEYDOWN: {
        if (wParam == 'O' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (m_onCommand) m_onCommand(IDM_SCAN_FILE);
            return 0;
        }
        break;
    }

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int scrollStep = (delta > 0) ? -2 : 2;
        m_tabScroll[m_activeTab] = std::max(0, m_tabScroll[m_activeTab] + scrollStep);
        InvalidateRect(hwnd, nullptr, FALSE);
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

        for (int i = 0; i < 3; ++i) {
            if (x >= m_tabRects[i].left && x <= m_tabRects[i].right &&
                y >= m_tabRects[i].top && y <= m_tabRects[i].bottom) {
                m_hoveredButton = -(10 + i);
                break;
            }
        }

        bool prevDropHover = m_dropHover;
        m_dropHover = (x >= m_dropZoneRect.left && x <= m_dropZoneRect.right &&
                       y >= m_dropZoneRect.top && y <= m_dropZoneRect.bottom);

        if (m_hoveredButton != -1 || m_dropHover) {
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32649)));
        } else {
            SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
        }

        if (prevHover != m_hoveredButton || prevDropHover != m_dropHover) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float x = static_cast<float>(LOWORD(lParam));
        float y = static_cast<float>(HIWORD(lParam));

        SetCapture(hwnd);

        for (int i = 0; i < 3; ++i) {
            if (x >= m_tabRects[i].left && x <= m_tabRects[i].right &&
                y >= m_tabRects[i].top && y <= m_tabRects[i].bottom) {
                m_activeTab = i;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        for (auto& b : m_buttons) {
            if (x >= b.rect.left && x <= b.rect.right && y >= b.rect.top && y <= b.rect.bottom) {
                b.isPressed = true;
                m_pressedButton = b.id;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        if (x >= m_dropZoneRect.left && x <= m_dropZoneRect.right &&
            y >= m_dropZoneRect.top && y <= m_dropZoneRect.bottom) {
            m_pressedButton = -4;
            return 0;
        }

        return 0;
    }

    case WM_LBUTTONUP: {
        float x = static_cast<float>(LOWORD(lParam));
        float y = static_cast<float>(HIWORD(lParam));

        ReleaseCapture();

        int executedCmd = 0;
        for (auto& b : m_buttons) {
            if (b.isPressed) {
                b.isPressed = false;
                if (x >= b.rect.left && x <= b.rect.right && y >= b.rect.top && y <= b.rect.bottom) {
                    executedCmd = b.id;
                }
            }
        }

        int prevPressed = m_pressedButton;
        m_pressedButton = -1;

        if (executedCmd != 0) {
            if (executedCmd >= IDM_SAMPLE_CLEAN && executedCmd <= IDM_SAMPLE_INJECTION) {
                m_activePreset = executedCmd;
            } else if (executedCmd == IDM_SCAN_FILE) {
                m_activePreset = -1;
            }

            if (m_onCommand) {
                m_onCommand(executedCmd);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (prevPressed == -4 && (x >= m_dropZoneRect.left && x <= m_dropZoneRect.right &&
                                  y >= m_dropZoneRect.top && y <= m_dropZoneRect.bottom)) {
            m_activePreset = -1;
            if (m_onCommand) m_onCommand(IDM_SCAN_FILE);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        InvalidateRect(hwnd, nullptr, FALSE);
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
            m_activePreset = -1;
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
