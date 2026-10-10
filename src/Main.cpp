#include "Common.h"
#include "App/Application.h"
#include <iostream>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        return 1;
    }

    // Set Per-Monitor V2 DPI awareness for crystal-sharp Direct2D rendering
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc >= 2) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!hOut || hOut == INVALID_HANDLE_VALUE) {
            if (AttachConsole(ATTACH_PARENT_PROCESS)) {
                FILE* fpOut = nullptr;
                FILE* fpErr = nullptr;
                freopen_s(&fpOut, "CONOUT$", "w", stdout);
                freopen_s(&fpErr, "CONOUT$", "w", stderr);
                std::ios::sync_with_stdio(true);
            }
        }
    }

    Koltzi::Application app;
    int result = app.Run(argc, argv);

    if (argv) {
        LocalFree(argv);
    }

    CoUninitialize();
    return result;
}

// Fallback main for console / CLI invocations
int main(int argc, char* argv[]) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    return wWinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineW(), SW_SHOW);
}
