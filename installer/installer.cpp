#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <shellapi.h>

#include "theme_common.h"

namespace fs = std::filesystem;

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

std::wstring XmlEscape(const std::wstring& value) {
    std::wstring escaped;
    escaped.reserve(value.size());
    for (wchar_t ch : value) {
        switch (ch) {
        case L'&': escaped += L"&amp;"; break;
        case L'<': escaped += L"&lt;"; break;
        case L'>': escaped += L"&gt;"; break;
        case L'"': escaped += L"&quot;"; break;
        case L'\'': escaped += L"&apos;"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

bool WriteUtf16XmlFile(const fs::path& path, const std::wstring& content) {
    static_assert(sizeof(wchar_t) == 2, "This writer expects Windows UTF-16 wchar_t.");

    std::ofstream outFile(path, std::ios::binary);
    if (!outFile.is_open()) {
        return false;
    }

    const unsigned char bom[] = { 0xFF, 0xFE };
    outFile.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    outFile.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size() * sizeof(wchar_t)));
    return outFile.good();
}

std::wstring GenerateTaskXml(const fs::path& targetExe, const std::wstring& author, const std::wstring& description,
    const std::wstring& triggers, const std::wstring& settings, const SwitchTimes& times) {
    std::wstring exePathXml = XmlEscape(targetExe.wstring());
    std::wstring lightStart = times.lightStartText;
    std::wstring darkStart = times.darkStartText;
    std::wstring arguments = XmlEscape(lightStart + L" " + darkStart);

    return LR"(<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Date>2023-01-01T00:00:00</Date>
    <Author>)" + author + LR"(</Author>
    <Description>)" + description + LR"(</Description>
  </RegistrationInfo>
  
)" + triggers + LR"(

  <Principals>
    <Principal id="Author">
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>LeastPrivilege</RunLevel>
    </Principal>
  </Principals>

)" + settings + LR"(

  <Actions Context="Author">
    <Exec>
      <Command>)" + exePathXml + LR"(</Command>
      <Arguments>)" + arguments + LR"(</Arguments>
    </Exec>
  </Actions>
</Task>
)";
}

std::wstring GenerateImmediateTaskXml(const fs::path& targetExe, const std::wstring& author, const SwitchTimes& times) {
    std::wstring description = L"自动切换 Windows 深色/浅色模式 (登录/解锁/唤醒即时检测)";
    std::wstring triggers = LR"(  <Triggers>
    <!-- 1. 登录时 (开机) -->
    <LogonTrigger>
      <Enabled>true</Enabled>
      <Delay>PT10S</Delay> <!-- 延迟10秒，等系统稳定 -->
    </LogonTrigger>

    <!-- 2. 工作站解锁 -->
    <SessionStateChangeTrigger>
      <StateChange>SessionUnlock</StateChange>
      <Enabled>true</Enabled>
    </SessionStateChangeTrigger>

    <!-- 3. 监听系统底层唤醒事件 (Event ID 1 & 107) -->
    <EventTrigger>
      <Enabled>true</Enabled>
      <Subscription>&lt;QueryList&gt;&lt;Query Id="0" Path="System"&gt;&lt;Select Path="System"&gt;*[System[Provider[@Name='Microsoft-Windows-Power-Troubleshooter'] and EventID=1]] or *[System[Provider[@Name='Microsoft-Windows-Kernel-Power'] and EventID=107]]&lt;/Select&gt;&lt;/Query&gt;&lt;/QueryList&gt;</Subscription>
    </EventTrigger>
  </Triggers>)";

    std::wstring settings = LR"(  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <AllowHardTerminate>true</AllowHardTerminate>
    <StartWhenAvailable>true</StartWhenAvailable>
    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>
    <IdleSettings>
      <StopOnIdleEnd>false</StopOnIdleEnd>
      <RestartOnIdle>false</RestartOnIdle>
    </IdleSettings>
    <AllowStartOnDemand>true</AllowStartOnDemand>
    <Enabled>true</Enabled>
    <Hidden>false</Hidden>
    <RunOnlyIfIdle>false</RunOnlyIfIdle>
    <WakeToRun>false</WakeToRun>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>7</Priority>
  </Settings>)";

    return GenerateTaskXml(targetExe, author, description, triggers, settings, times);
}

