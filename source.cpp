#include <windows.h>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

#include "theme_common.h"

const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

std::vector<std::wstring> SplitCommandLine(PWSTR pCmdLine) {
    std::wistringstream stream(pCmdLine ? pCmdLine : L"");
    std::vector<std::wstring> args;
    std::wstring arg;
    while (stream >> arg) {
        args.push_back(arg);
    }

    return args;
}

SwitchTimes ReadSwitchTimes(const std::vector<std::wstring>& args) {
    SwitchTimes times;
    if (args.size() != 2) {
        return times;
    }

    MakeSwitchTimes(args[0], args[1], times);
    return times;
}

bool SetRegistryValue(HKEY hKey, const wchar_t* valueName, DWORD data) {
    return RegSetValueExW(hKey, valueName, 0, REG_DWORD, (const BYTE*)&data, sizeof(data)) == ERROR_SUCCESS;
}

bool GetRegistryValue(HKEY hKey, const wchar_t* valueName, DWORD& outValue) {
    DWORD dataSize = sizeof(outValue);
    return RegQueryValueExW(hKey, valueName, nullptr, nullptr, (LPBYTE)&outValue, &dataSize) == ERROR_SUCCESS;
}

void ResetColorPrevalence(HKEY hKey) {
    DWORD value = 0;
    SetRegistryValue(hKey, L"ColorPrevalence", value);

    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_DWMCOLORIZATIONCOLORCHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
}

void SetSystemTheme(HKEY hKey, bool isLight) {
    DWORD value = isLight ? 1 : 0;
    SetRegistryValue(hKey, L"SystemUsesLightTheme", value);

    if (isLight) {
        ResetColorPrevalence(hKey);
    }

    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
}

void SetAppsTheme(HKEY hKey, bool isLight) {
    DWORD value = isLight ? 1 : 0;
    SetRegistryValue(hKey, L"AppsUseLightTheme", value);

    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)nCmdShow;

    std::vector<std::wstring> args = SplitCommandLine(pCmdLine);
    SwitchTimes times = ReadSwitchTimes(args);

    time_t t = time(nullptr);
    tm now;
    localtime_s(&now, &t);
    int currentMinutes = now.tm_hour * 60 + now.tm_min;

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return 1;
    }

    DWORD currentApp = 0, currentSys = 0;
    GetRegistryValue(hKey, L"AppsUseLightTheme", currentApp);
    GetRegistryValue(hKey, L"SystemUsesLightTheme", currentSys);

    bool isSystemLight = (currentSys == 1);
    bool isAppsLight = (currentApp == 1);
    bool shouldBeLight = args.empty()
        ? !(isSystemLight && isAppsLight)
        : IsInLightPeriod(currentMinutes, times);

    if (isSystemLight != shouldBeLight) {
        SetSystemTheme(hKey, shouldBeLight);
    }

    if (isAppsLight != shouldBeLight) {
        SetAppsTheme(hKey, shouldBeLight);
    }

    RegCloseKey(hKey);
    return 0;
}
