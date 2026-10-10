#pragma once
#include "../Common.h"
#include "FontManager.h"
#include "../Core/TriageReport.h"
#include <memory>
#include <functional>
#include <vector>

namespace Koltzi {

enum class ButtonStyle {
    Primary,   // Solid high-contrast CTA pill
    Secondary, // Translucent glass pill
    Segment    // Glass segment pill
};

struct ToolbarButton {
    int id = 0;
    std::wstring label;
    D2D1_RECT_F rect = { 0, 0, 0, 0 };
    ButtonStyle style = ButtonStyle::Secondary;
    float hoverAlpha = 0.0f;
    bool isPressed = false;
};

class GhostWindow {
public:
    using FileDropCallback = std::function<void(const std::wstring&)>;
    using CommandCallback = std::function<void(int)>;

    GhostWindow();
    ~GhostWindow();

    bool Create();
    void Show();
    void Hide();
    void Destroy();

    HWND GetHwnd() const { return m_hwnd; }

    void SetFileDropCallback(FileDropCallback cb) { m_onFileDrop = std::move(cb); }
    void SetCommandCallback(CommandCallback cb) { m_onCommand = std::move(cb); }

    void SetReport(std::shared_ptr<TriageReport> report);
    void SetMood(GhostMood mood);
    void SetDialogue(const std::string& text, bool immediate = false);
    void ToggleHUD();

    void Update(float deltaSeconds);
    void Render();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateDeviceIndependentResources();
    bool CreateDeviceResources();
    void DiscardDeviceResources();
    void ShowContextMenu(int screenX, int screenY);

    // Liquid Morphism Drawing Primitives
    void DrawGlassPanel(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float radius,
                        bool isHovered = false, float hoverFactor = 0.0f);
    void DrawGlassPill(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect,
                       bool isHovered = false, float hoverFactor = 0.0f);
    void DrawSolidPill(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect,
                       ID2D1Brush* fillBrush, ID2D1Brush* borderBrush = nullptr);

