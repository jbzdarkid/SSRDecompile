#include "pch.h"
#include "InputBuffer.h"

const size_t BUFFER_SIZE = 0x100000;

std::shared_ptr<InputBuffer> InputBuffer::Create(const std::shared_ptr<Memory>& memory) {
    if (!memory) return nullptr;

    auto inputBuffer = std::shared_ptr<InputBuffer>(new InputBuffer());
    inputBuffer->_memory = memory;

    // Game.Playerinputstring()
    __int64 playerInputString = 0;
    memory->AddSigScan("BE 03000000 48 8B C6", [&playerInputString](__int64 address, const std::vector<byte>& data) {
        playerInputString = address + 5;
    });
    // Rewired.Player.GetButton(string)
    __int64 getButton = 0;
    memory->AddSigScan("E9 58000000 48 8B 47 10", [&getButton](__int64 address, const std::vector<byte>& data) {
        getButton = address + 93;
    });
    // Rewired.Player.GetButtonDown(string)
    __int64 getButtonDown = 0;
    memory->AddSigScan("0F84 61000000 48 8D 64 24 00", [&getButtonDown](__int64 address, const std::vector<byte>& data) {
        getButtonDown = address - 0x25;
    });
    // Game.DoUndo()
    __int64 doUndo = 0;
    memory->AddSigScan("48 81 C4 E0 00 00 00 48 8B 86", [&doUndo](__int64 address, const std::vector<byte>& data) {
        doUndo = address;
    });

    size_t notFound = memory->ExecuteSigScans();
    if (notFound > 0) return nullptr;

    __int64 buffer = memory->AllocateBuffer(BUFFER_SIZE);
    inputBuffer->_buffer = buffer;
    inputBuffer->Wipe();

    memory->Intercept("Playerinputstring", playerInputString, playerInputString + 17, {
        // direction is in sil (single byte, but the register is technically a long because C#)
        0x50,                                                   // push rax
        0x53,                                                   // push rbx
        0x51,                                                   // push rcx
        0xA0, LONG_TO_BYTES(buffer+8),                          // mov al, [buffer+8]   ; al = operation mode
        IF_LT(0x3C, 0x10),                                      // cmp al, 0x10         ; If we're in an active mode
        THEN(                                                   //
            0x48, 0xBB, LONG_TO_BYTES(buffer),                  // mov rbx, buffer
            0x48, 0x8B, 0x0B,                                   // mov rcx, [rbx]       ; rcx = current buffer size
            0x48, 0xFF, 0xC1,                                   // inc rcx              ; rcx = new buffer size
            IF_LT(0x48, 0x81, 0xF9, INT_TO_BYTES(BUFFER_SIZE)), // cmp rcx, bufferSize  ; If this would not cause the buffer to overflow
            THEN(                                               //
                                                                //
                IF_EQ(0x3C, Recording),                         // cmp al, 0            ; If we're in recording mode
                THEN(                                           //
                    IF_NE(0x40, 0x80, 0xFE, 0x08),              // cmp sil, 0x08        ; If we took an action (dir != None)
                    THEN(                                       //
                        0x48, 0x89, 0x0B,                       // mov [rbx], rcx       ; Write the new buffer size
                        0x48, 0x01, 0xCB,                       // add rbx, rcx         ; rbx = first open buffer slot
                        0x40, 0x88, 0x33                        // mov [rbx], sil       ; Copy input from the game into buffer
                    )                                           //
                ),                                              //
                                                                //
                IF_EQ(0x3C, Playing),                           // cmp al, 1            ; If we're in playback mode
                THEN(                                           //
                    0x48, 0x89, 0x0B,                           // mov [rbx], rcx       ; Write the new buffer size
                    0x48, 0x01, 0xCB,                           // add rbx, rcx         ; rbx = first open buffer slot
                    0x40, 0x8A, 0x33,                           // mov sil, [rbx]       ; Copy input from buffer to the game
                    IF_EQ(0x40, 0x80, 0xFE, Stop),              // cmp sil, 0xFF        ; If we read a Stop input
                    THEN(
                        0x48, 0xBB, LONG_TO_BYTES(buffer),      // mov rbx, buffer
                        0xC6, 0x43, 0x08, Recording,            // mov [rbx+8], 0x00    ; Change back to recording mode (idle state)
                        0x48, 0x8B, 0x0B,                       // mov rcx, [rbx]       ; rcx = current playhead
                        0x48, 0xFF, 0xC9,                       // dec rcx
                        IF_GE(0x48, 0x83, 0xF9, 0x08),          // cmp rcx, 8           ; Ensure that we don't bring the playhead too far back
                        THEN(0x48, 0x89, 0x0B)                  // mov [rbx], rcx       ; Write new playhead
                    )                                           //
                ),                                              //
                                                                //
                IF_EQ(0x3C, ForwardStep),                       // cmp al, 2            ; Single step (in playback mode)
                THEN(                                           //
                    0xC6, 0x43, 0x08, Recording,                // mov [rbx+8], 0x00    ; Change back to recording mode (so we can overwrite the buffer)
                    0x48, 0x89, 0x0B,                           // mov [rbx], rcx       ; Write the new buffer size
                    0x48, 0x01, 0xCB,                           // add rbx, rcx         ; rbx = first open buffer slot
                    0x40, 0x8A, 0x33                            // mov sil, [rbx]       ; Copy input from buffer to the game
                )                                               //
            )                                                   //
        ),                                                      //
        0x59,                                                   // pop rcx
        0x5B,                                                   // pop rbx
        0x58,                                                   // pop rax
    });  
    #define UNDO 'u', '\0', 'n', '\0', 'd', '\0', 'o', '\0'
    #define REST 'r', '\0', 'e', '\0', 's', '\0', 't', '\0'
    memory->Intercept("GetButton", getButton, getButton + 17, {
        0x53,                                                   // push rbx
        0x51,                                                   // push rcx
        0x48, 0xBB, UNDO,                                       // mov rbx, 'undo'
        0x48, 0x8B, 0x4D, 0xE8,                                 // mov rcx, [rbp-0x18]  ; Reload the button string from the local variable
        IF_EQ(0x48, 0x39, 0x59, 0x14),                          // cmp [rcx+0x14], rbx  ; This code only runs for the 'undo' button
        THEN(                                                   //
            0x48, 0xBB, LONG_TO_BYTES(buffer),                  // mov rbx, buffer
            IF_EQ(0x80, 0x7B, 0x08, Playing),                   // cmp [rbx+8], 0x01    ; Playback mode
            THEN(                                               //
                0x48, 0x8B, 0x0B,                               // mov rcx, [rbx]       ; rcx = current buffer size
                0x48, 0xFF, 0xC1,                               // inc rcx              ; rcx = new buffer size
                0x48, 0x01, 0xCB,                               // add rbx, rcx         ; rbx = next input from the buffer
                IF_EQ(0x80, 0x3B, Undo),                        // cmp [rbx], Undo      ; If we read a undo input
                THEN(0xB0, 0x01)                                // mov al, 0x01         ; Return true (button is pressed)
            )                                                   //
        ),                                                      //
        0x48, 0xBB, REST,                                       // mov rbx, 'rest'
        0x48, 0x8B, 0x4D, 0xE8,                                 // mov rcx, [rbp-0x18]  ; Reload the button string from the local variable
        IF_EQ(0x48, 0x39, 0x59, 0x14),                          // cmp [rcx+0x14], rbx  ; This code only runs for the 'restart' button
        THEN(                                                   //
            0x48, 0xBB, LONG_TO_BYTES(buffer),                  // mov rbx, buffer
            IF_EQ(0x80, 0x7B, 0x08, Playing),                   // cmp [rbx+8], 0x01    ; Playback mode
            THEN(                                               //
                0x48, 0x8B, 0x0B,                               // mov rcx, [rbx]       ; rcx = current buffer size
                0x48, 0xFF, 0xC1,                               // inc rcx              ; rcx = new buffer size
                0x48, 0x01, 0xCB,                               // add rbx, rcx         ; rbx = next input from the buffer
                IF_EQ(0x80, 0x3B, Reset),                       // cmp [rbx], Reset     ; If we read a reset input
                THEN(0xB0, 0x01)                                // mov al, 0x01         ; Return true (button is pressed)
            )                                                   //
        ),                                                      //
        0x59,                                                   // pop rcx
        0x5B,                                                   // pop rbx
    });

    memory->Intercept("GetButtonDown", getButtonDown + 227, getButtonDown + 227 + 19, {
        0x53,                                                   // push rbx
        0x51,                                                   // push rcx
        0x48, 0xBB, UNDO,                                       // mov rbx, 'undo'
        IF_EQ(0x48, 0x39, 0x5E, 0x14),                          // cmp [rsi+0x14], rbx  ; This code only runs for the 'undo' button
        THEN(                                                   //
            0x48, 0xBB, LONG_TO_BYTES(buffer),                  // mov rbx, buffer
            IF_EQ(0x80, 0x7B, 0x08, BackStep),                  // cmp [rbx+8], 0x10    ; Backstep (playback mode)
            THEN(                                               //
                0xC6, 0x43, 0x08, Recording,                    // mov [rbx+8], 0x00    ; Change back to recording mode (idle state)
                0xB0, 0x01                                      // mov al, 0x01         ; Return true (button is pressed)
            ),                                                  //
            IF_EQ(0x80, 0x7B, 0x08, Playing),                   // cmp [rbx+8], 0x01    ; Playback mode
            THEN(                                               //
                0x48, 0x8B, 0x0B,                               // mov rcx, [rbx]
                0x48, 0xFF, 0xC1,                               // inc rcx              ; rcx = new buffer size
                0x48, 0x01, 0xCB,                               // add rbx, rcx         ; rbx = next input from the buffer
                IF_EQ(0x80, 0x3B, Undo),                        // cmp [rbx], Undo      ; If we read a undo input
                THEN(
                    0x48, 0xBB, LONG_TO_BYTES(buffer),          // mov rbx, buffer
                    0x48, 0x8B, 0x0B,                           // mov rcx, [rbx]       ; rcx = current buffer size
                    0x48, 0xFF, 0xC1,                           // inc rcx              ; rcx = new buffer size
                    0x48, 0x89, 0x0B,                           // mov [rbx], rcx       ; Write the new buffer size
                    0xB0, 0x01                                  // mov al, 0x01         ; Return true (button is pressed)
                )
            )                                                   //
        ),                                                      //
        0x48, 0xBB, REST,                                       // mov rbx, 'rest'
        IF_EQ(0x48, 0x39, 0x5E, 0x14),                          // cmp [rsi+0x14], rbx  ; This code only runs for the 'restart' button
        THEN(                                                   //
            0x48, 0xBB, LONG_TO_BYTES(buffer),                  // mov rbx, buffer
            IF_EQ(0x80, 0x7B, 0x08, Playing),                   // cmp [rbx+8], 0x01    ; Only relevant for playing mode
            THEN(
                0x48, 0x8B, 0x0B,                               // mov rcx, [rbx]
                0x48, 0xFF, 0xC1,                               // inc rcx              ; rcx = new buffer size
                0x48, 0x01, 0xCB,                               // add rbx, rcx         ; rbx = next input from the buffer
                IF_EQ(0x80, 0x3B, Reset),                       // cmp [rbx], Reset     ; If we read a reset input
                THEN(
                    0x48, 0xBB, LONG_TO_BYTES(buffer),          // mov rbx, buffer
                    0x48, 0x8B, 0x0B,                           // mov rcx, [rbx]       ; rcx = current playhead
                    0x48, 0xFF, 0xC1,                           // inc rcx              ; rcx = new buffer size
                    0x48, 0x89, 0x0B,                           // mov [rbx], rcx       ; Write new buffer size
                    0xB0, 0x01                                  // mov al, 0x01         ; Return true (button is pressed)
                )                                               //
            )                                                   //
        ),                                                      //
        0x59,                                                   // pop rcx
        0x5B,                                                   // pop rbx
    });

    memory->Intercept("DoUndo", doUndo, doUndo + 17, {
        0x53,                                                   // push rbx
        0x51,                                                   // push rcx
        0x48, 0xBB, LONG_TO_BYTES(buffer),                      // mov rbx, buffer
        IF_EQ(0x80, 0x7B, 0x08, Recording),                     // cmp [rbx+8], 0x00    ; If we're recording
        THEN(                                                   //
            0x48, 0x8B, 0x0B,                                   // mov rcx, [rbx]       ; rcx = current playhead
            0x48, 0xFF, 0xC9,                                   // dec rcx
            IF_GE(0x48, 0x83, 0xF9, 0x08),                      // cmp rcx, 8           ; Ensure that we don't bring the playhead too far back
            THEN(0x48, 0x89, 0x0B)                              // mov [rbx], rcx       ; Write new playhead
        ),                                                      //
        0x59,                                                   // pop rcx
        0x5B,                                                   // pop rbx
    });

    return inputBuffer;
}

