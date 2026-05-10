#include <windows.h>
#include <iostream>
#include <string>
#include <shellapi.h>

const wchar_t* IMMEDIATE_TASK_NAME = L"AutoThemeSwitcher";
const wchar_t* SCHEDULED_TASK_NAME = L"AutoThemeSwitcher_Scheduled";

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

std::wstring QuoteArgument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L"\\\"";
        }
        else {
            quoted += ch;
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring BuildArgumentString(int argc, wchar_t* argv[]) {
    std::wstring arguments;
    for (int i = 1; i < argc; ++i) {
        if (!arguments.empty()) {
            arguments += L" ";
        }
        arguments += QuoteArgument(argv[i]);
    }
    return arguments;
}

bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        CheckTokenMembership(nullptr, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    }

    return isAdmin == TRUE;
}

bool RelaunchAsAdmin(int argc, wchar_t* argv[]) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) {
        return false;
    }

    std::wstring arguments = BuildArgumentString(argc, argv);
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.lpVerb = L"runas";
    info.lpFile = exePath;
    info.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
    info.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&info) == TRUE;
}

bool RunHiddenCommand(const std::wstring& command) {
    return _wsystem(command.c_str()) == 0;
}

bool TaskExists(const std::wstring& taskName) {
    std::wstring command = L"schtasks /Query /TN \"" + taskName + L"\" >nul 2>&1";
    return RunHiddenCommand(command);
}

bool DeleteTask(const std::wstring& taskName) {
    std::wstring command = L"schtasks /Delete /TN \"" + taskName + L"\" /F >nul 2>&1";
    return RunHiddenCommand(command);
}

bool DeleteTaskIfExists(const std::wstring& taskName) {
    if (!TaskExists(taskName)) {
        ConsoleWriteLine(L"未找到任务，已跳过：" + taskName);
        return true;
    }

    if (DeleteTask(taskName)) {
        ConsoleWriteLine(L"已删除任务：" + taskName);
        return true;
    }

    ConsoleWriteLine(L"删除失败：" + taskName);
    return false;
}

int RunUninstaller() {
    ConsoleWriteLine(L"AutoThemeSwitcher 卸载器");
    ConsoleWriteLine(L"正在删除计划任务...");
    ConsoleWriteLine();

    bool immediateDeleted = DeleteTaskIfExists(IMMEDIATE_TASK_NAME);
    bool scheduledDeleted = DeleteTaskIfExists(SCHEDULED_TASK_NAME);

    ConsoleWriteLine();
    if (immediateDeleted && scheduledDeleted) {
        ConsoleWriteLine(L"卸载完成。程序文件不会被删除，当前主题也不会被修改。");
    }
    else {
        ConsoleWriteLine(L"部分任务删除失败，请确认已允许管理员权限后重试。");
    }

    system("pause");
    return (immediateDeleted && scheduledDeleted) ? 0 : 1;
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
