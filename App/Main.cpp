#include "pch.h"
#include "Shlobj.h"
#include "Version.h"

// - CreateRemoteThread + VirtualAllocEx allows me to *run* code in another process. This seems... powerful!

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return hr;
    WNDCLASS wndClass = {
        CS_HREDRAW | CS_VREDRAW,
        WndProc,
        0,
        0,
        hInstance,
        NULL,
        NULL,
        NULL,
        WINDOW_CLASS,
        WINDOW_CLASS,
    };
    RegisterClass(&wndClass);

    auto memory = std::make_shared<Memory>(L"Sausage.exe");
    memory->AddSigScan({0x4C, 0x8B, 0x32, 0x45, 0x32, 0xED}, [memory](__int64 offset, int index, const std::vector<byte>& data){
        memory->Intercept(offset + index, offset + index + 13, {0xCC, 0xCC, 0xCC});
    });
    size_t notFound = memory->ExecuteSigScans();
    assert(notFound == 0);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return (int) msg.wParam;
}