void InputBuffer::SetMode(Mode mode) {
    _memory->WriteData<byte>(_buffer + 8, {static_cast<byte>(mode)});
}

Mode InputBuffer::GetMode() {
    return static_cast<Mode>(_memory->ReadData<byte>(_buffer + 8, 1)[0]);
}

void InputBuffer::ResetPosition() {
    SetPosition(0);
}

__int64 InputBuffer::GetPosition() {
    return _memory->ReadData<__int64>(_buffer, 1)[0] - 8;
}

std::string InputBuffer::GetDisplayText() {
    __int64 position = GetPosition();
    std::vector<Direction> data = ReadData();

    size_t offset = std::max(0ll, position-10);
    
    std::string text;
    for (size_t i=offset; i<offset + 21 && i < data.size(); i++) {
        Direction dir = data[i];
        if (dir == North)       text += "North";
        else if (dir == South)  text += "South";
        else if (dir == East)   text += "East";
        else if (dir == West)   text += "West";
        else if (dir == Undo)   text += "[Undo]";
        else if (dir == Reset)  text += "[Reset]";
        else if (dir == Stop)   text += "[Stop]";
        if (i == position) text += "\t<---  " + std::to_string(position);
        text += '\n';
    }
    return text;
}

std::vector<int> InputBuffer::GetPlayerPosition() {
    if (!_memory) return {0, 0, 0};
    return _memory->ReadData<int>(L"UnityPlayer.dll", {0x1547668, 0x58, 0x98, 0x40, 0x18, 0x100, 0xD8}, 3);
}

