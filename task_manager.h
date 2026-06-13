#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

#include "theme_common.h"

inline constexpr const wchar_t* IMMEDIATE_TASK_NAME = L"AutoThemeSwitcher";
inline constexpr const wchar_t* SCHEDULED_TASK_NAME = L"AutoThemeSwitcher_Scheduled";

struct TaskStatus {
    bool immediateExists = false;
    bool scheduledExists = false;
    bool queryOk = false;
};

bool IsRunAsAdmin();
std::wstring BuildArgumentString(int argc, wchar_t* argv[]);
bool RelaunchAsAdmin(int argc, wchar_t* argv[]);
bool RunProcessHidden(const std::wstring& commandLine, DWORD* exitCode = nullptr);
bool RegisterAutoThemeTasks(const std::filesystem::path& targetExe, const std::filesystem::path& workingDir, const SwitchTimes& times, std::wstring& errorMessage);
bool DeleteAutoThemeTasks(std::wstring& errorMessage);
TaskStatus QueryAutoThemeTaskStatus();
bool ReadInstalledSwitchTimes(SwitchTimes& times);
std::wstring QuoteArgument(const std::wstring& value);
