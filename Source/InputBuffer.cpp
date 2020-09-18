#include "pch.h"
#include "InputBuffer.h"

const size_t BUFFER_SIZE = 0x100000;

std::shared_ptr<InputBuffer> InputBuffer::Create(const std::shared_ptr<Memory>& memory)
{
    if (!memory) return nullptr;

    auto inputBuffer = std::shared_ptr<InputBuffer>(new InputBuffer());
    inputBuffer->_memory = memory;

    __int64 playerInputString = 0;
    // Game.Playerinputstring()
    memory->AddSigScan("BE 03000000 48 8B C6", [&playerInputString](__int64 address, const std::vector<byte>& data) {
        playerInputString = address + 5;
    });
    __int64 getButton= 0;
    // Rewired.Player.GetButton(string)
    memory->AddSigScan("E9 58000000 48 8B 47 10", [&getButton](__int64 address, const std::vector<byte>& data) {
        getButton = address + 93;
    });
    // Rewired.Player.GetButtonDown(string)
    __int64 getButtonDown = 0;
    memory->AddSigScan("0F84 61000000 48 8D 64 24 00", [&getButtonDown](__int64 address, const std::vector<byte>& data) {
        getButtonDown = address - 19;
    });

    size_t notFound = memory->ExecuteSigScans();
    if (notFound > 0) return nullptr;

    __int64 buffer = memory->AllocateBuffer(BUFFER_SIZE);
    inputBuffer->_buffer = buffer;
    inputBuffer->Wipe();

    memory->Intercept(playerInputString, playerInputString + 17, {
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
                    0x40, 0x8A, 0x33                            // mov sil, [rbx]       ; Copy input from buffer to the game
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
    memory->Intercept(getButton, getButton + 17, {
        // "is undo pressed" is in rax.
        // rdi, r15 are safe registers
        0x53,                                                   // push rbx
        0x51,                                                   // push rcx
        0x48, 0xBB, UNDO,                                       // mov rbx, 'undo'
        0x48, 0x8B, 0x4D, 0xE8,                                 // mov rcx, [rbp-0x18]  ; Reload the button string from the local variable
        IF_EQ(0x48, 0x39, 0x59, 0x14),                          // cmp [rcx+0x14], rbx  ; This code only runs for the 'undo' button. We aren't intercepting other inputs.
        THEN(                                                   //
            0x48, 0xBB, LONG_TO_BYTES(buffer),                  // mov rbx, buffer
            0x40, 0x8A, 0x7B, 0x08,                             // mov dil, [rbx+8]     ; dil = operation mode
            IF_EQ(0x40, 0x80, 0xFF, Recording),                 // cmp dil, 0           ; If we're in recording mode
            THEN(                                               //
                IF_EQ(0x3C, 0x01),                              // cmp al, 0x01         ; If undo was pressed, allow it through, and go back one step in the buffer.
                THEN(                                           //
                    0x48, 0x8B, 0x0B,                           // mov rcx, [rbx]       ; rcx = current playhead
                    0x48, 0xFF, 0xC9,                           // dec rcx
                    0xC6, 0x43, 0x08, UndoEnd,                  // mov [rbx+8], 0x51    ; Change mode to "waiting for end of undo"
                    IF_GE(0x48, 0x83, 0xF9, 0x08),              // cmp rcx, 8           ; Ensure that we don't bring the playhead too far back
                    THEN(0x48, 0x89, 0x0B)                      // mov [rbx], rcx       ; Write new playhead
                )                                               //
            ),                                                  //
                                                                //
            IF_EQ(0x40, 0x80, 0xFF, UndoEnd),                   // cmp dil, 0           ; If we're waiting for the end of an undo
            THEN(                                               //
                IF_EQ(0x3C, 0x00),                              // cmp al, 0x01         ; If undo is not pressed
                THEN(0xC6, 0x43, 0x08, Recording)               // mov [rbx+8], 0x00    ; Change mode back to recording
            ),                                                  //
                                                                //
            IF_EQ(0x40, 0x80, 0xFF, Playing),                   // cmp dil, 1           ; If we're in playback mode
            THEN(0x30, 0xC0),                                   // xor al, al           ; Swallow the undo press
                                                                //
            IF_EQ(0x40, 0x80, 0xFF, BackStep),                  // cmp dil, 0x10        ; Backstep (playback mode)
            THEN(                                               //                      ; Move the playhead back, and emit an undo.
                0xB0, 0x01,                                     // mov al, 0x01         ; Return true (button is pressed). Note that we don't change mode here.
                0x48, 0x8B, 0x0B,                               // mov rcx, [rbx]       ; rcx = current playhead
                0x48, 0xFF, 0xC9,                               // dec rcx
                IF_GE(0x48, 0x83, 0xF9, 0x08),                  // cmp rcx, 8           ; Ensure that we don't bring the playhead too far back
                THEN(0x48, 0x89, 0x0B)                          // mov [rbx], rcx       ; Write new playhead
            )                                                   //
        ),                                                      //
        0x59,                                                   // pop rcx
        0x5B,                                                   // pop rbx
    });

    memory->Intercept(getButtonDown, getButtonDown + 19, {
        0x53,                                                   // push rbx
        0x48, 0xBB, UNDO,                                       // mov rbx, 'undo'
        IF_EQ(0x48, 0x39, 0x5A, 0x14),                          // cmp [rdx+0x14], rbx  ; This code only runs for the 'undo' button. We aren't intercepting other inputs.
        THEN(                                                   //
            0x48, 0xBB, LONG_TO_BYTES(buffer),                  // mov rbx, buffer
            IF_EQ(0x80, 0x7B, 0x08, BackStep),                  // cmp [rbx+8], 0x10    ; Backstep (playback mode)
            THEN(                                               //
                0xC6, 0x43, 0x08, Recording,                    // mov [rbx+8], 0x00    ; Change back to recording mode (idle state)
                0xB0, 0x01,                                     // mov al, 0x01         ; Return true (button is pressed)
                0x5B,                                           // pop rbx              ; Safely exit the function
                0x49, 0xBB, LONG_TO_BYTES(getButtonDown + 209), // mov r11, (end of source function)
                0x41, 0xFF, 0xE3                                // jmp r11
            )                                                   //
        ),                                                      //
        0x5B,                                                   // pop rbx
    });

    return inputBuffer;
}

void InputBuffer::SetMode(Mode mode)
{
    _memory->WriteData<byte>(_buffer + 8, {static_cast<byte>(mode)});
}

Mode InputBuffer::GetMode()
{
    return static_cast<Mode>(_memory->ReadData<byte>(_buffer + 8, 1)[0]);
}

void InputBuffer::ResetPosition()
{
    SetPosition(0);
}

__int64 InputBuffer::GetPosition()
{
    return _memory->ReadData<__int64>(_buffer, 1)[0] - 8;
}

std::string InputBuffer::GetDisplayText()
{
    __int64 position = GetPosition();
    std::vector<Direction> data = ReadData();

    size_t offset = (position > 5 ? position - 5 : 0);
    
    std::string text;
    for (size_t i=offset; i<offset + 11 && i < data.size(); i++) {
        Direction dir = data[i];
        if (dir == North)       text += "North";
        else if (dir == South)  text += "South";
        else if (dir == East)   text += "East";
        else if (dir == West)   text += "West";
        else                    text += "  ";
        if (i == position) text += "\t<--- (Next)";
        text += '\n';
    }
    return text;
}

// std::vector<int> GetPlayerPosition() {
//     if (!g_memory) return {0, 0, 0};
//     return g_memory->ReadData<int>({0x1547668, 0x58, 0x98, 0x40, 0x18, 0x100, 0xD8}, 3);
// }

void InputBuffer::SetPlayerPosition(const std::vector<int>& position) {
    if (!_memory) return;
    // UnityPlayer.dll
    _memory->WriteData<int>({0x6197A0000 + 0x1547668, 0x58, 0x98, 0x40, 0x18, 0x100, 0xD8}, position);
}

void InputBuffer::ReadFromFile(const std::wstring& filename) {
    std::vector<Direction> buffer;
    std::ifstream file(filename);
    std::string line;
    std::getline(file, line);
    int x = std::stoi(line);
    std::getline(file, line);
    int y = std::stoi(line);
    std::getline(file, line);
    int z = std::stoi(line);
    // SetPlayerPosition({x, y, z});

    while (std::getline(file, line)) {
        if (line == "North") buffer.push_back(North);
        if (line == "South") buffer.push_back(South);
        if (line == "East")  buffer.push_back(East);
        if (line == "West")  buffer.push_back(West);
        if (line == "None")  buffer.push_back(None); // Allow "None" in demo files, in case we need to buffer something, at some point.
    }
    Wipe();
    WriteData(buffer);
}

void InputBuffer::WriteToFile(const std::wstring& filename) {
    std::vector<std::string> positionLines;
    std::ifstream inFile(filename);

    if (inFile.is_open()) {
        std::string line;
        std::getline(inFile, line);
        positionLines.push_back(line);
        std::getline(inFile, line);
        positionLines.push_back(line);
        std::getline(inFile, line);
        positionLines.push_back(line);
        inFile.close();
    }

    std::ofstream file(filename);
    for (const auto& line : positionLines) file << line << '\n';
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

std::vector<Direction> InputBuffer::ReadData()
{
    // Read up to the buffer size.
    const std::vector<byte> data = _memory->ReadData<byte>(_buffer + 9, BUFFER_SIZE - 9);
    std::vector<Direction> out(data.size(), None);
    const byte* dataBegin = &data[0];
    Direction* outBegin = &out[0];

    for (size_t i=0; i<data.size(); i++) *(outBegin + i) = static_cast<Direction>(*(dataBegin + i));

    return out;
}

void InputBuffer::SetPosition(__int64 position)
{
    _memory->WriteData<__int64>(_buffer, {position + 8});
}

void InputBuffer::WriteData(const std::vector<Direction>& data)
{
    ResetPosition();
    _memory->WriteData<Direction>(_buffer + 9, data);
}

void InputBuffer::Wipe()
{
    ResetPosition();
    SetMode(Nothing);
    WriteData(std::vector<Direction>(BUFFER_SIZE - 9, None));
}

