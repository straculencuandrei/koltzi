#pragma once
#include "Common.h"
#include "GhostRenderer.h"
#include "SpeechBubble.h"
#include "FontManager.h"
#include "Core/TriageReport.h"
#include <memory>
#include <functional>
#include <vector>

namespace Koltzi {

struct ToolbarButton {
    int id;
    std::wstring label;
    D2D1_RECT_F rect;
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

    void RenderTopHeader(ID2D1RenderTarget* rt);
    void RenderLeftPanel(ID2D1RenderTarget* rt);
    void RenderRightDashboard(ID2D1RenderTarget* rt);
    void RenderFooter(ID2D1RenderTarget* rt);

    HWND m_hwnd = NULL;
    int m_width = 1140;
    int m_height = 760;

    // Direct2D Resources
    ID2D1Factory* m_d2dFactory = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;

    // Solid Color Brushes
    ID2D1SolidColorBrush* m_bgBrush = nullptr;
    ID2D1SolidColorBrush* m_panelBgBrush = nullptr;
    ID2D1SolidColorBrush* m_panelBorderBrush = nullptr;
    ID2D1SolidColorBrush* m_cardBgBrush = nullptr;
    ID2D1SolidColorBrush* m_textWhiteBrush = nullptr;
    ID2D1SolidColorBrush* m_textMutedBrush = nullptr;
    ID2D1SolidColorBrush* m_textDimBrush = nullptr;
    ID2D1SolidColorBrush* m_accentBrush = nullptr;
    ID2D1SolidColorBrush* m_redBrush = nullptr;
    ID2D1SolidColorBrush* m_yellowBrush = nullptr;
    ID2D1SolidColorBrush* m_greenBrush = nullptr;
    ID2D1SolidColorBrush* m_buttonBgBrush = nullptr;
    ID2D1SolidColorBrush* m_buttonHoverBrush = nullptr;

    // DirectWrite Text Formats
    IDWriteTextFormat* m_titleFormat = nullptr;
    IDWriteTextFormat* m_subtitleFormat = nullptr;
    IDWriteTextFormat* m_headingFormat = nullptr;
    IDWriteTextFormat* m_subheadingFormat = nullptr;
    IDWriteTextFormat* m_bodyFormat = nullptr;
    IDWriteTextFormat* m_codeFormat = nullptr;
    IDWriteTextFormat* m_badgeFormat = nullptr;
    IDWriteTextFormat* m_buttonFormat = nullptr;

    GhostRenderer m_ghostRenderer;
    SpeechBubble m_speechBubble;
    FontManager m_fontManager;

    GhostMood m_currentMood = GhostMood::Idle;
    std::shared_ptr<TriageReport> m_currentReport;

    std::vector<ToolbarButton> m_buttons;
    int m_hoveredButton = -1;
    bool m_dropHover = false;
    D2D1_RECT_F m_dropZoneRect = { 0, 0, 0, 0 };

    bool m_showAuditLog = false;
    D2D1_RECT_F m_tabFindingsRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_tabAuditRect = { 0, 0, 0, 0 };

    FileDropCallback m_onFileDrop;
    CommandCallback m_onCommand;
};

} // namespace Koltzi
