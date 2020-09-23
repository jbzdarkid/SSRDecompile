#include "pch.h"
#include "Memory.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <sstream>
#include <iomanip>

std::shared_ptr<Memory> Memory::Create(const std::wstring& processName) {
    auto memory = std::shared_ptr<Memory>(new Memory());
    memory->_processName = processName;

    HANDLE handle = nullptr;
    // First, get the handle of the process
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    while (Process32NextW(snapshot, &entry)) {
        if (memory->_processName == entry.szExeFile) {
            memory->_pid = entry.th32ProcessID;
            handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, memory->_pid);
            break;
        }
    }
    // Game likely not opened yet. Don't spam the log.
    if (!handle || !memory->_pid) return nullptr;
    DebugPrint(L"Found " + memory->_processName + L": PID " + std::to_wstring(memory->_pid));

    memory->_baseAddress = DebugUtils::GetBaseAddress(handle);
    if (memory->_baseAddress == 0) {
        DebugPrint("Couldn't locate base address");
        return nullptr;
    }

    // Clear sigscans to avoid duplication (or leftover sigscans from the trainer)
    assert(memory->_sigScans.size() == 0);
    memory->_sigScans.clear();
    memory->_handle = handle;
    return memory;
}

Memory::~Memory() {
    for (const auto& interception : _interceptions) Unintercept(std::get<0>(interception), std::get<1>(interception), std::get<2>(interception));
    for (void* addr : _allocations) {
        if (addr != nullptr) VirtualFreeEx(_handle, addr, 0, MEM_RELEASE);
    }
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

int find(const std::vector<byte> &data, const std::vector<byte>& search) {
    const byte* dataBegin = &data[0];
    const byte* searchBegin = &search[0];
    size_t maxI = data.size();
    size_t maxJ = search.size();

    for (int i=0; i<maxI; i++) {
        bool match = true;
        for (size_t j=0; j<maxJ; j++) {
            if (*(dataBegin + i + j) == *(searchBegin + j)) {
                continue;
            }
            match = false;
            break;
        }
        if (match) return i;
    }
    return -1;
}

size_t Memory::ExecuteSigScans() {
    size_t notFound = 0;
    for (const auto& [_, sigScan] : _sigScans) if (!sigScan.found) notFound++;

    MEMORY_BASIC_INFORMATION memoryInfo;
    __int64 baseAddress = 0;
    while (VirtualQueryEx(_handle, (void*)baseAddress, &memoryInfo, sizeof(memoryInfo))) {
        baseAddress = (__int64)memoryInfo.BaseAddress + memoryInfo.RegionSize;
        if (memoryInfo.State & MEM_FREE) continue;

        std::vector<byte> buff;
        buff.resize(memoryInfo.RegionSize);
        SIZE_T numBytesWritten;
        if (!ReadProcessMemory(_handle, (void*)((__int64)memoryInfo.BaseAddress), &buff[0], buff.size(), &numBytesWritten)) continue;
        buff.resize(numBytesWritten);

        for (auto& [scanBytes, sigScan] : _sigScans) {
            if (sigScan.found) continue;
            int index = find(buff, scanBytes);
            if (index == -1) continue;
            sigScan.scanFunc((__int64)memoryInfo.BaseAddress + index, buff);
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

__int64 Memory::GetModuleBaseAddress(const std::wstring& moduleName) {
    std::vector<HMODULE> modules;
    modules.resize(1024);
    DWORD sizeNeeded;
    EnumProcessModules(_handle, &modules[0], sizeof(HMODULE) * static_cast<DWORD>(modules.size()), &sizeNeeded);
    for (int i=0; i<sizeNeeded / sizeof(HMODULE); i++) {
        HMODULE module = modules[i];
        std::wstring fileName(256, L'\0');
        GetModuleFileNameExW(_handle, module, &fileName[0], static_cast<DWORD>(fileName.size()));
        if (fileName.find(moduleName) != std::wstring::npos) {
            MODULEINFO moduleInfo;
            GetModuleInformation(_handle, module, &moduleInfo, sizeof(moduleInfo));
            return (__int64)moduleInfo.lpBaseOfDll;
        }
    }
    return 0;
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
    std::vector<byte> jumpBack = {
        0x41, 0x53,                                 // push r11
        0x49, 0xBB, LONG_TO_BYTES(firstLine + 15),  // mov r11, firstLine + 15
        0x41, 0xFF, 0xE3,                           // jmp r11
    };

    std::vector<byte> injectionBytes = {0x41, 0x5B}; // pop r11 (before executing code that might need it)
    injectionBytes.insert(injectionBytes.end(), data.begin(), data.end());
    injectionBytes.push_back(0x90); // Padding nop
    std::vector<byte> replacedCode = ReadData<byte>({firstLine}, nextLine - firstLine);
    injectionBytes.insert(injectionBytes.end(), replacedCode.begin(), replacedCode.end());
    injectionBytes.push_back(0x90); // Padding nop
    injectionBytes.insert(injectionBytes.end(), jumpBack.begin(), jumpBack.end());

    __int64 addr = (__int64)VirtualAllocEx(_handle, NULL, injectionBytes.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    DebugPrint("Source address: " + DebugUtils::ToString(firstLine));
    DebugPrint("Injection address: " + DebugUtils::ToString(addr));
    WriteData<byte>(addr, injectionBytes);

    std::vector<byte> jumpAway = {
        0x41, 0x53,                         // push r11
        0x49, 0xBB, LONG_TO_BYTES(addr),    // mov r11, addr
        0x41, 0xFF, 0xE3,                   // jmp r11
        0x41, 0x5B,                         // pop r11 (we return to this opcode)
    };
    // We need enough space for the jump in the source code
    assert(static_cast<int>(nextLine - firstLine) >= jumpAway.size());

    // Fill any leftover space with nops
    for (size_t i=jumpAway.size(); i<static_cast<size_t>(nextLine - firstLine); i++) jumpAway.push_back(0x90);
    WriteData<byte>({firstLine}, jumpAway);

    _interceptions.emplace_back(firstLine, replacedCode, addr);
}

void Memory::Unintercept(__int64 firstLine, const std::vector<byte>& replacedCode, __int64 addr) {
    WriteData<byte>({firstLine}, replacedCode);
    if (addr != 0) VirtualFreeEx(_handle, (void*)addr, 0, MEM_RELEASE);
}

__int64 Memory::AllocateBuffer(size_t bufferSize) {
    void* addr = VirtualAllocEx(_handle, NULL, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    _allocations.emplace_back(addr);
    DebugPrint("Allocated buffer: " + DebugUtils::ToString((__int64)addr));
    return (__int64)addr;
}

void Memory::ReadDataInternal(uintptr_t addr, void* buffer, size_t bufferSize) {
    assert(bufferSize > 0);
    if (!_handle) return;
    // Ensure that the buffer size does not cause a read across a page boundary.
    MEMORY_BASIC_INFORMATION memoryInfo;
    VirtualQueryEx(_handle, (void*)addr, &memoryInfo, sizeof(memoryInfo));
    assert(!(memoryInfo.State & MEM_FREE));
    __int64 endOfPage = (__int64)memoryInfo.BaseAddress + memoryInfo.RegionSize;
    if (bufferSize > endOfPage - addr) {
        bufferSize = endOfPage - addr;
    }
    if (!ReadProcessMemory(_handle, (void*)addr, buffer, bufferSize, nullptr)) {
        DebugPrint("Failed to read process memory.");
        assert(false);
    }
}

void Memory::WriteDataInternal(uintptr_t addr, const void* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0) return;
    if (!_handle) return;
    if (!WriteProcessMemory(_handle, (void*)addr, buffer, bufferSize, nullptr)) {
        DebugPrint("Failed to write process memory.");
        assert(false);
    }
}

uintptr_t Memory::ComputeOffset(__int64 baseAddress, std::vector<__int64> offsets) {
    assert(offsets.size() > 0);
    assert(offsets.front() != 0);

    // Leave off the last offset, since it will be either read/write, and may not be of type uintptr_t.
    const __int64 final_offset = offsets.back();
    offsets.pop_back();

    uintptr_t cumulativeAddress = baseAddress;
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
