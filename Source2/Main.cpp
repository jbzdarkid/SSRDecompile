#include "Memory.h"
#include "InputBuffer.h"

#include <memory>
#include <string>
#include <thread>
#include <iostream>

int main() {
    std::shared_ptr<Memory> g_memory;
    std::shared_ptr<InputBuffer> g_inputBuffer;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!g_memory) g_memory = Memory::Create(L"Sausage.exe");
        if (!g_memory) continue;
        if (!g_inputBuffer) g_inputBuffer = InputBuffer::Create(g_memory);
        // TODO: g_inputBuffer->AttemptSigscans();
        if (!g_inputBuffer) continue;

        std::cout << "All loaded\n";
        std::cout << "(0) Quit\n";
        std::cout << "(1) [Re]load master demo\n";
        std::cout << "(2) Play\n";
        std::cout << "(3) Pause\n";
        std::string a;
        std::cin >> a;
        if (a == "0") {
            break;
        } else if (a == "1") {
            g_inputBuffer->ReadFromFile(ALL_DEM);
        } else if (a == "2") {
            g_inputBuffer->SetMode(Mode::Playing);
        } else if (a == "3") {
            g_inputBuffer->SetMode(Mode::Recording);
        } else {
            std::cout << "Unknown selection: " << a << std::endl;
        }
    }

    return 0;
}
