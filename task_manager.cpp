#include "task_manager.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <windows.h>
#include <shellapi.h>

namespace fs = std::filesystem;

namespace {

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

std::wstring XmlUnescape(std::wstring value) {
    struct Replacement {
        const wchar_t* from;
        const wchar_t* to;
    };
    const Replacement replacements[] = {
        { L"&quot;", L"\"" },
        { L"&apos;", L"'" },
        { L"&lt;", L"<" },
        { L"&gt;", L">" },
        { L"&amp;", L"&" },
    };

    for (const auto& replacement : replacements) {
        size_t pos = 0;
        while ((pos = value.find(replacement.from, pos)) != std::wstring::npos) {
            value.replace(pos, wcslen(replacement.from), replacement.to);
            pos += wcslen(replacement.to);
        }
    }
    return value;
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

bool ReadFileBinary(const fs::path& path, std::vector<unsigned char>& bytes) {
    std::ifstream inFile(path, std::ios::binary);
    if (!inFile.is_open()) {
        return false;
    }

    bytes.assign(std::istreambuf_iterator<char>(inFile), std::istreambuf_iterator<char>());
    return true;
}

std::wstring DecodeFileText(const std::vector<unsigned char>& bytes) {
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        std::wstring text;
        size_t wcharCount = (bytes.size() - 2) / sizeof(wchar_t);
        text.resize(wcharCount);
        memcpy(text.data(), bytes.data() + 2, wcharCount * sizeof(wchar_t));
        return text;
    }

    if (bytes.empty()) {
        return L"";
    }

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), nullptr, 0);
    if (sizeNeeded <= 0) {
        sizeNeeded = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), nullptr, 0);
        if (sizeNeeded <= 0) {
            return L"";
        }
        std::wstring text(sizeNeeded, L'\0');
        MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), text.data(), sizeNeeded);
        return text;
    }

    std::wstring text(sizeNeeded, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), text.data(), sizeNeeded);
    return text;
}

std::wstring GenerateTaskXml(const fs::path& targetExe, const std::wstring& author, const std::wstring& description,
    const std::wstring& triggers, const std::wstring& settings, const SwitchTimes& times) {
    std::wstring exePathXml = XmlEscape(targetExe.wstring());
    std::wstring arguments = XmlEscape(times.lightStartText + L" " + times.darkStartText);

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
    std::wstring description = L"自动切换 Windows 深色/浅色模式 (" + times.lightStartText + L"/" + times.darkStartText + L" 定时 + 空闲后执行)";

    std::wstring triggers = LR"(  <Triggers>
    <!-- 1. 每天浅色开始时间执行 -->
    <CalendarTrigger>
      <StartBoundary>2023-01-01T)" + times.lightStartText + LR"(:00</StartBoundary>
      <Enabled>true</Enabled>
      <ScheduleByDay>
        <DaysInterval>1</DaysInterval>
      </ScheduleByDay>
    </CalendarTrigger>

    <!-- 2. 每天深色开始时间执行 -->
    <CalendarTrigger>
      <StartBoundary>2023-01-01T)" + times.darkStartText + LR"(:00</StartBoundary>
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

bool RegisterTaskFromXml(const fs::path& workingDir, const std::wstring& taskName, const std::wstring& xmlFileName, const std::wstring& xmlContent) {
    fs::path xmlPath = workingDir / xmlFileName;
    if (!WriteUtf16XmlFile(xmlPath, xmlContent)) {
        return false;
    }

    std::wstring command = L"schtasks /Create /TN " + QuoteArgument(taskName) + L" /XML " + QuoteArgument(xmlPath.wstring()) + L" /F";
    DWORD exitCode = 1;
    bool ran = RunProcessHidden(command, &exitCode);

    try { fs::remove(xmlPath); }
    catch (...) {}

    return ran && exitCode == 0;
}

bool TaskExists(const std::wstring& taskName) {
    std::wstring command = L"schtasks /Query /TN " + QuoteArgument(taskName);
    DWORD exitCode = 1;
    return RunProcessHidden(command, &exitCode) && exitCode == 0;
}

bool DeleteTask(const std::wstring& taskName) {
    std::wstring command = L"schtasks /Delete /TN " + QuoteArgument(taskName) + L" /F";
    DWORD exitCode = 1;
    return RunProcessHidden(command, &exitCode) && exitCode == 0;
}

bool QueryTaskXml(const std::wstring& taskName, std::wstring& xml) {
    wchar_t tempDir[MAX_PATH];
    wchar_t tempFile[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempDir) == 0 || GetTempFileNameW(tempDir, L"ats", 0, tempFile) == 0) {
        return false;
    }

    fs::path xmlPath = tempFile;
    std::wstring command = L"cmd.exe /C schtasks /Query /TN " + QuoteArgument(taskName) + L" /XML > " + QuoteArgument(xmlPath.wstring()) + L" 2>nul";
    DWORD exitCode = 1;
    bool ran = RunProcessHidden(command, &exitCode);
    if (!ran || exitCode != 0) {
        try { fs::remove(xmlPath); }
        catch (...) {}
        return false;
    }

    std::vector<unsigned char> bytes;
    bool read = ReadFileBinary(xmlPath, bytes);
    try { fs::remove(xmlPath); }
    catch (...) {}
    if (!read) {
        return false;
    }

    xml = DecodeFileText(bytes);
    return !xml.empty();
}

