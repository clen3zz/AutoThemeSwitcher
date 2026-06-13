#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "task_manager.h"
#include "theme_common.h"

namespace fs = std::filesystem;

void ConsoleWrite(const std::wstring& text) {
    DWORD written = 0;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != INVALID_HANDLE_VALUE && GetFileType(output) == FILE_TYPE_CHAR) {
        WriteConsoleW(output, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
        return;
    }

    std::wcout << text;
}

void ConsoleWriteLine(const std::wstring& text = L"") {
    ConsoleWrite(text + L"\n");
}

void PrintUsage() {
    ConsoleWriteLine(L"用法：");
    ConsoleWriteLine(L"  installer.exe");
    ConsoleWriteLine(L"  installer.exe <浅色开始时间> <深色开始时间>");
    ConsoleWriteLine(L"示例：");
    ConsoleWriteLine(L"  installer.exe 07:00 17:00");
    ConsoleWriteLine(L"时间必须使用 24 小时制 HH:MM，并补零。");
}

std::wstring PromptTime(const wchar_t* label, const std::wstring& defaultValue) {
    while (true) {
        ConsoleWrite(std::wstring(label) + L" [" + defaultValue + L"]: ");

        std::wstring input;
        std::getline(std::wcin, input);
        if (input.empty()) {
            input = defaultValue;
        }

        int minutes = 0;
        if (ParseTime(input, minutes)) {
            return input;
        }

        ConsoleWriteLine(L"时间格式不正确，请使用 24 小时制 HH:MM，例如 07:00。");
    }
}

void PromptSwitchTimes(SwitchTimes& times) {
    ConsoleWriteLine(L"AutoThemeSwitcher 安装器");
    ConsoleWriteLine(L"请输入每天切换浅色/深色模式的时间；直接回车使用默认值。");
    ConsoleWriteLine();

    while (true) {
        std::wstring lightStart = PromptTime(L"浅色开始时间", times.lightStartText);
        std::wstring darkStart = PromptTime(L"深色开始时间", times.darkStartText);

        if (MakeSwitchTimes(lightStart, darkStart, times)) {
            return;
        }

        ConsoleWriteLine(L"浅色和深色开始时间不能相同，请重新输入。");
        ConsoleWriteLine();
    }
}

bool ReadSwitchTimes(int argc, wchar_t* argv[], SwitchTimes& times) {
    if (argc == 1) {
        PromptSwitchTimes(times);
        return true;
    }
    if (argc != 3) {
        PrintUsage();
        return false;
    }

    if (!MakeSwitchTimes(argv[1], argv[2], times)) {
        PrintUsage();
        return false;
    }

    return true;
}

int RunInstaller(int argc, wchar_t* argv[]) {
    SwitchTimes times;
    if (!ReadSwitchTimes(argc, argv, times)) {
        system("pause");
        return 1;
    }

    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    fs::path currentPath = buffer;
    fs::path workingDir = currentPath.parent_path();
    fs::path targetExe = workingDir / L"AutoThemeSwitcher.exe";

    ConsoleWriteLine(L"正在注册计划任务...");
    ConsoleWriteLine(L"浅色开始时间：" + times.lightStartText + L"，深色开始时间：" + times.darkStartText);

    std::wstring errorMessage;
    bool installed = RegisterAutoThemeTasks(targetExe, workingDir, times, errorMessage);

    if (installed) {
        ConsoleWriteLine(L"\n成功！两个任务均已更新。");
        ConsoleWriteLine(L"登录/解锁/唤醒会立即检测，定时任务会等待系统空闲后执行。");
    }
    else {
        ConsoleWriteLine(L"\n失败。请确认已允许管理员权限后重试。");
        ConsoleWriteLine(errorMessage);
    }

    system("pause");
    return installed ? 0 : 1;
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (!IsRunAsAdmin()) {
        ConsoleWriteLine(L"安装计划任务需要管理员权限，正在请求管理员权限...");
        if (RelaunchAsAdmin(argc, argv)) {
            return 0;
        }

        ConsoleWriteLine(L"无法获取管理员权限，安装已取消。");
        system("pause");
        return 1;
    }

    return RunInstaller(argc, argv);
}
