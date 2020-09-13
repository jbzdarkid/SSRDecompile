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

    std::shared_ptr<Memory> memory;
    while (!memory) {
        memory = Memory::Create(L"Sausage.exe");
        ::Sleep(1000);
    }
    size_t bufferSize = 0x1000;
    __int64 buffer = memory->AllocateBuffer(bufferSize, {0x07});

    memory->AddSigScan("48 83 EC 40 48 89 78 10", [memory, buffer, bufferSize](__int64 offset, int index, const std::vector<byte>& data){
        memory->Intercept(offset + index + 15, offset + index + 29, {
            IF(0x41, 0x80, 0xF8, 0x77, 0x75),                        // cmp dl, 'w',        ; If dl == 'w'
            THEN(0x41, 0xB0, 0x73),                                  // mov r8b, 's'        ;

            // 0x81, 0xFA, INT_TO_BYTES_LE(WM_KEYDOWN),       // cmp edx, 0x100       ;
            // 0x75, 0x23,                                    // jne end              ; If the message is not WM_KEYDOWN, do nothing.

            0x48, 0xBB, LONG_TO_BYTES_LE(buffer),                    // mov rbx, buffer      ; 
            0x4C, 0x8B, 0x33,                                        // mov r14, [rbx]       ; R14 = buffer size
            IF(0x49, 0x81, 0xFE, INT_TO_BYTES_LE(bufferSize), 0x73), // cmp r14, bufferSize  ; If the buffer is not full
            THEN(
                0x49, 0x83, 0xC6, 0x01,                              // add r14, 1           ; R14 = new buffer size (entry size = 1 byte)
                0x4C, 0x89, 0x33,                                    // mov [rbx], r14       ; Increment the 'next empty slot'
                0x49, 0x01, 0xDE,                                    // add r14, rbx         ; R14 = first empty buffer slot
                0x45, 0x88, 0x06                                     // mov [r14], r8b       ; Write the input into the empty slot
            )
        });
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
