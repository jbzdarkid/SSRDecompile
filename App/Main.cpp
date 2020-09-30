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
constexpr WORD BAD_INPUT_BREAK   = 0x40F;

std::shared_ptr<Memory> g_memory;
std::shared_ptr<InputBuffer> g_inputBuffer;
HWND g_instructionDisplay, g_playButton, g_demoName, g_levelName;

std::vector<std::tuple<std::wstring, std::wstring>> demoNames = {
    {L"1-0.dem",     L"Overworld Start"},
    {L"1-1.dem",     L"Lachrymose Head"},
    {L"1-2.dem",     L"Southjaunt"},
    {L"1-3.dem",     L"Infant's Break"},
    {L"1-4.dem",     L"Comely Hearth"},
    {L"1-5.dem",     L"Little Fire"},
    {L"1-6.dem",     L"Eastreach"},
    {L"1-7.dem",     L"Bay's Neck"},
    {L"1-8.dem",     L"Burning Wharf"},
    {L"1-9.dem",     L"Happy Pool"},
    {L"1-10.dem",    L"Maiden's Walk"},
    {L"1-11.dem",    L"Fiery Jut"},
    {L"1-12.dem",    L"Merchant's Elegy"},
    {L"1-13.dem",    L"Seafinger"},
    {L"1-14.dem",    L"The Clover"},
    {L"1-15.dem",    L"Inlet Shore"},
    {L"1-16.dem",    L"The Anchorage"},
    {L"1-final.dem", L"Overworld Sausage 1"},

    {L"2-1.dem",     L"Emerson Jetty"},
    {L"2-2.dem",     L"Sad Farm"},
    {L"2-3.dem",     L"Cove"},
    {L"2-4.dem",     L"The Great Tower"},
    {L"2-5.dem",     L"The Paddock"},
    {L"2-6.dem",     L"Beautiful Horizon"},
    {L"2-7.dem",     L"Barrow Set"},
    {L"2-8.dem",     L"Rough Field"},
    {L"2-9.dem",     L"Fallow Earth"},
    {L"2-10.dem",    L"Twisty Farm"},
    {L"2-final.dem", L"Overworld Sausage 2"},

    {L"3-1.dem",     L"Cold Jag"},
    {L"3-2.dem",     L"Cold Finger"},
    {L"3-3.dem",     L"Cold Escarpment"},
    {L"3-4.dem",     L"Cold Frustration"},
    {L"3-5.dem",     L"Cold Trail"},
    {L"3-6.dem",     L"Cold Cliff"},
    {L"3-7.dem",     L"Cold Pit"},
    {L"3-8.dem",     L"Cold Plateau"},
    {L"3-9.dem",     L"Cold Head"},
    {L"3-10.dem",    L"Cold Ladder"},
    {L"3-11.dem",    L"Cold Sausage"},
    {L"3-12.dem",    L"Cold Terrace"},
    {L"3-13.dem",    L"Cold Horizon"},
    {L"3-14.dem",    L"Cold Gate"},
    {L"3-final.dem", L"Overworld Sausage 3"},

    {L"4-1.dem",     L"Toad's Folly"},
    {L"4-2.dem",     L"Wretch's Retreat"},
    {L"4-3.dem",     L"Sludge Coast"},
    {L"4-4.dem",     L"Foul Fen"},
    {L"4-5.dem",     L"Crunchy Leaves"},
    {L"4-6.dem",     L"Gator Paddock"},
    {L"4-final.dem", L"Overworld Sausage 4"},

    {L"5-1.dem",     L"The Gorge"},
    {L"5-2.dem",     L"Widow's Finger"},
    {L"5-3.dem",     L"Skeleton"},
    {L"5-4.dem",     L"Open Baths"},
    {L"5-5.dem",     L"Slope View"},
    {L"5-6.dem",     L"Land's End"},
    {L"5-7.dem",     L"Crater"},
    {L"5-8.dem",     L"Pressure Points"},
    {L"5-9.dem",     L"Drumlin"},
    {L"5-10.dem",    L"Tarry Ridge"},
    {L"5-11.dem",    L"Rough View"},
    {L"5-12.dem",    L"Baby Rock"},

    {L"6-1.dem",     L"Dead End"},
    {L"6-2.dem",     L"Sea Dragon"},
    {L"6-2.5.dem",   L"Folklore Setup"},
    {L"6-3.dem",     L"Folklore"},
    {L"6-3.5.dem",   L"The Decay Setup"},
    {L"6-4.dem",     L"The Decay"},
    {L"6-5.dem",     L"The Splitting Bough"},
    {L"6-6.dem",     L"Captive Hydra"},
    {L"6-7.dem",     L"Rattlesnakes"},
    {L"6-8.dem",     L"Sty"},
    {L"6-8.5.dem",   L"Split Face Setup"},
    {L"6-9.dem",     L"Split Face"},
    {L"6-9.5.dem",   L"Four-faced Liar Setup"},
    {L"6-10.dem",    L"Four-faced Liar"},
    {L"6-10.5.dem",  L"Suspension Bridge Setup"},
    {L"6-11.dem",    L"Suspension Bridge"},
    {L"6-11.5.dem",  L"Curious Dragons Setup"},
    {L"6-12.dem",    L"Curious Dragons"},
    {L"6-13.dem",    L"Ancient Dam"},
    {L"6-14.dem",    L"Apex"},
    {L"6-15.dem",    L"The Stone Tree"},
    {L"6-15.5.dem",  L"The Backbone Setup"},
    {L"6-16.dem",    L"The Backbone"},
    {L"6-17.dem",    L"Baby Swan"},
    {L"6-17.5.dem",  L"Shy Dragon Setup"},
    {L"6-18.dem",    L"Shy Dragon"},
    {L"6-18.5.dem",  L"Lovers' Sadness Setup"},
    {L"6-19.dem",    L"Lovers' Sadness"},
    {L"6-19.5.dem",  L"Dragonclaw Setup"},
    {L"6-20.dem",    L"Dragonclaw"},
    {L"6-20.5.dem",  L"Obscene Gesture Setup"},
    {L"6-21.dem",    L"Obscene Gesture"},
    {L"6-22.dem",    L"The Nursery"},
    {L"6-22.5.dem",  L"Canal Setup"},
    {L"6-23.dem",    L"Canal"},
    {L"6-23.5.dem",  L"Mommy Swan Setup"},
    {L"6-24.dem",    L"Mommy Swan"},
    {L"6-24.5.dem",  L"Loft of the Spirit Setup"},
    {L"6-25.dem",    L"Loft of the Spirit"},
    {L"6-25.5.dem",  L"Wobblecliff Setup"},
    {L"6-26.dem",    L"Wobblecliff"},
    {L"6-26.5.dem",  L"Plateau Ferry Setup"},
    {L"6-27.dem",    L"Plateau Ferry"}, // Needs some optimization
    {L"6-27.5.dem",  L"God Pillar Setup"},
    {L"6-28.dem",    L"God Pillar"}, // Needs optimization
    {L"6-final.dem", L"Final cleanup"}, // Needs optimization
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
            const auto& [newName, levelName] = demoNames[i + offset];
            SetWindowTextW(g_demoName, newName.c_str());
            SetWindowTextW(g_levelName, levelName.c_str());
            SetWindowTextW(g_playButton, L"Play");
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
#ifndef _DEBUG
                    SendMessage(hwnd, WM_COMMAND, CREATE_DEMO, NULL);
                    SendMessage(hwnd, WM_COMMAND, READ_FROM_FILE, NULL);
#endif
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
                        for (const auto& [demoName, _] : demoNames) {
                            std::ifstream demo(demoName);
                            while (std::getline(demo, line)) all << line << '\n';
                        }
                        all.close();
                    }
                    break;
                case WRITE_NONE:
                    g_inputBuffer->WriteNone();
                    break;
                case BAD_INPUT_BREAK:
                    if (g_inputBuffer->BreakOnBadInput()) {
                        CheckDlgButton(hwnd, BAD_INPUT_BREAK, true);
                    } else {
                        CheckDlgButton(hwnd, BAD_INPUT_BREAK, false);
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

HWND CreateCheckbox(HWND hwnd, int x, int& y, __int64 message) {
    HWND checkbox = CreateWindow(L"BUTTON", NULL,
        WS_VISIBLE | WS_CHILD | BS_CHECKBOX,
        x, y + 2, 12, 12,
        hwnd, (HMENU)message, NULL, NULL);
    y += 20;
    return checkbox;
}

void CreateComponents(HWND hwnd) {
    // Column 1
    int x = 10;
    int y = 10;

    CreateButton(hwnd, x, y, 200, L"Launch game", LAUNCH_GAME);
#ifdef _DEBUG
    CreateButton(hwnd, x, y, 200, L"Create master demo", CREATE_DEMO);
    CreateButton(hwnd, x, y, 200, L"Reset the playhead", RESET_PLAYHEAD);
    CreateButton(hwnd, x, y, 200, L"Write empty instruction", WRITE_NONE);
    CreateLabel(hwnd, x + 20, y, 185, 16, L"Break on bad inputs");
    CreateCheckbox(hwnd, x, y, BAD_INPUT_BREAK);
#endif

    // Column 2
    x = 300;
    y = 10;

#ifdef _DEBUG
    CreateButton(hwnd, x, y, 70, L"Load", READ_FROM_FILE);
    y -= 30;
    CreateButton(hwnd, x + 80, y, 70, L"Save", WRITE_TO_FILE);

    g_demoName = CreateText(hwnd, x, y, 150, L"6-20.dem");
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
#else
    g_demoName = CreateText(hwnd, x, y, 150, L"all.dem");
    g_playButton = CreateButton(hwnd, x, y, 150, L"Play", SET_PLAYBACK_MODE);
#endif

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
}
