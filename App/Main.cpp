#include "pch.h"

#include "Shlobj.h"
#include "Version.h"
#include "Richedit.h"
#include "shellapi.h"

#include "Memory.h"
#include "InputBuffer.h"

constexpr WORD SET_PLAYBACK_MODE = 0x402;
constexpr WORD READ_FROM_FILE    = 0x403;
constexpr WORD WRITE_TO_FILE     = 0x404;
constexpr WORD ATTACH_MEMORY     = 0x405;
constexpr WORD RESET_PLAYHEAD    = 0x406;
constexpr WORD PLAY_ONE_FRAME    = 0x407;
constexpr WORD BACK_ONE_FRAME    = 0x408;
constexpr WORD UPDATE_DISPLAY    = 0x409;
constexpr WORD LAUNCH_GAME       = 0x40A;
constexpr WORD GOTO_NEXT_DEMO    = 0x40B;
constexpr WORD GOTO_PREV_DEMO    = 0x40C;
constexpr WORD CREATE_DEMO       = 0x40D;
constexpr WORD WRITE_NONE        = 0x40E;

std::shared_ptr<Memory> g_memory;
std::shared_ptr<InputBuffer> g_inputBuffer;
HWND g_instructionDisplay;
HWND g_playButton;
HWND g_demoName;
HWND g_levelName;

