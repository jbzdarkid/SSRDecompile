#pragma once
#include "ThreadSafeAddressMap.h"
#include <vector>
#include <functional>

enum ProcStatus : WPARAM {
    NotRunning,
    Started,
    Running,
    Reload,
    NewGame,
    Stopped
};

using byte = unsigned char;
// Note: Little endian
#define LONG_TO_BYTES(val) \
    static_cast<byte>((val & 0x00000000000000FF) >> 0x00), \
    static_cast<byte>((val & 0x000000000000FF00) >> 0x08), \
    static_cast<byte>((val & 0x0000000000FF0000) >> 0x10), \
    static_cast<byte>((val & 0x00000000FF000000) >> 0x18), \
    static_cast<byte>((val & 0x000000FF00000000) >> 0x20), \
    static_cast<byte>((val & 0x0000FF0000000000) >> 0x28), \
    static_cast<byte>((val & 0x00FF000000000000) >> 0x30), \
    static_cast<byte>((val & 0xFF00000000000000) >> 0x38)

// Note: Little endian
#define INT_TO_BYTES(val) \
    static_cast<byte>((val & 0x000000FF) >> 0x00), \
    static_cast<byte>((val & 0x0000FF00) >> 0x08), \
    static_cast<byte>((val & 0x00FF0000) >> 0x10), \
    static_cast<byte>((val & 0xFF000000) >> 0x18)

#define ARGCOUNT(...) std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value

#define IF_GE(...) __VA_ARGS__, 0x72 // jb
#define IF_LT(...) __VA_ARGS__, 0x73 // jae
#define IF_NE(...) __VA_ARGS__, 0x74 // je
#define IF_EQ(...) __VA_ARGS__, 0x75 // jne
#define IF_GT(...) __VA_ARGS__, 0x76 // jbe
#define IF_LE(...) __VA_ARGS__, 0x77 // ja
#define THEN(...) ARGCOUNT(__VA_ARGS__), __VA_ARGS__

// https://github.com/erayarslan/WriteProcessMemory-Example
// http://stackoverflow.com/q/32798185
// http://stackoverflow.com/q/36018838
// http://stackoverflow.com/q/1387064
// https://github.com/fkloiber/witness-trainer/blob/master/source/foreign_process_memory.cpp
class Memory final {
public:
    static std::shared_ptr<Memory> Create(const std::wstring& processName);
    ~Memory();
    Memory(const Memory& other) = delete;
    Memory& operator=(const Memory& other) = delete;

    // lineLength is the number of bytes from the given index to the end of the instruction. Usually, it's 4.
    static __int64 ReadStaticInt(__int64 offset, int index, const std::vector<byte>& data, size_t lineLength = 4);
    using ScanFunc = std::function<void(__int64 offset, int index, const std::vector<byte>& data)>;
    void AddSigScan(const std::string& scan, const ScanFunc& scanFunc);
    [[nodiscard]] size_t ExecuteSigScans();

    template<typename T>
    inline std::vector<T> ReadData(const std::vector<__int64>& offsets, size_t numItems) {
        std::vector<T> data(numItems, 0);
        ReadDataInternal(ComputeOffset(offsets), &data[0], numItems * sizeof(T));
        return data;
    }
    std::string ReadString(std::vector<__int64> offsets);

    template <typename T>
    inline void WriteData(const std::vector<__int64>& offsets, const std::vector<T>& data) {
        WriteDataInternal(ComputeOffset(offsets), &data[0], sizeof(T) * data.size());
    }

    void Intercept(__int64 firstLine, __int64 nextLine, const std::vector<byte>& data);
    void Unintercept(__int64 firstLine, const std::vector<byte>& replacedCode, __int64 addr);
    __int64 AllocateBuffer(size_t bufferSize, const std::vector<byte>& initialData = {});

private:
    Memory() = default;
    void ReadDataInternal(uintptr_t addr, void* buffer, size_t bufferSize);
    void WriteDataInternal(uintptr_t addr, const void* buffer, size_t bufferSize);
    uintptr_t ComputeOffset(std::vector<__int64> offsets);

    // Parts of the constructor / StartHeartbeat
    std::wstring _processName;
    HANDLE _handle = nullptr;
    DWORD _pid = 0;
    uintptr_t _baseAddress = 0;
    std::vector<__int64> _allocations;
    std::vector<std::tuple<__int64, std::vector<byte>, __int64>> _interceptions;

    // Parts of Read / Write / Sigscan
    ThreadSafeAddressMap _computedAddresses;

    struct SigScan {
        bool found = false;
        ScanFunc scanFunc;
    };
    std::map<std::vector<byte>, SigScan> _sigScans;
};
