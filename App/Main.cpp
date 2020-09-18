#include "pch.h"

#include "Shlobj.h"
#include "Version.h"
#include "Richedit.h"
#include "shellapi.h"

#include "Memory.h"
#include "InputBuffer.h"

constexpr WORD SET_RECORD_MODE   = 0x401;
constexpr WORD SET_PLAYBACK_MODE = 0x402;
constexpr WORD READ_FROM_FILE    = 0x403;
constexpr WORD WRITE_TO_FILE     = 0x404;
constexpr WORD ATTACH_MEMORY     = 0x405;
constexpr WORD RESET_PLAYHEAD    = 0x406;
constexpr WORD PLAY_ONE_FRAME    = 0x407;
constexpr WORD BACK_ONE_FRAME    = 0x408;
constexpr WORD UPDATE_DISPLAY    = 0x409;
constexpr WORD LAUNCH_GAME       = 0x40A;

std::shared_ptr<Memory> g_memory;
std::shared_ptr<InputBuffer> g_inputBuffer;
HWND g_bufferSize;
HWND g_instructionDisplay;
HWND g_recordButton;
HWND g_playButton;
HWND g_demoName;

std::wstring GetWindowString(HWND hwnd) {
    SetLastError(0); // GetWindowTextLength does not clear LastError.
    int length = GetWindowTextLengthW(hwnd);
    std::wstring text(length, L'\0');
    length = GetWindowTextW(hwnd, text.data(), static_cast<int>(text.size() + 1)); // Length includes the null terminator
    text.resize(length);
    return text;
}

std::vector<int> GetPosition() {
    if (!g_memory) return {0, 0, 0};
    return g_memory->ReadData<int>({0x1547668, 0x58, 0x98, 0x40, 0x18, 0x100, 0xD8}, 3);
}

void SetPosition(const std::vector<int>& position) {
    if (!g_memory) return;
    g_memory->WriteData<int>({0x1547668, 0x58, 0x98, 0x40, 0x18, 0x100, 0xD8}, position);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TIMER:
            switch (wParam) {
                case ATTACH_MEMORY:
                    g_inputBuffer = InputBuffer::Create();
                    if (!g_inputBuffer) break;
                    KillTimer(hwnd, ATTACH_MEMORY);
                    break;
                case UPDATE_DISPLAY:
                    if (g_inputBuffer) {
                        SetWindowTextA(g_instructionDisplay, g_inputBuffer->GetDisplayText().c_str());
                    }
                    break;
            }
            break;
        case WM_DESTROY:
            g_inputBuffer = nullptr;
            PostQuitMessage(0);
            return 0;
        case WM_CTLCOLORSTATIC:
            // Get rid of the gross gray background. https://stackoverflow.com/a/4495814
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == LAUNCH_GAME) {
                ShellExecute(NULL, L"open", L"steam://rungameid/353540", NULL, NULL, SW_SHOWDEFAULT);
                break;
            }

            if (!g_inputBuffer) break;
            switch (LOWORD(wParam)) {
                case SET_RECORD_MODE:
                    if (g_inputBuffer->GetMode() == Recording) {
                        g_inputBuffer->SetMode(Nothing);
                        SetWindowTextW(g_recordButton, L"Start recording");
                    } else {
                        g_inputBuffer->SetMode(Recording);
                        SetWindowTextW(g_recordButton, L"Stop recording");
                    }
                    break;
                case SET_PLAYBACK_MODE:
                    if (g_inputBuffer->GetMode() == Playing) {
                        g_inputBuffer->SetMode(Recording);
                        SetWindowTextW(g_playButton, L"Play");
                    } else {
                        g_inputBuffer->SetMode(Playing);
                        SetWindowTextW(g_playButton, L"Pause");
                    }
                    break;
                case PLAY_ONE_FRAME:
                    g_inputBuffer->SetMode(ForwardStep);
                    break;
                case BACK_ONE_FRAME:
                    g_inputBuffer->SetMode(BackStep);
                    break;
                case RESET_PLAYHEAD:
                    g_inputBuffer->ResetPosition();
                    break;
                case READ_FROM_FILE:
                    g_inputBuffer->ReadFromFile(GetWindowString(g_demoName));
                    break;
                case WRITE_TO_FILE:
                    g_inputBuffer->WriteToFile(GetWindowString(g_demoName));
                    break;
            }
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

HWND CreateButton(HWND hwnd, int x, int& y, int width, LPCWSTR text, __int64 message, LPCWSTR hoverText = L"") {
    HWND button = CreateWindow(L"BUTTON", text,
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        x, y, width, 26,
        hwnd, (HMENU)message, NULL, NULL);
    y += 30;
    return button;
}

