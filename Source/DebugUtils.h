#pragma once
#include <string>

class DebugUtils final
{
public:
    static void RegenerateCallstack(const std::wstring& callstack);
    static void ShowAssertDialogue();
    static std::wstring version;
    static uint64_t GetBaseAddress(HANDLE process);

private:
    static std::wstring GetStackTrace();
};

void DebugPrint(const std::string& text);
void DebugPrint(const std::wstring& text);
inline void ShowAssertDialogue() {
    DebugUtils::ShowAssertDialogue();
}
