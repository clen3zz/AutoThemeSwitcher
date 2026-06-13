#pragma once

#include <filesystem>
#include <string>
#include <windows.h>

#include "theme_common.h"

inline constexpr const wchar_t* IMMEDIATE_TASK_NAME = L"AutoThemeSwitcher";
inline constexpr const wchar_t* SCHEDULED_TASK_NAME = L"AutoThemeSwitcher_Scheduled";
inline constexpr const wchar_t* SOLAR_REFRESH_TASK_NAME = L"AutoThemeSwitcher_SolarRefresh";

struct TaskStatus {
    bool immediateExists = false;
    bool scheduledExists = false;
    bool solarRefreshExists = false;
    bool queryOk = false;
};

bool IsRunAsAdmin();
std::wstring BuildArgumentString(int argc, wchar_t* argv[]);
bool RelaunchAsAdmin(int argc, wchar_t* argv[]);
bool RunProcessHidden(const std::wstring& commandLine, DWORD* exitCode = nullptr);
bool RegisterAutoThemeTasks(const std::filesystem::path& targetExe, const std::filesystem::path& workingDir, const SwitchTimes& times, std::wstring& errorMessage);
bool RegisterSolarRefreshTask(const std::filesystem::path& guiExe, const std::filesystem::path& workingDir, double latitude, double longitude, std::wstring& errorMessage);
bool DeleteAutoThemeTasks(std::wstring& errorMessage);
bool DeleteSolarRefreshTask(std::wstring& errorMessage);
TaskStatus QueryAutoThemeTaskStatus();
bool ReadInstalledSwitchTimes(SwitchTimes& times);
bool ReadInstalledSolarCoordinates(double& latitude, double& longitude);
std::wstring QuoteArgument(const std::wstring& value);