std::vector<std::tuple<std::wstring, int, int, int, std::wstring>> demoNames = {
    {L"1-0.dem",      -4,  -1, -1, L"Overworld Start"},
    {L"1-1.dem",       4,  -2, -1, L"Lachrymose Head"},
    {L"1-2.dem",       1,  -2, -1, L"Southjaunt"},
    {L"1-3.dem",       1,  -3, -1, L"Infant's Break"},
    {L"1-4.dem",       2,  -9, -1, L"Comely Hearth"},
    {L"1-5.dem",       2, -11, -1, L"Little Fire"},
    {L"1-6.dem",       6, -12, -1, L"Eastreach"},
    {L"1-7.dem",       1, -16, -1, L"Bay's Neck"},
    {L"1-8.dem",       2, -17, -1, L"Burning Wharf"},
    {L"1-9.dem",      -5, -10, -1, L"Happy Pool"},
    {L"1-10.dem",     -7,  -9, -1, L"Maiden's Walk"},
    {L"1-11.dem",     -8,  -5, -1, L"Fiery Jut"},
    {L"1-12.dem",    -14,  -9, -1, L"Merchant's Elegy"},
    {L"1-13.dem",    -15,  -9, -1, L"Seafinger"},
    {L"1-14.dem",    -16, -15, -1, L"The Clover"},
    {L"1-15.dem",     -9, -14, -1, L"Inlet Shore"},
    {L"1-16.dem",    -10, -20, -1, L"The Anchorage"},
    {L"1-final.dem", -10, -20, -1, L"Overworld Sausage 1"},

    {L"2-1.dem",     -10, -20, -1, L"Emerson Jetty"},
    {L"2-2.dem",       0,   0,  0, L"Sad Farm"},
    {L"2-3.dem",       0,   0,  0, L"Cove"},
    {L"2-4.dem",       0,   0,  0, L"The Great Tower"},
    {L"2-5.dem",       0,   0,  0, L"The Paddock"},
    {L"2-6.dem",       0,   0,  0, L"Beautiful Horizon"},
    {L"2-7.dem",       0,   0,  0, L"Barrow Set"},
    {L"2-8.dem",       0,   0,  0, L"Rough Field"},
    {L"2-9.dem",       0,   0,  0, L"Fallow Earth"},
    {L"2-10.dem",      0,   0,  0, L"Twisty Farm"},
    {L"2-final.dem", -10, -20, -1, L"Overworld Sausage 2"},

    {L"3-1.dem",       0,   0,  0, L"Cold Jag"},
    {L"3-2.dem",       0,   0,  0, L"Cold Finger"},
    {L"3-3.dem",       0,   0,  0, L"Cold Escarpment"},
    {L"3-4.dem",       0,   0,  0, L"Cold Frustration"},
    {L"3-5.dem",       0,   0,  0, L"Cold Trail"},
    {L"3-6.dem",       0,   0,  0, L"Cold Cliff"},
    {L"3-7.dem",       0,   0,  0, L"Cold Pit"},
    {L"3-8.dem",       0,   0,  0, L"Cold Plateau"},
    {L"3-9.dem",       0,   0,  0, L"Cold Head"},
    {L"3-10.dem",      0,   0,  0, L"Cold Ladder"},
    {L"3-11.dem",      0,   0,  0, L"Cold Sausage"},
    {L"3-12.dem",      0,   0,  0, L"Cold Terrace"},
    {L"3-13.dem",      0,   0,  0, L"Cold Horizon"},
    {L"3-14.dem",      0,   0,  0, L"Cold Gate"},
    {L"3-final.dem", -10, -20, -1, L"Overworld Sausage 3"},

    {L"4-1.dem",       0,   0,  0, L"Toad's Folly"},
    {L"4-2.dem",       0,   0,  0, L"Wretch's Retreat"},
    {L"4-3.dem",       0,   0,  0, L"Sludge Coast"},
    {L"4-4.dem",       0,   0,  0, L"Crunchy Leaves"},
    {L"4-5.dem",       0,   0,  0, L"Gator Paddock"},
    {L"4-6.dem",       0,   0,  0, L"Foul Fen"},
    {L"4-final.dem", -10, -20, -1, L"Overworld Sausage 4"},

    {L"5-1.dem",       0,   0,  0, L"The Gorge"},
    {L"5-2.dem",       0,   0,  0, L"Widow's Finger"},
    {L"5-3.dem",       0,   0,  0, L"Skeleton"},
    {L"5-4.dem",       0,   0,  0, L"Open Baths"},
    {L"5-5.dem",       0,   0,  0, L"Slope View"},
    {L"5-6.dem",       0,   0,  0, L"Land's End"},
    {L"5-7.dem",       0,   0,  0, L"Crater"},
    {L"5-8.dem",       0,   0,  0, L"Pressure Points"},
    {L"5-9.dem",       0,   0,  0, L"Drumlin"},
    {L"5-10.dem",      0,   0,  0, L"Tarry Ridge"},
    {L"5-11.dem",      0,   0,  0, L"Rough View"},
    {L"5-12.dem",      0,   0,  0, L"Baby Rock"},

    {L"6-1.dem",       0,   0,  0, L"Dead End"},
    {L"6-2.dem",       0,   0,  0, L"Sea Dragon"},
    {L"6-3.dem",       0,   0,  0, L"Folklore"},
    {L"6-4.dem",       0,   0,  0, L"The Decay"},
    {L"6-5.dem",       0,   0,  0, L"The Splitting Bough"},
    {L"6-6.dem",       0,   0,  0, L"Captive Hydra"},
    {L"6-7.dem",       0,   0,  0, L"Rattlesnakes"},
    {L"6-8.dem",       0,   0,  0, L"Sty"},
    {L"6-9.dem",       0,   0,  0, L"Split Face"},
    {L"6-10.dem",      0,   0,  0, L"Four-faced Liar"},
    {L"6-11.dem",      0,   0,  0, L"Suspension Bridge"},
    {L"6-12.dem",      0,   0,  0, L"Curious Dragons"},
    {L"6-13.dem",      0,   0,  0, L"Ancient Dam"},
    {L"6-14.dem",      0,   0,  0, L"Apex"},

    {L"7-1.dem",       0,   0,  0, L"The Stone Tree"},
    {L"7-2.dem",       0,   0,  0, L"The Backbone"},
    {L"7-3.dem",       0,   0,  0, L"Baby Swan"},
    {L"7-4.dem",       0,   0,  0, L"Shy Dragon"},
    {L"7-5.dem",       0,   0,  0, L"Lovers' Sadness"},
    {L"7-6.dem",       0,   0,  0, L"Dragonclaw"},
    {L"7-7.dem",       0,   0,  0, L"Obscene Gesture"},
    {L"7-8.dem",       0,   0,  0, L"The Nursery"},
    {L"7-9.dem",       0,   0,  0, L"Canal"},
    {L"7-10.dem",      0,   0,  0, L"Mommy Swan"},
    {L"7-11.dem",      0,   0,  0, L"Loft of the Spirit"},
    {L"7-12.dem",      0,   0,  0, L"Wobblecliff"},
    {L"7-13.dem",      0,   0,  0, L"Plateau Ferry"},
    {L"7-14.dem",      0,   0,  0, L"God Pillar"},
    {L"7-final.dem",   0,   0,  0, L"Final cleanup"},
};

