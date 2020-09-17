#include "pch.h"
#include "Shlobj.h"
#include "Version.h"
#include "Richedit.h"
#include <fstream>

// Support undo & step during playback
// Display current (and previous) steps during playback
// Support undo during recording
//   This means I need to filter "None" while recording, not after
// Support overwriting steps during playback

constexpr WORD SET_RECORD_MODE   = 0x401;
constexpr WORD SET_PLAYBACK_MODE = 0x402;
constexpr WORD SET_NOP_MODE      = 0x403;
constexpr WORD READ_FROM_FILE    = 0x404;
constexpr WORD WRITE_TO_FILE     = 0x405;
constexpr WORD GET_BUFFER_SIZE   = 0x406;
constexpr WORD ATTACH_MEMORY     = 0x407;
constexpr WORD RESET_PLAYHEAD    = 0x408;
constexpr WORD PLAY_ONE_FRAME    = 0x409;
constexpr WORD BACK_ONE_FRAME    = 0x40A;

// From Direction.cs
enum Direction : byte {
    North,
    South,
    West,
    East,
    NorthEast,
    SouthWest,
    NorthWest,
    SouthEast,
    None,
    Down,
    Up,
};

std::shared_ptr<Memory> g_memory;
__int64 g_buffer = 0;
HWND g_bufferSize;
HWND g_currentState;
HWND g_instructionDisplay;

