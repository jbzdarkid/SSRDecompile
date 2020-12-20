#pragma once
#include "Memory.h"

enum Mode : byte {
    Recording = 0x00,
    Playing = 0x01,
    ForwardStep = 0x02,

    BackStep = 0x10,

    UNUSED1 = 0x50,
    UNUSED2 = 0x51,
};

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

// Added by me
    Stop = 0xFF,
    Undo = 0xFE,
    Reset = 0xFD,
};

class InputBuffer final
{
public:
    static std::shared_ptr<InputBuffer> Create(const std::shared_ptr<Memory>& memory);

    void SetMode(Mode mode);
    Mode GetMode();

    void ResetPosition();
    __int64 GetPosition();

    std::string GetDisplayText();

    std::vector<int> GetPlayerPosition();
    void SetPlayerPosition(const std::vector<int>& position);

    void ReadFromFile(const std::wstring& filename);
    void WriteToFile(const std::wstring& filename);

    void WriteNone();

    // True if the injection worked
    bool BreakOnBadInput();

private:
    InputBuffer() = default;

    std::vector<Direction> ReadData();
    void SetPosition(__int64 position);
    void WriteData(const std::vector<Direction>& data);
    void Wipe();

    std::shared_ptr<Memory> _memory;

    __int64 _buffer = 0;
};

