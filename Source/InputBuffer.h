#pragma once
#include "Memory.h"

enum Mode : byte {
    Recording = 0x00,
    Playing = 0x01,
    ForwardStep = 0x02,

    BackStep = 0x10,

    Nothing = 0x50,
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

// My additions
    Undo,
};

class InputBuffer final
{
public:
    static std::shared_ptr<InputBuffer> Create();

    void SetMode(Mode mode);

    void ResetPosition();
    __int64 GetPosition();

    std::string GetDisplayText();

    void ReadFromFile(const std::string& filename);
    void WriteToFile(const std::string& filename);

private:
    InputBuffer() = default;

    std::vector<Direction> ReadData();
    void SetPosition(__int64 position);
    void WriteData(const std::vector<Direction>& data);

    std::shared_ptr<Memory> _memory;

    __int64 _buffer = 0;
};