bool TryAttachMemory() {
    g_memory = Memory::Create(L"Sausage.exe");
    if (!g_memory) return false;

    __int64 firstLine = 0, nextLine = 0;
    // Targeting Game.Playerinputstring (C#)
    g_memory->AddSigScan("BE 03000000 48 8B C6", [&firstLine, &nextLine](__int64 address, const std::vector<byte>& data) {
        firstLine = address + 5;
        nextLine = address + 20;
    });

    size_t notFound = g_memory->ExecuteSigScans();
    if (notFound > 0) return false;

    size_t bufferSize = 0x100000;
    g_buffer = g_memory->AllocateBuffer(bufferSize);
    g_memory->WriteData<byte>(g_buffer, {LONG_TO_BYTES(8), 0x02 /* NOP mode */});

    g_memory->Intercept(firstLine, nextLine, {
        // direction is in sil (single byte, but the register is technically a long because C#)
        0x50,                                                   // push rax
        0x53,                                                   // push rbx
        0x51,                                                   // push rcx
        0xA0, LONG_TO_BYTES(g_buffer+8),                        // mov al, [buffer+8]   ; al = operation mode
        IF_LT(0x3C, 0x10),                                      // cmp al, 2            ; If we're in an active mode
        THEN(                                                   //
            0x48, 0xBB, LONG_TO_BYTES(g_buffer),                // mov rbx, buffer
            0x48, 0x8B, 0x0B,                                   // mov rcx, [rbx]       ; rcx = current buffer size
            0x48, 0xFF, 0xC1,                                   // inc rcx              ; rcx = new buffer size
            IF_LT(0x48, 0x81, 0xF9, INT_TO_BYTES(bufferSize)),  // cmp rcx, bufferSize  ; If this would not cause the buffer to overflow
            THEN(                                               //
                                                                //
                IF_EQ(0x3C, 0),                                 // cmp al, 0            ; If we're in recording mode
                THEN(                                           //
                    IF_NE(0x40, 0x80, 0xFE, 0x08),              // cmp sil, 0x08        ; If we took an action (dir != None)
                    THEN(                                       //
                        0x48, 0x89, 0x0B,                       // mov [rbx], rcx       ; Write the new buffer size
                        0x48, 0x01, 0xCB,                       // add rbx, rcx         ; rbx = first open buffer slot
                        0x40, 0x88, 0x33                        // mov [rbx], sil       ; Copy input from the game into buffer
                    )                                           //
                ),                                              //
                                                                //
                IF_EQ(0x3C, 1),                                 // cmp al, 1            ; If we're in playback mode
                THEN(                                           //
                    0x48, 0x89, 0x0B,                           // mov [rbx], rcx       ; Write the new buffer size
                    0x48, 0x01, 0xCB,                           // add rbx, rcx         ; rbx = first open buffer slot
                    0x40, 0x8A, 0x33                            // mov sil, [rbx]       ; Copy input from buffer to the game
                ),                                              //
                                                                //
                IF_EQ(0x3C, 2),                                 // cmp al, 2            ; Single step (in playback mode)
                THEN(                                           //
                    0x48, 0x89, 0x0B,                           // mov [rbx], rcx       ; Write the new buffer size
                    0xC6, 0x43, 0x08, 0x02,                     // mov [rbx+8], 0x2     ; Change mode to nop
                    0x48, 0x01, 0xCB,                           // add rbx, rcx         ; rbx = first open buffer slot
                    0x40, 0x8A, 0x33                            // mov sil, [rbx]       ; Copy input from buffer to the game
                )                                               //
            )                                                   //
        ),                                                      //
        0x59,                                                   // pop rcx
        0x5B,                                                   // pop rbx
        0x58,                                                   // pop rax
    });
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TIMER:
            switch (wParam) {
                case ATTACH_MEMORY:
                    if (TryAttachMemory()) {
                        KillTimer(hwnd, ATTACH_MEMORY);
                    }
                    break;
                case GET_BUFFER_SIZE:
                    __int64 bufferSize = g_memory->ReadData<__int64>(g_buffer, 1)[0];
                    std::vector<byte> bufferData = g_memory->ReadData<byte>(g_buffer, bufferSize);
                    __int64 actualBufferSize = 0;
                    for (size_t i=9; i<bufferData.size(); i++) {
                        if ((Direction)bufferData[i] != None) actualBufferSize++;
                    }
                    SetWindowTextW(g_bufferSize, (L"Recorded inputs: " + std::to_wstring(actualBufferSize)).c_str());
                    break;
            }
            break;
        case WM_DESTROY:
            g_memory = nullptr;
            PostQuitMessage(0);
            return 0;
        case WM_CTLCOLORSTATIC:
            // Get rid of the gross gray background. https://stackoverflow.com/a/4495814
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case SET_RECORD_MODE:
                    g_memory->WriteData<byte>(g_buffer+8, {0x00});
                    SetTimer(hwnd, GET_BUFFER_SIZE, 1000, NULL); // Start watching the buffer
                    SetWindowTextW(g_currentState, L"Recording inputs");
                    break;
                case SET_PLAYBACK_MODE:
                    g_memory->WriteData<byte>(g_buffer+8, {0x01});
                    KillTimer(hwnd, GET_BUFFER_SIZE);
                    SetWindowTextW(g_currentState, L"Playing inputs");
                    break;
                case PLAY_ONE_FRAME:
                    g_memory->WriteData<byte>(g_buffer+8, {0x02});
                    KillTimer(hwnd, GET_BUFFER_SIZE);
                    SetWindowTextW(g_currentState, L"Stepped once");
                    break;
                case BACK_ONE_FRAME:
                    g_memory->WriteData<byte>(g_buffer+8, {0x10});
                    // This needs to hook up to 'undo, but only once'
                    break;
                case SET_NOP_MODE:
                    g_memory->WriteData<byte>(g_buffer+8, {0x50});
                    KillTimer(hwnd, GET_BUFFER_SIZE);
                    SetWindowTextW(g_currentState, L"(Doing nothing)");
                    break;
                case RESET_PLAYHEAD:
                    g_memory->WriteData<__int64>(g_buffer, {0x08});
                    break;
                case READ_FROM_FILE:
                    {
                        std::vector<Direction> buffer;
                        std::ifstream file("test.dem");
                        for (std::string dir; std::getline(file, dir);) {
                            if (dir == "North") buffer.push_back(North);
                            if (dir == "South") buffer.push_back(South);
                            if (dir == "East")  buffer.push_back(East);
                            if (dir == "West")  buffer.push_back(West);
                            if (dir == "None")  buffer.push_back(None);
                        }
                        SendMessage(hwnd, RESET_PLAYHEAD, NULL, NULL);
                        g_memory->WriteData<Direction>(g_buffer + 9, buffer);
                    }
                    break;
                case WRITE_TO_FILE:
                    {
                        __int64 currentBufferSize = g_memory->ReadData<__int64>(g_buffer, 1)[0];
                        std::vector<byte> buffer = g_memory->ReadData<byte>(g_buffer, currentBufferSize);
                        std::ofstream file("test.dem");
                        for (size_t i=9; i<buffer.size(); i++) {
                            Direction dir = static_cast<Direction>(buffer[i]);
                            if (dir == North) file << "North\n";
                            if (dir == South) file << "South\n";
                            if (dir == East)  file << "East\n";
                            if (dir == West)  file << "West\n";
                        }
                        file.close();
                    }
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

    CreateButton(hwnd, x, y, 200, L"Start recording inputs", SET_RECORD_MODE);
    CreateButton(hwnd, x, y, 200, L"Stop doing anything", SET_NOP_MODE);
    CreateButton(hwnd, x, y, 200, L"Reset the playhead", RESET_PLAYHEAD);

    g_currentState = CreateLabel(hwnd, x, y, 200, 16, L"(Doing nothing)");

    // Column 2
    x = 300;
    y = 10;

    CreateButton(hwnd, x, y, 70, L"Load", WRITE_TO_FILE);
    y -= 30;
    CreateButton(hwnd, x + 80, y, 70, L"Save", READ_FROM_FILE);

    CreateButton(hwnd, x, y, 40, L"<", BACK_ONE_FRAME);
    y -= 30;
    CreateButton(hwnd, x + 40, y, 70, L"Play", SET_PLAYBACK_MODE);
    y -= 30;
    CreateButton(hwnd, x + 110, y, 40, L">", PLAY_ONE_FRAME);

    g_instructionDisplay = CreateLabel(hwnd, x, y, 100, 300, L"asdfasdfasdf\na\nb\nc\nd\ne\nf\ng\nh\n");
    y += 300;
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
