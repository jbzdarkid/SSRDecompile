#include "pch.h"
#include "InputBuffer.h"

const size_t BUFFER_SIZE = 0x100000;

std::shared_ptr<InputBuffer> InputBuffer::Create()
{
    auto memory = Memory::Create(L"Sausage.exe");
    if (!memory) return nullptr;

    auto inputBuffer = std::shared_ptr<InputBuffer>(new InputBuffer());
    inputBuffer->_memory = memory;

    __int64 firstLine = 0, nextLine = 0;
    // Targeting Game.Playerinputstring (C#)
    memory->AddSigScan("BE 03000000 48 8B C6", [&firstLine, &nextLine](__int64 address, const std::vector<byte>& data) {
        firstLine = address + 5;
        nextLine = address + 20;
    });

    size_t notFound = memory->ExecuteSigScans();
    if (notFound > 0) return nullptr;

    __int64 buffer = memory->AllocateBuffer(BUFFER_SIZE);
    inputBuffer->_buffer = buffer;
    inputBuffer->ResetPosition();
    inputBuffer->SetMode(Nothing);
    inputBuffer->WriteData(std::vector<Direction>(BUFFER_SIZE - 9, None));

    memory->Intercept(firstLine, nextLine, {
        // direction is in sil (single byte, but the register is technically a long because C#)
        0x50,                                                   // push rax
        0x53,                                                   // push rbx
        0x51,                                                   // push rcx
        0xA0, LONG_TO_BYTES(buffer+8),                          // mov al, [buffer+8]   ; al = operation mode
        IF_LT(0x3C, 0x10),                                      // cmp al, 2            ; If we're in an active mode
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
                    0x48, 0x89, 0x0B,                           // mov [rbx], rcx       ; Write the new buffer size
                    0xC6, 0x43, 0x08, Recording,                // mov [rbx+8], 0x00    ; Change back to recording mode (so we can overwrite the buffer)
                    0x48, 0x01, 0xCB,                           // add rbx, rcx         ; rbx = first open buffer slot
                    0x40, 0x8A, 0x33                            // mov sil, [rbx]       ; Copy input from buffer to the game
                )                                               //
            )                                                   //
        ),                                                      //
        0x59,                                                   // pop rcx
        0x5B,                                                   // pop rbx
        0x58,                                                   // pop rax
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
    for (size_t i=offset; i<offset + 9 && i < data.size(); i++) {
        Direction dir = data[i];
        if (dir == North)       text += "North";
        else if (dir == South)  text += "South";
        else if (dir == East)   text += "East";
        else if (dir == West)   text += "West";
        else                    text += "  ";
        if (i == position) text += "\t<---";
        text += '\n';
    }
    return text;
}

void InputBuffer::ReadFromFile(const std::string& filename)
{
    std::vector<Direction> buffer;
    std::ifstream file(filename);
    for (std::string dir; std::getline(file, dir);) {
        if (dir == "North") buffer.push_back(North);
        if (dir == "South") buffer.push_back(South);
        if (dir == "East")  buffer.push_back(East);
        if (dir == "West")  buffer.push_back(West);
        if (dir == "None")  buffer.push_back(None); // Allow "None" in demo files, in case we need to buffer something, at some point.
    }
    WriteData(buffer);
}

void InputBuffer::WriteToFile(const std::string& filename)
{
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

