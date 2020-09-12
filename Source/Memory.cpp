#include "pch.h"
#include "Memory.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <sstream>
#include <iomanip>

Memory::Memory(const std::wstring& processName) : _processName(processName) {
    HANDLE handle = nullptr;
    // First, get the handle of the process
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    while (Process32NextW(snapshot, &entry)) {
        if (_processName == entry.szExeFile) {
            _pid = entry.th32ProcessID;
            handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, _pid);
            break;
        }
    }
    // Game likely not opened yet. Don't spam the log.
    if (!handle || !_pid) return;
    DebugPrint(L"Found " + _processName + L": PID " + std::to_wstring(_pid));

    _hwnd = NULL; // Will be populated later.

    _baseAddress = DebugUtils::GetBaseAddress(handle);
    if (_baseAddress == 0) {
        DebugPrint("Couldn't locate base address");
        return;
    }

    // Clear sigscans to avoid duplication (or leftover sigscans from the trainer)
    assert(_sigScans.size() == 0);
    _sigScans.clear();
    _handle = handle;
}

__int64 Memory::ReadStaticInt(__int64 offset, int index, const std::vector<byte>& data, size_t lineLength) {
    // (address of next line) + (index interpreted as 4byte int)
    return offset + index + lineLength + *(int*)&data[index];
}

void Memory::AddSigScan(const std::string& scan, const ScanFunc& scanFunc) {
    std::vector<byte> scanBytes;
    bool first = true; // First half of the byte
    for (const char c : scan) {
        if (c == ' ') continue;

        byte b = 0x00;
        if (c >= '0' && c <= '9') b = c - '0';
        else if (c >= 'A' && c <= 'F') b = c - 'A' + 0xA;
        else if (c >= 'a' && c <= 'f') b = c - 'a' + 0xa;
        else continue; // TODO: Support '?', somehow
        if (first) {
            scanBytes.push_back(b * 0x10);
            first = false;
        } else {
            scanBytes[scanBytes.size()-1] |= b;
            first = true;
        }
    }
    assert(first);

    _sigScans[scanBytes] = {false, scanFunc};
}

int find(const std::vector<byte> &data, const std::vector<byte>& search, size_t startIndex = 0) {
    for (size_t i=startIndex; i<data.size() - search.size(); i++) {
        bool match = true;
        for (size_t j=0; j<search.size(); j++) {
            if (data[i+j] == search[j]) {
                continue;
            }
            match = false;
            break;
        }
        if (match) return static_cast<int>(i);
    }
    return -1;
}

#define BUFFER_SIZE 0x10000 // 10 KB
size_t Memory::ExecuteSigScans() {
    size_t notFound = 0;
    for (const auto& [_, sigScan] : _sigScans) if (!sigScan.found) notFound++;
    std::vector<byte> buff;
    buff.resize(BUFFER_SIZE + 0x100); // padding in case the sigscan is past the end of the buffer

    for (uintptr_t i = 0; i < 0x1'000'000'000; i += BUFFER_SIZE) {
        SIZE_T numBytesWritten;
        if (!ReadProcessMemory(_handle, reinterpret_cast<void*>(_baseAddress + i), &buff[0], buff.size(), &numBytesWritten)) continue;
        buff.resize(numBytesWritten);
        for (auto& [scanBytes, sigScan] : _sigScans) {
            if (sigScan.found) continue;
            int index = find(buff, scanBytes);
            if (index == -1) continue;
            sigScan.scanFunc(i, index, buff); // We're expecting i to be relative to the base address here.
            sigScan.found = true;
            notFound--;
        }
        if (notFound == 0) break;
    }

    if (notFound > 0) {
        DebugPrint("Failed to find " + std::to_string(notFound) + " sigscans:");
        for (const auto& [scanBytes, sigScan] : _sigScans) {
            if (sigScan.found) continue;
            std::stringstream ss;
            for (const auto b : scanBytes) {
                ss << "0x" << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int16_t>(b) << ", ";
            }
            DebugPrint(ss.str());
        }
    }

    _sigScans.clear();
    return notFound;
}

#define MAX_STRING 100
// Technically this is ReadChar*, but this name makes more sense with the return type.
std::string Memory::ReadString(std::vector<__int64> offsets) {
    __int64 charAddr = ReadData<__int64>(offsets, 1)[0];
    if (charAddr == 0) return ""; // Handle nullptr for strings

    offsets.push_back(0L); // Dereference the char* to a char[]
    std::vector<char> tmp = ReadData<char>(offsets, MAX_STRING);
    std::string name(tmp.begin(), tmp.end());
    // Remove garbage past the null terminator (we read 100 chars, but the string was probably shorter)
    name.resize(strnlen_s(tmp.data(), tmp.size()));
    if (name.size() == tmp.size()) {
        DebugPrint("Buffer did not get shrunk, ergo this string is longer than 100 chars. Please change MAX_STRING.");
        assert(false);
    }
    return name;
}