bool ExtractArguments(const std::wstring& xml, std::wstring& arguments) {
    size_t start = xml.find(L"<Arguments>");
    size_t end = xml.find(L"</Arguments>");
    if (start == std::wstring::npos || end == std::wstring::npos || end <= start) {
        return false;
    }

    start += wcslen(L"<Arguments>");
    arguments = XmlUnescape(xml.substr(start, end - start));
    return true;
}

} // namespace

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

bool RunProcessHidden(const std::wstring& commandLine, DWORD* exitCode) {
    std::wstring mutableCommand = commandLine;
    STARTUPINFOW startupInfo = {};
    PROCESS_INFORMATION processInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo)) {
        return false;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(processInfo.hProcess, &code);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    if (exitCode) {
        *exitCode = code;
    }
    return true;
}

bool RegisterAutoThemeTasks(const fs::path& targetExe, const fs::path& workingDir, const SwitchTimes& times, std::wstring& errorMessage) {
    if (!fs::exists(targetExe)) {
        errorMessage = L"找不到 AutoThemeSwitcher.exe：" + targetExe.wstring();
        return false;
    }

    std::wstring userName;
    if (!GetCurrentUserName(userName)) {
        errorMessage = L"无法获取当前用户名。";
        return false;
    }

    std::wstring author = XmlEscape(userName);
    std::wstring immediateTaskXml = GenerateImmediateTaskXml(targetExe, author, times);
    std::wstring scheduledTaskXml = GenerateScheduledTaskXml(targetExe, author, times);

    bool immediateRegistered = RegisterTaskFromXml(workingDir, IMMEDIATE_TASK_NAME, L"AutoTheme_Immediate.xml", immediateTaskXml);
    bool scheduledRegistered = RegisterTaskFromXml(workingDir, SCHEDULED_TASK_NAME, L"AutoTheme_Scheduled.xml", scheduledTaskXml);

    if (!immediateRegistered || !scheduledRegistered) {
        errorMessage = L"计划任务注册失败。";
        if (!immediateRegistered) {
            errorMessage += L" 即时触发任务失败。";
        }
        if (!scheduledRegistered) {
            errorMessage += L" 定时空闲任务失败。";
        }
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DeleteAutoThemeTasks(std::wstring& errorMessage) {
    bool immediateDeleted = !TaskExists(IMMEDIATE_TASK_NAME) || DeleteTask(IMMEDIATE_TASK_NAME);
    bool scheduledDeleted = !TaskExists(SCHEDULED_TASK_NAME) || DeleteTask(SCHEDULED_TASK_NAME);

    if (!immediateDeleted || !scheduledDeleted) {
        errorMessage = L"部分计划任务删除失败。";
        if (!immediateDeleted) {
            errorMessage += L" 即时触发任务失败。";
        }
        if (!scheduledDeleted) {
            errorMessage += L" 定时空闲任务失败。";
        }
        return false;
    }

    errorMessage.clear();
    return true;
}

TaskStatus QueryAutoThemeTaskStatus() {
    TaskStatus status;
    status.immediateExists = TaskExists(IMMEDIATE_TASK_NAME);
    status.scheduledExists = TaskExists(SCHEDULED_TASK_NAME);
    status.queryOk = true;
    return status;
}

bool ReadInstalledSwitchTimes(SwitchTimes& times) {
    std::wstring xml;
    if (!QueryTaskXml(SCHEDULED_TASK_NAME, xml) && !QueryTaskXml(IMMEDIATE_TASK_NAME, xml)) {
        return false;
    }

    std::wstring arguments;
    if (!ExtractArguments(xml, arguments)) {
        return false;
    }

    std::wistringstream stream(arguments);
    std::wstring lightStart;
    std::wstring darkStart;
    stream >> lightStart >> darkStart;
    if (lightStart.empty() || darkStart.empty()) {
        return false;
    }

    return MakeSwitchTimes(lightStart, darkStart, times);
}
