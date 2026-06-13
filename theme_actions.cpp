#include "theme_actions.h"

#include <ctime>
#include <windows.h>

namespace {

constexpr const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

bool SetRegistryValue(HKEY hKey, const wchar_t* valueName, DWORD data) {
    return RegSetValueExW(hKey, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&data), sizeof(data)) == ERROR_SUCCESS;
}

bool GetRegistryValue(HKEY hKey, const wchar_t* valueName, DWORD& outValue) {
    DWORD dataSize = sizeof(outValue);
    return RegQueryValueExW(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(&outValue), &dataSize) == ERROR_SUCCESS;
}

void BroadcastThemeChanged() {
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"ImmersiveColorSet"), SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
}

void ResetColorPrevalence(HKEY hKey) {
    SetRegistryValue(hKey, L"ColorPrevalence", 0);

    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"ImmersiveColorSet"), SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_DWMCOLORIZATIONCOLORCHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
}

bool SetSystemTheme(HKEY hKey, bool isLight) {
    if (!SetRegistryValue(hKey, L"SystemUsesLightTheme", isLight ? 1 : 0)) {
        return false;
    }

    if (isLight) {
        ResetColorPrevalence(hKey);
    }

    BroadcastThemeChanged();
    return true;
}

bool SetAppsTheme(HKEY hKey, bool isLight) {
    if (!SetRegistryValue(hKey, L"AppsUseLightTheme", isLight ? 1 : 0)) {
        return false;
    }

    BroadcastThemeChanged();
    return true;
}

int CurrentMinutes() {
    time_t t = time(nullptr);
    tm now;
    localtime_s(&now, &t);
    return now.tm_hour * 60 + now.tm_min;
}

} // namespace

bool ReadThemeState(ThemeState& state) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        state = {};
        return false;
    }

    DWORD currentApp = 0;
    DWORD currentSys = 0;
    bool appRead = GetRegistryValue(hKey, L"AppsUseLightTheme", currentApp);
    bool sysRead = GetRegistryValue(hKey, L"SystemUsesLightTheme", currentSys);
    RegCloseKey(hKey);

    if (!appRead || !sysRead) {
        state = {};
        return false;
    }

    state.systemLight = currentSys == 1;
    state.appsLight = currentApp == 1;
    if (state.systemLight && state.appsLight) {
        state.mode = ThemeMode::Light;
    }
    else if (!state.systemLight && !state.appsLight) {
        state.mode = ThemeMode::Dark;
    }
    else {
        state.mode = ThemeMode::Mixed;
    }

    return true;
}

bool ApplyTheme(bool isLight) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    ThemeState state;
    DWORD currentApp = 0;
    DWORD currentSys = 0;
    bool appRead = GetRegistryValue(hKey, L"AppsUseLightTheme", currentApp);
    bool sysRead = GetRegistryValue(hKey, L"SystemUsesLightTheme", currentSys);
    state.systemLight = sysRead && currentSys == 1;
    state.appsLight = appRead && currentApp == 1;

    bool ok = true;
    if (!sysRead || state.systemLight != isLight) {
        ok = SetSystemTheme(hKey, isLight) && ok;
    }
    if (!appRead || state.appsLight != isLight) {
        ok = SetAppsTheme(hKey, isLight) && ok;
    }

    RegCloseKey(hKey);
    return ok;
}

bool ToggleTheme() {
    ThemeState state;
    if (!ReadThemeState(state)) {
        return false;
    }

    bool shouldBeLight = !(state.systemLight && state.appsLight);
    return ApplyTheme(shouldBeLight);
}

bool ApplyThemeForCurrentTime(const SwitchTimes& times) {
    return ApplyTheme(IsInLightPeriod(CurrentMinutes(), times));
}

std::wstring ThemeModeText(ThemeMode mode) {
    switch (mode) {
    case ThemeMode::Light:
        return L"浅色";
    case ThemeMode::Dark:
        return L"深色";
    case ThemeMode::Mixed:
        return L"混合";
    case ThemeMode::Unknown:
    default:
        return L"未知";
    }
}