void Memory::Intercept(__int64 firstLine, __int64 nextLine, const std::vector<byte>& data) {
    // We need enough space for the jump in the source code
    assert(nextLine - firstLine >= 14);

    std::vector<byte> jumpBack = {
        0x50,                                                        // push rax
        0x48, 0xB8, LONG_TO_BYTES_LE(_baseAddress + firstLine + 13), // mov rax, firstLine + 13
        0xFF, 0xE0,                                                  // jmp rax
    };

    std::vector<byte> injectionBytes = {0x58}; // pop rax (before executing code that might need it)
    injectionBytes.insert(injectionBytes.end(), data.begin(), data.end());
    injectionBytes.push_back(0x90); // Padding nop
    std::vector<byte> replacedCode = ReadData<byte>({firstLine}, nextLine - firstLine);
    injectionBytes.insert(injectionBytes.end(), replacedCode.begin(), replacedCode.end());
    injectionBytes.push_back(0x90); // Padding nop
    injectionBytes.insert(injectionBytes.end(), jumpBack.begin(), jumpBack.end());

    __int64 addr = (__int64)VirtualAllocEx(_handle, NULL, injectionBytes.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    // _allocations.emplace_back(addr);
    WriteDataInternal(addr, &injectionBytes[0], injectionBytes.size());

    std::vector<byte> jumpAway = {
        0x50,                               // push rax
        0x48, 0xB8, LONG_TO_BYTES_LE(addr), // mov rax, addr
        0xFF, 0xE0,                         // jmp rax
        0x58,                               // pop rax (we return to this opcode)
    };
    // Fill any leftover space with nops
    for (size_t i=jumpAway.size(); i<static_cast<size_t>(nextLine - firstLine); i++) jumpAway.push_back(0x90);
    WriteData<byte>({firstLine}, jumpAway);
}

void Memory::ReadDataInternal(uintptr_t addr, void* buffer, size_t bufferSize) {
    assert(bufferSize > 0);
    if (!_handle) return;
    // Ensure that the buffer size does not cause a read across a page boundary.
    if (bufferSize > 0x1000 - (addr & 0x0000FFF)) {
        bufferSize = 0x1000 - (addr & 0x0000FFF);
    }
    if (!ReadProcessMemory(_handle, (void*)addr, buffer, bufferSize, nullptr)) {
        DebugPrint("Failed to read process memory.");
        assert(false);
    }
}

void Memory::WriteDataInternal(uintptr_t addr, const void* buffer, size_t bufferSize) {
    assert(bufferSize > 0);
    if (!_handle) return;
    if (!WriteProcessMemory(_handle, (void*)addr, buffer, bufferSize, nullptr)) {
        DebugPrint("Failed to write process memory.");
        assert(false);
    }
}

uintptr_t Memory::ComputeOffset(std::vector<__int64> offsets) {
    assert(offsets.size() > 0);
    assert(offsets.front() != 0);

    // Leave off the last offset, since it will be either read/write, and may not be of type uintptr_t.
    const __int64 final_offset = offsets.back();
    offsets.pop_back();

    uintptr_t cumulativeAddress = _baseAddress;
    for (const __int64 offset : offsets) {
        cumulativeAddress += offset;

        // If the address was already computed, continue to the next offset.
        uintptr_t foundAddress = _computedAddresses.Find(cumulativeAddress);
        if (foundAddress != 0) {
            cumulativeAddress = foundAddress;
            continue;
        }

        // If the address was not yet computed, read it from memory.
        uintptr_t computedAddress = 0;
        if (!_handle) return 0;
        if (!ReadProcessMemory(_handle, reinterpret_cast<LPCVOID>(cumulativeAddress), &computedAddress, sizeof(computedAddress), NULL)) {
            DebugPrint("Failed to read process memory.");
            assert(false);
            return 0;
        } else if (computedAddress == 0) {
            DebugPrint("Attempted to dereference NULL!");
            assert(false);
            return 0;
        }

        _computedAddresses.Set(cumulativeAddress, computedAddress);
        cumulativeAddress = computedAddress;
    }
    return cumulativeAddress + final_offset;
}