void InputBuffer::SetPlayerPosition(const std::vector<int>& position) {
    if (!_memory) return;
    _memory->WriteData<int>(L"UnityPlayer.dll", {0x1547668, 0x58, 0x98, 0x40, 0x18, 0x100, 0xD8}, position);
}

void InputBuffer::ReadFromFile(const std::wstring& filename) {
    std::vector<Direction> buffer;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        if (line == "North") buffer.push_back(North);
        if (line == "South") buffer.push_back(South);
        if (line == "East")  buffer.push_back(East);
        if (line == "West")  buffer.push_back(West);
        if (line == "Undo")  buffer.push_back(Undo);
        if (line == "Reset") buffer.push_back(Reset);
        if (line == "None")  buffer.push_back(None); // Allow "None" in demo files, in case we need to buffer something, at some point.
    }
    buffer.push_back(Stop);
    Wipe();
    WriteData(buffer);
}

void InputBuffer::WriteToFile(const std::wstring& filename) {
    std::ofstream file(filename);
    auto data = ReadData();
    size_t bufferSize = GetPosition();
    for (size_t i = 0; i<bufferSize; i++) {
        Direction dir = data[i];
        if (dir == North) file << "North\n";
        if (dir == South) file << "South\n";
        if (dir == East)  file << "East\n";
        if (dir == West)  file << "West\n";
    }
    file.close();
}

