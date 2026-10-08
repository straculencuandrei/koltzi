#pragma once
#include "Common.h"
#include "GhostRenderer.h"
#include "SpeechBubble.h"
#include "Core/TriageReport.h"
#include <memory>
#include <functional>

namespace Koltzi {

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

    HWND m_hwnd = NULL;
    int m_width = 580;
    int m_height = 440;
    int m_posX = 100;
    int m_posY = 100;

    bool m_isDragging = false;
    POINT m_dragStartPos = { 0, 0 };
    POINT m_dragStartWindowPos = { 0, 0 };

    // Direct2D & GDI Layered Window buffers
    HDC m_hMemDC = NULL;
    HBITMAP m_hBitmap = NULL;
    HBITMAP m_hOldBitmap = NULL;
    void* m_pBits = nullptr;

    ID2D1Factory* m_d2dFactory = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;
    ID2D1DCRenderTarget* m_dcRenderTarget = nullptr;

    GhostRenderer m_ghostRenderer;
    SpeechBubble m_speechBubble;

    GhostMood m_currentMood = GhostMood::Idle;
    std::shared_ptr<TriageReport> m_currentReport;

    FileDropCallback m_onFileDrop;
    CommandCallback m_onCommand;
};

} // namespace Koltzi