std::wstring GetWindowString(HWND hwnd) {
    SetLastError(0); // GetWindowTextLength does not clear LastError.
    int length = GetWindowTextLengthW(hwnd);
    std::wstring text(length, L'\0');
    length = GetWindowTextW(hwnd, text.data(), static_cast<int>(text.size() + 1)); // Length includes the null terminator
    text.resize(length);
    return text;
}

void LoadRelativeDemo(int offset) {
    std::wstring expectedName = GetWindowString(g_demoName);
    for (size_t i=0; i<demoNames.size(); i++) {
        const std::wstring& name = std::get<0>(demoNames[i]);
        if (name == expectedName) {
            if (i + offset < 0) break;
            if (i + offset >= demoNames.size()) break;
            const auto& [newName, x, y, z, levelName] = demoNames[i + offset];
            SetWindowTextW(g_demoName, newName.c_str());
            SetWindowTextW(g_levelName, levelName.c_str());
            SetWindowTextW(g_playButton, L"Play");
            // g_inputBuffer->SetPlayerPosition({x, y, z});
            g_inputBuffer->ReadFromFile(newName);
            return;
        }
    }

    // Not a known demo, just load it (don't move the player)
    if (offset == 0) {
        g_inputBuffer->ReadFromFile(expectedName);
        SetWindowTextW(g_levelName, NULL);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_TIMER:
            switch (wParam) {
                case ATTACH_MEMORY:
                    g_memory = Memory::Create(L"Sausage.exe");
                    g_inputBuffer = InputBuffer::Create(g_memory);
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
                case GOTO_PREV_DEMO:
                    LoadRelativeDemo(-1);
                    break;
                case READ_FROM_FILE:
                    LoadRelativeDemo(0);
                    break;
                case GOTO_NEXT_DEMO:
                    LoadRelativeDemo(+1);
                    break;
                case WRITE_TO_FILE:
                    g_inputBuffer->WriteToFile(GetWindowString(g_demoName));
                    break;
                case CREATE_DEMO:
                    {
                        std::ofstream all("all.dem");
                        std::string line;
                        for (const auto& [demoName, _1, _2, _3, _4] : demoNames) {
                            std::ifstream demo(demoName);
                            while (std::getline(demo, line)) all << line << '\n';
                        }
                        all.close();
                    }
                    break;
                case WRITE_NONE:
                    g_inputBuffer->WriteNone();
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

    CreateButton(hwnd, x, y, 200, L"Reset the playhead", RESET_PLAYHEAD);
    CreateButton(hwnd, x, y, 200, L"Launch game", LAUNCH_GAME);
    CreateButton(hwnd, x, y, 200, L"Create master demo", CREATE_DEMO);
    CreateButton(hwnd, x, y, 200, L"Write empty instruction", WRITE_NONE);

    // Column 2
    x = 300;
    y = 10;

    CreateButton(hwnd, x, y, 70, L"Load", READ_FROM_FILE);
    y -= 30;
    CreateButton(hwnd, x + 80, y, 70, L"Save", WRITE_TO_FILE);

    g_demoName = CreateText(hwnd, x, y, 150, L"4-1.dem");
    g_levelName = CreateLabel(hwnd, x, y, 150, 16, L"");
    y += 20;

    CreateButton(hwnd, x, y, 20, L"<<", GOTO_PREV_DEMO);
    y -= 30;
    CreateButton(hwnd, x + 20, y, 20, L"<", BACK_ONE_FRAME);
    y -= 30;
    g_playButton = CreateButton(hwnd, x + 40, y, 70, L"Play", SET_PLAYBACK_MODE);
    y -= 30;
    CreateButton(hwnd, x + 110, y, 20, L">", PLAY_ONE_FRAME);
    y -= 30;
    CreateButton(hwnd, x + 130, y, 20, L">>", GOTO_NEXT_DEMO);

    g_instructionDisplay = CreateLabel(hwnd, x, y, 150, 500);
    SetTimer(hwnd, UPDATE_DISPLAY, 100, NULL);
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
