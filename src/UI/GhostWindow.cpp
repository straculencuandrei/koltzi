#include "GhostWindow.h"

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
    wc.lpszClassName = L"KoltziGhostWindowClass";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    // Initial position: Bottom right of screen workarea
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    m_posX = workArea.right - m_width - 40;
    m_posY = workArea.bottom - m_height - 60;
    if (m_posX < 20) m_posX = 20;
    if (m_posY < 20) m_posY = 20;

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_ACCEPTFILES | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Koltzi - Ghost Triage Companion",
        WS_POPUP,
        m_posX, m_posY, m_width, m_height,
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
    return SUCCEEDED(hr);
}

bool GhostWindow::CreateDeviceResources() {
    if (!m_d2dFactory || !m_dwriteFactory) return false;

    HDC hdcScreen = GetDC(nullptr);
    m_hMemDC = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_width;
    bmi.bmiHeader.biHeight = -m_height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_hMemDC, &bmi, DIB_RGB_COLORS, &m_pBits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);

    if (!m_hBitmap) return false;
    m_hOldBitmap = static_cast<HBITMAP>(SelectObject(m_hMemDC, m_hBitmap));

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0, 0,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT
    );

    HRESULT hr = m_d2dFactory->CreateDCRenderTarget(&props, &m_dcRenderTarget);
    if (FAILED(hr)) return false;

    m_ghostRenderer.Initialize(m_dcRenderTarget, m_dwriteFactory);
    m_speechBubble.Initialize(m_dcRenderTarget, m_dwriteFactory);

    return true;
}

void GhostWindow::DiscardDeviceResources() {
    m_ghostRenderer.DiscardDeviceResources();
    m_speechBubble.DiscardDeviceResources();
    SafeRelease(m_dcRenderTarget);

    if (m_hMemDC) {
        if (m_hOldBitmap) {
            SelectObject(m_hMemDC, m_hOldBitmap);
            m_hOldBitmap = NULL;
        }
        DeleteDC(m_hMemDC);
        m_hMemDC = NULL;
    }

    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = NULL;
    }
}

void GhostWindow::SetReport(std::shared_ptr<TriageReport> report) {
    m_currentReport = report;
    if (report) {
        SetMood(report->mood);
        SetDialogue(report->personalityDialogue, false);
    }
}

void GhostWindow::SetMood(GhostMood mood) {
    m_currentMood = mood;
}

void GhostWindow::SetDialogue(const std::string& text, bool immediate) {
    m_speechBubble.SetDialogue(text, immediate);
}

void GhostWindow::ToggleHUD() {
    m_speechBubble.ToggleExpandedDetails();
}

void GhostWindow::Update(float dt) {
    m_ghostRenderer.Update(dt, m_currentMood);
    m_speechBubble.Update(dt);
}

void GhostWindow::Render() {
    if (!m_dcRenderTarget || !m_hMemDC || !m_hwnd) return;

    RECT rect = { 0, 0, m_width, m_height };
    HRESULT hr = m_dcRenderTarget->BindDC(m_hMemDC, &rect);
    if (FAILED(hr)) {
        DiscardDeviceResources();
        CreateDeviceResources();
        return;
    }

    m_dcRenderTarget->BeginDraw();
    m_dcRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent

    // Ghost center coordinates: (100, 160)
    m_ghostRenderer.Render(m_dcRenderTarget, 100.0f, 160.0f);

    // Speech bubble & HUD
    m_speechBubble.Render(m_dcRenderTarget, m_currentReport.get(), m_currentMood);

    hr = m_dcRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        CreateDeviceResources();
        return;
    }

    // Update Windows Layered Window with per-pixel alpha
    HDC hdcScreen = GetDC(nullptr);
    POINT ptSrc = { 0, 0 };
    POINT ptPos = { m_posX, m_posY };
    SIZE sizeWnd = { m_width, m_height };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    UpdateLayeredWindow(
        m_hwnd,
        hdcScreen,
        &ptPos,
        &sizeWnd,
        m_hMemDC,
        &ptSrc,
        0,
        &blend,
        ULW_ALPHA
    );

    ReleaseDC(nullptr, hdcScreen);
}

void GhostWindow::ShowContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_SCAN_FILE, L"Analyze PE Binary File...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_CLEAN, L"Test Profile: Clean PE (Standard Imports)");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_PACKED, L"Test Profile: High Entropy / Packed Code");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_SYSCALL_PEB, L"Test Profile: Direct Syscalls + PEB Hashing");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_CRED_STEALER, L"Test Profile: Credential Scraping Artifacts");
    AppendMenuW(hMenu, MF_STRING, IDM_SAMPLE_INJECTION, L"Test Profile: Process Injection Chain");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE_HUD, m_speechBubble.IsExpanded() ? L"Collapse Technical HUD" : L"Expand Technical HUD");
    AppendMenuW(hMenu, MF_STRING, IDM_RESET, L"Reset Mascot to Idle");
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
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<GhostWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
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
            Render();
            return 0;
        }
        break;

    case WM_DROPFILES: {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        if (fileCount > 0) {
            wchar_t filePath[MAX_PATH] = { 0 };
            if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH) > 0) {
                if (m_onFileDrop) {
                    m_onFileDrop(filePath);
                }
            }
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mouseX = GET_X_LPARAM(lParam);
        int mouseY = GET_Y_LPARAM(lParam);

        // If clicked on speech bubble toggle area, toggle HUD
        if (m_speechBubble.HitTest(static_cast<float>(mouseX), static_cast<float>(mouseY))) {
            m_speechBubble.ToggleExpandedDetails();
            Render();
            return 0;
        }

        // Otherwise begin window dragging
        m_isDragging = true;
        SetCapture(hwnd);
        GetCursorPos(&m_dragStartPos);
        m_dragStartWindowPos.x = m_posX;
        m_dragStartWindowPos.y = m_posY;
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (m_isDragging) {
            POINT pt;
            GetCursorPos(&pt);
            int dx = pt.x - m_dragStartPos.x;
            int dy = pt.y - m_dragStartPos.y;
            m_posX = m_dragStartWindowPos.x + dx;
            m_posY = m_dragStartWindowPos.y + dy;
            Render();
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_isDragging) {
            m_isDragging = false;
            ReleaseCapture();
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt;
        GetCursorPos(&pt);
        ShowContextMenu(pt.x, pt.y);
        return 0;
    }

    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        } else if (wParam == VK_TAB || wParam == VK_SPACE) {
            m_speechBubble.ToggleExpandedDetails();
            Render();
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace Koltzi