    // Layout Sections
    void RenderBackgroundBlobs(ID2D1RenderTarget* rt);
    void RenderTopHeader(ID2D1RenderTarget* rt);
    void RenderExecutiveInspectionPanel(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    void RenderWorkbench(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    void RenderTabOverview(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    void RenderTabSections(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    void RenderTabTelemetry(ID2D1RenderTarget* rt, float x, float y, float w, float h);
    void RenderFooter(ID2D1RenderTarget* rt);

    // Vector Icon Utilities
    void DrawVectorFileIcon(ID2D1RenderTarget* rt, float cx, float cy, float size, ID2D1Brush* brush);
    void DrawVectorShieldIcon(ID2D1RenderTarget* rt, float cx, float cy, float size, ID2D1Brush* brush);
    void DrawVectorUploadIcon(ID2D1RenderTarget* rt, float cx, float cy, float size, ID2D1Brush* brush);
    void DrawVectorZapIcon(ID2D1RenderTarget* rt, float cx, float cy, float size, ID2D1Brush* brush);

    HWND m_hwnd = NULL;
    int m_width = 1280;
    int m_height = 820;

    // Direct2D Core
    ID2D1Factory* m_d2dFactory = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;
    ID2D1StrokeStyle* m_dashedStroke = nullptr;

    // Liquid Morphism Gradient & Ambient Resources
    ID2D1LinearGradientBrush* m_bgBaseGradient = nullptr;
    ID2D1RadialGradientBrush* m_blobBrush1 = nullptr;   // Warm Amber (#f59e0b)
    ID2D1RadialGradientBrush* m_blobBrush2 = nullptr;   // Burnt Sienna (#c2410c)
    ID2D1RadialGradientBrush* m_blobBrush3 = nullptr;   // Deep Amber-Brown (#78350f)

    // Liquid Glass Surface Brushes
    ID2D1SolidColorBrush* m_glassFillBrush = nullptr;       // rgba(255, 255, 255, 0.08)
    ID2D1SolidColorBrush* m_glassFillHoverBrush = nullptr;  // rgba(255, 255, 255, 0.14)
    ID2D1SolidColorBrush* m_glassFillActiveBrush = nullptr; // rgba(255, 255, 255, 0.20)
    ID2D1SolidColorBrush* m_glassBorderBrush = nullptr;     // rgba(255, 255, 255, 0.12)
    ID2D1SolidColorBrush* m_glassBorderHoverBrush = nullptr;// rgba(255, 255, 255, 0.24)
    ID2D1SolidColorBrush* m_glassSpecularBrush = nullptr;   // rgba(255, 255, 255, 0.20) inset top edge

    // Primary CTA and Semantic Accent Brushes
    ID2D1SolidColorBrush* m_accentPrimaryBrush = nullptr;      // #f59e0b Amber solid CTA
    ID2D1SolidColorBrush* m_accentPrimaryHoverBrush = nullptr; // #fbbf24 Amber hover
    ID2D1SolidColorBrush* m_accentSecondaryBrush = nullptr;    // #c2410c Burnt Sienna
    ID2D1SolidColorBrush* m_cyanAccentBrush = nullptr;         // #38bdf8 Steel Cyan

    // Status Brushes
    ID2D1SolidColorBrush* m_passBrush = nullptr;       // #10b981 Muted Emerald
    ID2D1SolidColorBrush* m_passBgBrush = nullptr;     // rgba(16, 185, 129, 0.14)
    ID2D1SolidColorBrush* m_warnBrush = nullptr;       // #f59e0b Amber
    ID2D1SolidColorBrush* m_warnBgBrush = nullptr;     // rgba(245, 158, 11, 0.14)
    ID2D1SolidColorBrush* m_threatBrush = nullptr;     // #ef4444 Soft Crimson
    ID2D1SolidColorBrush* m_threatBgBrush = nullptr;   // rgba(239, 68, 68, 0.14)

    // Typography Brushes (Warm Off-White Palette)
    ID2D1SolidColorBrush* m_textPrimaryBrush = nullptr;   // rgba(255, 248, 240, 0.95)
    ID2D1SolidColorBrush* m_textSecondaryBrush = nullptr; // rgba(255, 235, 210, 0.60)
    ID2D1SolidColorBrush* m_textMutedBrush = nullptr;     // rgba(255, 220, 180, 0.35)
    ID2D1SolidColorBrush* m_textDimBrush = nullptr;       // rgba(255, 220, 180, 0.20)
    ID2D1SolidColorBrush* m_textWhiteBrush = nullptr;     // #ffffff for solid buttons

    FontManager m_fontManager;

    GhostMood m_currentMood = GhostMood::Idle;
    std::shared_ptr<TriageReport> m_currentReport;

    std::vector<ToolbarButton> m_buttons;
    int m_hoveredButton = -1;
    int m_pressedButton = -1;
    bool m_dropHover = false;
    float m_dropHoverAlpha = 0.0f;
    D2D1_RECT_F m_dropZoneRect = { 0, 0, 0, 0 };

    // Tabs: 0 = Overview, 1 = Sections and headers, 2 = Telemetry log
    int m_activeTab = 0;
    D2D1_RECT_F m_tabRects[3] = {};
    float m_tabHoverAlphas[3] = { 0.0f, 0.0f, 0.0f };
    int m_tabScroll[3] = { 0, 0, 0 };

    // Telemetry Subsystem Filter (0: All, 1: Header, 2: Disassembly, 3: Hash, 4: Peb)
    int m_telemetryFilter = 0;
    D2D1_RECT_F m_filterPills[5] = {};

    // Smooth KPI animations
    float m_animatedThreatScore = 0.0f;
    float m_animatedEntropy = 0.0f;

    LARGE_INTEGER m_perfFreq = { 0 };
    LARGE_INTEGER m_lastPerfCounter = { 0 };
    float m_uiTime = 0.0f;
    int m_activePreset = -1;

    FileDropCallback m_onFileDrop;
    CommandCallback m_onCommand;
};

} // namespace Koltzi
