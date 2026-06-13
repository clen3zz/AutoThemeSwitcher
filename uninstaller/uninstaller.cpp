#include <windows.h>

#include <iostream>
#include <string>

#include "task_manager.h"

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

int RunUninstaller() {
    ConsoleWriteLine(L"AutoThemeSwitcher 卸载器");
    ConsoleWriteLine(L"正在删除计划任务...");
    ConsoleWriteLine();

    std::wstring errorMessage;
    bool uninstalled = DeleteAutoThemeTasks(errorMessage);

    if (uninstalled) {
        ConsoleWriteLine(L"卸载完成。程序文件不会被删除，当前主题也不会被修改。");
    }
    else {
        ConsoleWriteLine(L"部分任务删除失败，请确认已允许管理员权限后重试。");
        ConsoleWriteLine(errorMessage);
    }

    system("pause");
    return uninstalled ? 0 : 1;
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (!IsRunAsAdmin()) {
        ConsoleWriteLine(L"删除计划任务需要管理员权限，正在请求管理员权限...");
        if (RelaunchAsAdmin(argc, argv)) {
            return 0;
        }

        ConsoleWriteLine(L"无法获取管理员权限，卸载已取消。");
        system("pause");
        return 1;
    }

    return RunUninstaller();
}