std::wstring GenerateScheduledTaskXml(const fs::path& targetExe, const std::wstring& author, const SwitchTimes& times) {
    std::wstring lightStart = times.lightStartText;
    std::wstring darkStart = times.darkStartText;
    std::wstring description = L"自动切换 Windows 深色/浅色模式 (" + lightStart + L"/" + darkStart + L" 定时 + 空闲后执行)";

    std::wstring triggers = LR"(  <Triggers>
    <!-- 1. 每天浅色开始时间执行 -->
    <CalendarTrigger>
      <StartBoundary>2023-01-01T)" + lightStart + LR"(:00</StartBoundary>
      <Enabled>true</Enabled>
      <ScheduleByDay>
        <DaysInterval>1</DaysInterval>
      </ScheduleByDay>
    </CalendarTrigger>

    <!-- 2. 每天深色开始时间执行 -->
    <CalendarTrigger>
      <StartBoundary>2023-01-01T)" + darkStart + LR"(:00</StartBoundary>
      <Enabled>true</Enabled>
      <ScheduleByDay>
        <DaysInterval>1</DaysInterval>
      </ScheduleByDay>
    </CalendarTrigger>
  </Triggers>)";

    std::wstring settings = LR"(  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <AllowHardTerminate>true</AllowHardTerminate>
    <StartWhenAvailable>true</StartWhenAvailable>
    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>
    <IdleSettings>
      <Duration>PT1M</Duration>
      <WaitTimeout>PT3H</WaitTimeout>
      <StopOnIdleEnd>false</StopOnIdleEnd>
      <RestartOnIdle>false</RestartOnIdle>
    </IdleSettings>
    <AllowStartOnDemand>true</AllowStartOnDemand>
    <Enabled>true</Enabled>
    <Hidden>false</Hidden>
    <RunOnlyIfIdle>true</RunOnlyIfIdle>
    <WakeToRun>false</WakeToRun>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Priority>7</Priority>
  </Settings>)";

    return GenerateTaskXml(targetExe, author, description, triggers, settings, times);
}

bool GetCurrentUserName(std::wstring& userName) {
    wchar_t buffer[256];
    DWORD bufferLength = 256;
    if (!GetUserNameW(buffer, &bufferLength)) {
        return false;
    }

    userName = buffer;
    return true;
}

bool RegisterScheduledTask(const std::wstring& taskName, const fs::path& xmlPath) {
    std::wstring command = L"schtasks /Create /TN \"" + taskName + L"\" /XML \"" + xmlPath.wstring() + L"\" /F";
    return _wsystem(command.c_str()) == 0;
}

bool RegisterTaskFromXml(const fs::path& workingDir, const std::wstring& taskName, const std::wstring& xmlFileName, const std::wstring& xmlContent) {
    fs::path xmlPath = workingDir / xmlFileName;
    if (!WriteUtf16XmlFile(xmlPath, xmlContent)) {
        ConsoleWriteLine(L"无法创建临时 XML 文件：" + xmlPath.wstring());
        return false;
    }

    bool registered = RegisterScheduledTask(taskName, xmlPath);

    try { fs::remove(xmlPath); }
    catch (...) {}

    return registered;
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
    fs::path targetExe = currentPath.parent_path() / L"AutoThemeSwitcher.exe";

    if (!fs::exists(targetExe)) {
        ConsoleWriteLine(L"错误：找不到 " + targetExe.wstring());
        ConsoleWriteLine(L"请确保 installer.exe 和 AutoThemeSwitcher.exe 在同一目录下。");
        system("pause");
        return 1;
    }

    std::wstring userName;
    if (!GetCurrentUserName(userName)) {
        ConsoleWriteLine(L"无法获取当前用户名。");
        system("pause");
        return 1;
    }

    fs::path workingDir = currentPath.parent_path();
    std::wstring author = XmlEscape(userName);
    std::wstring immediateTaskXml = GenerateImmediateTaskXml(targetExe, author, times);
    std::wstring scheduledTaskXml = GenerateScheduledTaskXml(targetExe, author, times);

    ConsoleWriteLine(L"正在注册计划任务...");
    ConsoleWriteLine(L"浅色开始时间：" + times.lightStartText + L"，深色开始时间：" + times.darkStartText);

    bool immediateRegistered = RegisterTaskFromXml(workingDir, IMMEDIATE_TASK_NAME, L"AutoTheme_Immediate.xml", immediateTaskXml);
    bool scheduledRegistered = RegisterTaskFromXml(workingDir, SCHEDULED_TASK_NAME, L"AutoTheme_Scheduled.xml", scheduledTaskXml);

    if (immediateRegistered && scheduledRegistered) {
        ConsoleWriteLine(L"\n成功！两个任务均已更新。");
        ConsoleWriteLine(L"登录/解锁/唤醒会立即检测，定时任务会等待系统空闲后执行。");
    }
    else {
        ConsoleWriteLine(L"\n失败。请确认已允许管理员权限后重试。");
        if (!immediateRegistered) {
            ConsoleWriteLine(L"- 即时触发任务注册失败：" + std::wstring(IMMEDIATE_TASK_NAME));
        }
        if (!scheduledRegistered) {
            ConsoleWriteLine(L"- 定时空闲任务注册失败：" + std::wstring(SCHEDULED_TASK_NAME));
        }
    }

    system("pause");
    return (immediateRegistered && scheduledRegistered) ? 0 : 1;
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
