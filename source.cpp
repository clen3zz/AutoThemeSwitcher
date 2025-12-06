#include <windows.h>
#include <ctime>

// 定义注册表路径
const wchar_t* REG_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

// 辅助：写入注册表
bool SetRegistryValue(HKEY hKey, const wchar_t* valueName, DWORD data) {
    return RegSetValueExW(hKey, valueName, 0, REG_DWORD, (const BYTE*)&data, sizeof(data)) == ERROR_SUCCESS;
}

// 辅助：读取注册表
bool GetRegistryValue(HKEY hKey, const wchar_t* valueName, DWORD& outValue) {
    DWORD dataSize = sizeof(outValue);
    return RegQueryValueExW(hKey, valueName, nullptr, nullptr, (LPBYTE)&outValue, &dataSize) == ERROR_SUCCESS;
}

// 【官方逻辑 1】重置颜色优先级并通知 DWM
// 对应 ThemeHelper.cpp 中的 ResetColorPrevalence
void ResetColorPrevalence(HKEY hKey) {
    DWORD value = 0;
    SetRegistryValue(hKey, L"ColorPrevalence", value);

    // 官方的三连广播：Immersive -> Theme -> DWM
    // 注意：WM_DWMCOLORIZATIONCOLORCHANGED 已经在 windows.h 中定义，直接使用即可
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_DWMCOLORIZATIONCOLORCHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
}

// 【官方逻辑 2】设置系统主题
// 对应 ThemeHelper.cpp 中的 SetSystemTheme
void SetSystemTheme(HKEY hKey, bool isLight) {
    DWORD value = isLight ? 1 : 0;
    SetRegistryValue(hKey, L"SystemUsesLightTheme", value);

    // 如果是切到浅色，必须重置 Prevalence
    if (isLight) {
        ResetColorPrevalence(hKey);
    }

    // 设置完系统主题后，再次广播
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
}

// 【官方逻辑 3】设置应用主题
// 对应 ThemeHelper.cpp 中的 SetAppsTheme
void SetAppsTheme(HKEY hKey, bool isLight) {
    DWORD value = isLight ? 1 : 0;
    SetRegistryValue(hKey, L"AppsUseLightTheme", value);

    // 设置完应用主题后，再次广播
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, nullptr);
    SendMessageTimeoutW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 5000, nullptr);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // 1. 获取时间
    time_t t = time(nullptr);
    tm now;
    localtime_s(&now, &t);
    int hour = now.tm_hour;

    // 2. 目标: 17点-7点深色(false)，其他浅色(true)
    bool shouldBeLight = true;
    if (hour >= 17 || hour < 7) {
        shouldBeLight = false;
    }

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PATH, 0, KEY_READ | KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return 1;
    }

    // 3. 检查当前状态 (对应 LightSwitchService.cpp 中的 ApplyTheme 逻辑)
    DWORD currentApp = 0, currentSys = 0;
    GetRegistryValue(hKey, L"AppsUseLightTheme", currentApp);
    GetRegistryValue(hKey, L"SystemUsesLightTheme", currentSys);

    bool isSystemLight = (currentSys == 1);
    bool isAppsLight = (currentApp == 1);

    // 4. 按顺序应用 (模仿 ApplyTheme 的顺序)

    // 先切系统 (任务栏/开始菜单)
    if (isSystemLight != shouldBeLight) {
        SetSystemTheme(hKey, shouldBeLight);
    }

    // 后切应用 (资源管理器/窗口)
    if (isAppsLight != shouldBeLight) {
        SetAppsTheme(hKey, shouldBeLight);
    }

    // 如果两个状态都已经正确，则什么都不做，也不会发送消息干扰系统

    RegCloseKey(hKey);
    return 0;
}