#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <codecvt>

namespace fs = std::filesystem;

// 辅助：将 wstring 转换为 string
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

int main() {
    // 1. 获取路径
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    fs::path currentPath = buffer;
    fs::path targetExe = currentPath.parent_path() / L"AutoThemeSwitcher.exe";

    if (!fs::exists(targetExe)) {
        std::wcout << L"错误：找不到 " << targetExe.c_str() << std::endl;
        std::wcout << L"请确保 Installer.exe 和 AutoThemeSwitcher.exe 在同一目录下。" << std::endl;
        system("pause");
        return 1;
    }

    std::string exePathUtf8 = WStringToString(targetExe.wstring());

    // 获取当前用户名
    wchar_t username[256];
    DWORD usernameLen = 256;
    GetUserNameW(username, &usernameLen);
    std::string author = WStringToString(username);

    // 2. 准备 XML (更新了 Triggers 部分)
    std::string xmlContent = R"(<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Date>2023-01-01T00:00:00</Date>
    <Author>)" + author + R"(</Author>
    <Description>自动切换 Windows 深色/浅色模式 (07:00/17:00 定时 + 开机/唤醒/解锁检测)</Description>
  </RegistrationInfo>
  
  <Triggers>
    <!-- 1. 登录时 (开机) -->
    <LogonTrigger>
      <Enabled>true</Enabled>
    </LogonTrigger>

    <!-- 2. 每天早上 07:00 执行 (切浅色) -->
    <CalendarTrigger>
      <StartBoundary>2023-01-01T07:00:00</StartBoundary>
      <Enabled>true</Enabled>
      <ScheduleByDay>
        <DaysInterval>1</DaysInterval>
      </ScheduleByDay>
    </CalendarTrigger>

    <!-- 3. 每天下午 17:00 执行 (切深色) -->
    <CalendarTrigger>
      <StartBoundary>2023-01-01T17:00:00</StartBoundary>
      <Enabled>true</Enabled>
      <ScheduleByDay>
        <DaysInterval>1</DaysInterval>
      </ScheduleByDay>
    </CalendarTrigger>

    <!-- 4. 工作站解锁时 (防止待机错过时间点) -->
    <SessionStateChangeTrigger>
      <StateChange>SessionUnlock</StateChange>
      <Enabled>true</Enabled>
    </SessionStateChangeTrigger>

    <!-- 5. 系统从睡眠唤醒时 (精准补漏) -->
    <EventTrigger>
      <Enabled>true</Enabled>
      <Subscription>&lt;QueryList&gt;&lt;Query Id="0" Path="System"&gt;&lt;Select Path="System"&gt;*[System[Provider[@Name='Microsoft-Windows-Power-Troubleshooter'] and EventID=1]]&lt;/Select&gt;&lt;/Query&gt;&lt;/QueryList&gt;</Subscription>
    </EventTrigger>
  </Triggers>

  <Principals>
    <Principal id="Author">
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>LeastPrivilege</RunLevel>
    </Principal>
  </Principals>

  <Settings>
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
  </Settings>

  <Actions Context="Author">
    <Exec>
      <Command>)" + exePathUtf8 + R"(</Command>
    </Exec>
  </Actions>
</Task>
)";

    // 3. 写入 XML
    fs::path xmlPath = currentPath.parent_path() / L"AutoTheme_Task.xml";
    std::ofstream outFile(xmlPath);
    if (!outFile.is_open()) {
        std::wcout << L"无法创建临时 XML 文件。" << std::endl;
        return 1;
    }
    outFile << xmlContent;
    outFile.close();

    std::wcout << L"正在注册精准定时任务 (07:00 & 17:00)..." << std::endl;

    // 4. 执行 schtasks
    std::wstring command = L"schtasks /Create /TN \"AutoThemeSwitcher\" /XML \"" + xmlPath.wstring() + L"\" /F";
    int result = _wsystem(command.c_str());

    // 5. 清理
    try { fs::remove(xmlPath); }
    catch (...) {}

    if (result == 0) {
        std::wcout << L"\n成功！任务已更新。" << std::endl;
        std::wcout << L"触发条件：每天07:00 | 每天17:00 | 开机 | 唤醒 | 解锁" << std::endl;
    }
    else {
        std::wcout << L"\n失败。请右键选择 [以管理员身份运行] 重试。" << std::endl;
    }

    system("pause");
    return 0;
}