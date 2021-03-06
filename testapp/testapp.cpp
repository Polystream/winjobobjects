// just a little test app that launches notepad and never exits
#include "stdafx.h"
#include <windows.h>
#include <iostream>

// your mileage could vary; perhaps you've installed it on your D drive, just for fun
auto NOTEPAD_EXE_PATH = TEXT("C:\\Windows\\Notepad.exe");
auto NOTEPAD_WORK_DIR = nullptr;

auto EXE_PATH = NOTEPAD_EXE_PATH;
auto WORK_DIR = NOTEPAD_WORK_DIR;

int main()
{
    std::cout << "Hello, this is the testapp process, we'll launch Notepad.exe\n";
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFO si = { 0 };
    auto handle = CreateProcess(EXE_PATH, nullptr, nullptr, nullptr, FALSE, 0, nullptr, WORK_DIR, &si, &pi);
    if (handle)
    {
        std::cout << "We are doing something...for ever...\n";
        while (true)
        {
            Sleep(1000);
            std::cout << "...and ever...\n";
        }
    }
    return 0;
}