void InputBuffer::WriteNone() {
    __int64 position = GetPosition();
    _memory->WriteData<Direction>(_buffer + 9 + position, {None});
    SetPosition(position + 1);
}

bool InputBuffer::BreakOnBadInput() {
    // DiscardUndoSnapshot
    _memory->AddSigScan("48 63 40 18 85 C0 7E 31", [_memory = _memory](__int64 address, const std::vector<byte>& data) {
        _memory->WriteData<byte>(address - 16, {0xCC, 0xCC}); // This function shouldn't be called -- it signals dead inputs
    });
    return _memory->ExecuteSigScans() == 0;
}

std::vector<Direction> InputBuffer::ReadData() {
    // Read up to the buffer size.
    const std::vector<byte> data = _memory->ReadData<byte>(_buffer + 9, BUFFER_SIZE - 9);
    std::vector<Direction> out(data.size(), None);
    const byte* dataBegin = &data[0];
    Direction* outBegin = &out[0];

    for (size_t i=0; i<data.size(); i++) *(outBegin + i) = static_cast<Direction>(*(dataBegin + i));

    return out;
}

void InputBuffer::SetPosition(__int64 position) {
    _memory->WriteData<__int64>(_buffer, {position + 8});
}

void InputBuffer::WriteData(const std::vector<Direction>& data) {
    ResetPosition();
    _memory->WriteData<Direction>(_buffer + 9, data);
}

void InputBuffer::Wipe() {
    ResetPosition();
    SetMode(Recording);
    WriteData(std::vector<Direction>(BUFFER_SIZE - 9, None));
}
