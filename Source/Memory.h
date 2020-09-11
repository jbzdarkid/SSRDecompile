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

// https://github.com/erayarslan/WriteProcessMemory-Example
// http://stackoverflow.com/q/32798185
// http://stackoverflow.com/q/36018838
// http://stackoverflow.com/q/1387064
// https://github.com/fkloiber/witness-trainer/blob/master/source/foreign_process_memory.cpp
class Memory final {
public:
    Memory(const std::wstring& processName);

    // lineLength is the number of bytes from the given index to the end of the instruction. Usually, it's 4.
    static __int64 ReadStaticInt(__int64 offset, int index, const std::vector<byte>& data, size_t lineLength = 4);
    using ScanFunc = std::function<void(__int64 offset, int index, const std::vector<byte>& data)>;
    void AddSigScan(const std::vector<byte>& scanBytes, const ScanFunc& scanFunc);
    [[nodiscard]] size_t ExecuteSigScans();

    template<class T>
    inline std::vector<T> ReadData(const std::vector<__int64>& offsets, size_t numItems) {
        std::vector<T> data(numItems, 0);
        ReadDataInternal(ComputeOffset(offsets), &data[0], numItems * sizeof(T));
        return data;
    }
    std::string ReadString(std::vector<__int64> offsets);

    template <class T>
    inline void WriteData(const std::vector<__int64>& offsets, const std::vector<T>& data) {
        WriteDataInternal(ComputeOffset(offsets), &data[0], sizeof(T) * data.size());
    }

    void Intercept(__int64 lineStart, __int64 lineEnd, const std::vector<byte>& data);

private:
    void ReadDataInternal(uintptr_t addr, void* buffer, size_t bufferSize);
    void WriteDataInternal(uintptr_t addr, const void* buffer, size_t bufferSize);
    uintptr_t ComputeOffset(std::vector<__int64> offsets);

    // Parts of the constructor / StartHeartbeat
    std::wstring _processName;
    HANDLE _handle = nullptr;
    DWORD _pid = 0;
    HWND _hwnd = NULL;
    uintptr_t _baseAddress = 0;

    // Parts of Read / Write / Sigscan
    ThreadSafeAddressMap _computedAddresses;

    struct SigScan {
        bool found = false;
        ScanFunc scanFunc;
    };
    std::map<std::vector<byte>, SigScan> _sigScans;
};