HWND CreateLabel(HWND hwnd, int x, int y, int width, int height, LPCWSTR text = L"") {
    return CreateWindow(L"STATIC", text,
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | SS_LEFT,
        x, y, width, height,
        hwnd, NULL, NULL, NULL);
}

HWND CreateText(HWND hwnd, int x, int& y, int width, LPCWSTR defaultText = L"", __int64 message = NULL) {
    HWND text = CreateWindow(MSFTEDIT_CLASS, defaultText,
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER,
        x, y, width, 26,
        hwnd, (HMENU)message, NULL, NULL);
    y += 30;
    return text;
}

void CreateComponents(HWND hwnd) {
    // Column 1
    int x = 10;
    int y = 10;

    g_recordButton = CreateButton(hwnd, x, y, 200, L"Start recording", SET_RECORD_MODE);
    CreateButton(hwnd, x, y, 200, L"Reset the playhead", RESET_PLAYHEAD);
    CreateButton(hwnd, x, y, 200, L"Launch game", LAUNCH_GAME);

    // Column 2
    x = 300;
    y = 10;

    CreateButton(hwnd, x, y, 70, L"Load", READ_FROM_FILE);
    y -= 30;
    CreateButton(hwnd, x + 80, y, 70, L"Save", WRITE_TO_FILE);

    g_demoName = CreateText(hwnd, x, y, 150, L"all.dem");

    CreateButton(hwnd, x, y, 20, L"<<", NULL);
    y -= 30;
    CreateButton(hwnd, x + 20, y, 20, L"<", BACK_ONE_FRAME);
    y -= 30;
    g_playButton = CreateButton(hwnd, x + 40, y, 70, L"Play", SET_PLAYBACK_MODE);
    y -= 30;
    CreateButton(hwnd, x + 110, y, 20, L">", PLAY_ONE_FRAME);
    y -= 30;
    CreateButton(hwnd, x + 130, y, 20, L">>", NULL);

    g_instructionDisplay = CreateLabel(hwnd, x, y, 150, 400);
    SetTimer(hwnd, UPDATE_DISPLAY, 100, NULL);

    y += 400;
    g_bufferSize = CreateLabel(hwnd, x, y, 200, 16, L"Recorded inputs: 0");

}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return hr;
    LoadLibrary(L"Msftedit.dll");

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

    RECT rect;
    GetClientRect(GetDesktopWindow(), &rect);
    HWND hwnd = CreateWindow(WINDOW_CLASS, PRODUCT_NAME,
        WS_SYSMENU | WS_MINIMIZEBOX,
        rect.right - 550, 200, 500, 500,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    SetTimer(hwnd, ATTACH_MEMORY, 1000, NULL);

    CreateComponents(hwnd);
    DebugUtils::version = VERSION_STR;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return (int) msg.wParam;


/*
    memory->AddSigScan("48 83 EC 40 48 89 78 10", [memory, buffer, bufferSize](__int64 offset, int index, const std::vector<byte>& data) {
        memory->Intercept(offset + index + 15, offset + index + 29, {
            IF_EQ(0x41, 0x80, 0xF8, 'w'),                          // cmp r8b, 'w',       ; If the input is 'w'
            THEN(0x41, 0xB0, 's'),                                 // mov r8b, 's'        ; Set the input to 's'
                                                                   //
            IF_EQ(0x81, 0xFA, INT_TO_BYTES(WM_KEYDOWN)),           // cmp edx, WM_KEYDOWN ; If the message is WM_KEYDOWN
            THEN(                                                  //
                0x48, 0xBB, LONG_TO_BYTES(buffer),                 // mov rbx, buffer
                0x4C, 0x8B, 0x33,                                  // mov r14, [rbx]      ; R14 = buffer size (stored in the first 8 buffer bytes)
                0x49, 0x83, 0xC6, 0x01,                            // add r14, 1          ; R14 = new buffer size (entry size = 1 byte)
                IF_LT(0x49, 0x81, 0xFE, INT_TO_BYTES(bufferSize)), // cmp r14, bufferSize ; If this won't cause the buffer to overflow
                THEN(                                              //
                    0x4C, 0x89, 0x33,                              // mov [rbx], r14      ; Increment the 'next empty slot'
                    0x49, 0x01, 0xDE,                              // add r14, rbx        ; R14 = first empty buffer slot
                    0x45, 0x88, 0x06                               // mov [r14], r8b      ; Write the input into the empty slot
                )
            )
        });
    });

    // Targeting scripting_method_invoke
    memory->AddSigScan("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F1 49 8B D8 49 8B C9", [memory, buffer, bufferSize](__int64 offset, int index, const std::vector<byte>& data) {
        memory->Intercept(offset + index + 11, offset + index + 27, {
            // Registers to work with: rax, rbx, rsi, rdi
            0x48, 0x8B, 0x42, 0x18,        // mov rax, [rdx + 0x18]  ; rax is the target string
            0xB3, 0x01,                    // mov bl, 0x01           ; bl is "does this string match"
            IF_NE(0x80, 0x38, 'U'),        // cmp [rax], 'U'
            THEN(0xB3, 0x00),              // mov bl, 0x00
            IF_NE(0x80, 0x78, 0x01, 'p'),  // cmp [rax+1], 'p'
            THEN(0xB3, 0x00),              // mov bl, 0x00
            IF_NE(0x80, 0x78, 0x02, 'd'),  // cmp [rax+2], 'd'
            THEN(0xB3, 0x00),              // mov bl, 0x00
            IF_NE(0x80, 0x78, 0x03, 'a'),  // cmp [rax+3], 'a'
            THEN(0xB3, 0x00),              // mov bl, 0x00
            IF_NE(0x80, 0x78, 0x04, 't'),  // cmp [rax+4], 't'
            THEN(0xB3, 0x00),              // mov bl, 0x00
            IF_NE(0x80, 0x78, 0x05, 'e'),  // cmp [rax+5], 'e'
            THEN(0xB3, 0x00),              // mov bl, 0x00
            IF_NE(0x80, 0x78, 0x06, '\0'), // cmp [rax+6], '\0'
            THEN(0xB3, 0x00),              // mov bl, 0x00
            0x48, 0x8B, 0x42, 0x08,        // mov rax, [rdx + 0x8]
            0x48, 0x8B, 0x40, 0x50,        // mov rax, [rdx + 0x50] ; Function class (i.e. *Game*.Update)
            IF_NE(0x80, 0x38, 'G'),        // cmp [rax], 'G'        ; InputManager_Base is our target (for now)
            THEN(0xB3, 0x00),              // mov bl, 0x00
        });
    });

    // Targeting scripting_method_invoke
    memory->AddSigScan("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F1 49 8B D8 49 8B C9", [memory, buffer, bufferSize](__int64 offset, int index, const std::vector<byte>& data) {
        memory->Intercept(offset + index + 11, offset + index + 27, {
            // Registers to work with: rax, rbx, rsi, rdi
            0x48, 0x8B, 0x42, 0x18,                             // mov rax, [rdx + 0x18]  ; rax is the target string
            0x48, 0x31, 0xDB,                                   // xor rbx, rbx           ; rbx will be the string length
            DO_WHILE(                                           //
                0x48, 0xFF, 0xC3,                               // inc rbx
                IF_EQ(0x80, 0x3C, 0x18, 0x00)                   // cmp [rax+rbx], 0
            ),                                                  //
            0x48, 0xFF, 0xC3,                                   // inc rbx                ; Add null terminator to string length
            0x48, 0xBE, LONG_TO_BYTES(buffer),                  // mov rsi, buffer
            0x48, 0x8B, 0x3E,                                   // mov rdi, [rsi]         ; rdi is now the current buffer size
            0x48, 0x01, 0xFB,                                   // add rbx, rdi           ; rbx is now the new buffer size
            IF_GE(0x48, 0x81, 0xFB, INT_TO_BYTES(bufferSize)),  // cmp rbx, bufferSize    ; If this would cause the buffer to overflow
            THEN(0x48, 0xC7, 0xC3, INT_TO_BYTES(8)),            // mov rbx, 8             ; reset the buffer
            0x48, 0x89, 0x1E,                                   // mov [rsi], rbx         ; Write the new buffer size
            0x48, 0x01, 0xF7,                                   // add rdi, rsi           ; rdi is now a pointer to the first open buffer slot
            DO_WHILE(                                           //
                0x40, 0x8A, 0x30,                               // mov sil, [rax]         ; sil is now the next character in the string
                0x40, 0x88, 0x37,                               // mov [rdi], sil         ; write the character to buffer
                0x48, 0xFF, 0xC0,                               // inc rax
                0x48, 0xFF, 0xC7,                               // inc rdi
                IF_EQ(0x40, 0x80, 0xFE, 0x00)                   // cmp sil, 0             ; check if we wrote the null terminator
            )
        });
    });
*/
}
