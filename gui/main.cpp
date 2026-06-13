#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>

#include "task_manager.h"
#include "theme_actions.h"

namespace fs = std::filesystem;

namespace {

constexpr int IDC_THEME_VALUE = 1001;
constexpr int IDC_TASK_VALUE = 1002;
constexpr int IDC_LIGHT_EDIT = 1003;
constexpr int IDC_DARK_EDIT = 1004;
constexpr int IDC_REFRESH = 1005;
constexpr int IDC_TOGGLE = 1006;
constexpr int IDC_APPLY_NOW = 1007;
constexpr int IDC_INSTALL = 1008;
constexpr int IDC_UNINSTALL = 1009;
constexpr int IDC_STATUS = 1010;

constexpr int WINDOW_WIDTH = 520;
constexpr int WINDOW_HEIGHT = 360;

HWND gMainWindow = nullptr;
HFONT gFont = nullptr;

std::wstring GetExePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

fs::path GetExeDirectory() {
    return fs::path(GetExePath()).parent_path();
}

void SetControlText(int id, const std::wstring& text) {
    SetWindowTextW(GetDlgItem(gMainWindow, id), text.c_str());
}

std::wstring GetControlText(int id) {
    HWND control = GetDlgItem(gMainWindow, id);
    int length = GetWindowTextLengthW(control);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(length);
    return text;
}

void SetStatus(const std::wstring& text) {
    SetControlText(IDC_STATUS, text);
}

std::wstring TaskStatusText(const TaskStatus& status) {
    if (!status.queryOk) {
        return L"读取失败";
    }
    if (status.immediateExists && status.scheduledExists) {
        return L"已安装";
    }
    if (!status.immediateExists && !status.scheduledExists) {
        return L"未安装";
    }
    return L"部分安装";
}

bool ReadTimesFromControls(SwitchTimes& times) {
    std::wstring lightStart = GetControlText(IDC_LIGHT_EDIT);
    std::wstring darkStart = GetControlText(IDC_DARK_EDIT);
    if (!MakeSwitchTimes(lightStart, darkStart, times)) {
        SetStatus(L"时间格式不正确，或浅色/深色开始时间相同。请使用 HH:MM。");
        return false;
    }
    return true;
}

void RefreshState(const std::wstring& successMessage = L"状态已刷新。") {
    ThemeState themeState;
    if (ReadThemeState(themeState)) {
        SetControlText(IDC_THEME_VALUE, ThemeModeText(themeState.mode));
    }
    else {
        SetControlText(IDC_THEME_VALUE, L"未知");
    }

    TaskStatus taskStatus = QueryAutoThemeTaskStatus();
    SetControlText(IDC_TASK_VALUE, TaskStatusText(taskStatus));

    SwitchTimes installedTimes;
    if (ReadInstalledSwitchTimes(installedTimes)) {
        SetControlText(IDC_LIGHT_EDIT, installedTimes.lightStartText);
        SetControlText(IDC_DARK_EDIT, installedTimes.darkStartText);
    }
    else if (!taskStatus.immediateExists && !taskStatus.scheduledExists) {
        SwitchTimes defaults;
        SetControlText(IDC_LIGHT_EDIT, defaults.lightStartText);
        SetControlText(IDC_DARK_EDIT, defaults.darkStartText);
    }

    SetStatus(successMessage);
}

bool RunElevatedSelfAndWait(const std::wstring& parameters, DWORD& exitCode) {
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    std::wstring exePath = GetExePath();
    info.lpVerb = L"runas";
    info.lpFile = exePath.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;

    if (!ShellExecuteExW(&info)) {
        exitCode = GetLastError();
        return false;
    }

    EnableWindow(gMainWindow, FALSE);
    WaitForSingleObject(info.hProcess, INFINITE);
    GetExitCodeProcess(info.hProcess, &exitCode);
    CloseHandle(info.hProcess);
    EnableWindow(gMainWindow, TRUE);
    SetForegroundWindow(gMainWindow);
    return true;
}

void InstallTasksFromWindow() {
    SwitchTimes times;
    if (!ReadTimesFromControls(times)) {
        return;
    }

    SetStatus(L"正在请求管理员权限并更新计划任务...");
    DWORD exitCode = 1;
    std::wstring parameters = L"--install " + QuoteArgument(times.lightStartText) + L" " + QuoteArgument(times.darkStartText);
    if (!RunElevatedSelfAndWait(parameters, exitCode)) {
        SetStatus(L"管理员权限请求被取消，未更新计划任务。");
        return;
    }

    if (exitCode == 0) {
        RefreshState(L"自动切换已安装/更新。");
    }
    else {
        RefreshState(L"计划任务更新失败，请确认 AutoThemeSwitcher.exe 与 GUI 在同一目录。");
    }
}

void UninstallTasksFromWindow() {
    SetStatus(L"正在请求管理员权限并卸载计划任务...");
    DWORD exitCode = 1;
    if (!RunElevatedSelfAndWait(L"--uninstall", exitCode)) {
        SetStatus(L"管理员权限请求被取消，未卸载计划任务。");
        return;
    }

    if (exitCode == 0) {
        RefreshState(L"自动切换已卸载。");
    }
    else {
        RefreshState(L"计划任务卸载失败，请重试。");
    }
}

void ToggleThemeFromWindow() {
    if (ToggleTheme()) {
        RefreshState(L"已立即反转系统和应用主题。");
    }
    else {
        RefreshState(L"主题切换失败，无法访问当前用户主题注册表。");
    }
}

void ApplyCurrentTimeFromWindow() {
    SwitchTimes times;
    if (!ReadTimesFromControls(times)) {
        return;
    }

    if (ApplyThemeForCurrentTime(times)) {
        RefreshState(L"已按当前时间应用主题。");
    }
    else {
        RefreshState(L"按当前时间应用主题失败。");
    }
}

HWND AddControl(const wchar_t* className, const wchar_t* text, DWORD style, int x, int y, int width, int height, int id) {
    HWND control = CreateWindowExW(
        0,
        className,
        text,
        WS_CHILD | WS_VISIBLE | style,
        x,
        y,
        width,
        height,
        gMainWindow,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(gFont), TRUE);
    return control;
}

void CreateMainControls() {
    gFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    AddControl(L"STATIC", L"AutoThemeSwitcher", SS_LEFT, 24, 18, 280, 28, -1);
    AddControl(L"STATIC", L"当前主题", SS_LEFT, 24, 62, 90, 24, -1);
    AddControl(L"STATIC", L"未知", SS_LEFT, 150, 62, 300, 24, IDC_THEME_VALUE);

    AddControl(L"STATIC", L"自动切换", SS_LEFT, 24, 96, 90, 24, -1);
    AddControl(L"STATIC", L"读取中", SS_LEFT, 150, 96, 300, 24, IDC_TASK_VALUE);

    AddControl(L"STATIC", L"浅色开始", SS_LEFT, 24, 140, 90, 24, -1);
    AddControl(L"EDIT", DEFAULT_LIGHT_START, WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 150, 136, 120, 28, IDC_LIGHT_EDIT);

    AddControl(L"STATIC", L"深色开始", SS_LEFT, 296, 140, 90, 24, -1);
    AddControl(L"EDIT", DEFAULT_DARK_START, WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 386, 136, 88, 28, IDC_DARK_EDIT);

    AddControl(L"BUTTON", L"刷新", WS_TABSTOP | BS_PUSHBUTTON, 24, 194, 92, 34, IDC_REFRESH);
    AddControl(L"BUTTON", L"立即反转", WS_TABSTOP | BS_PUSHBUTTON, 128, 194, 104, 34, IDC_TOGGLE);
    AddControl(L"BUTTON", L"按当前时间应用", WS_TABSTOP | BS_PUSHBUTTON, 244, 194, 140, 34, IDC_APPLY_NOW);

    AddControl(L"BUTTON", L"保存并更新自动切换", WS_TABSTOP | BS_DEFPUSHBUTTON, 24, 244, 208, 36, IDC_INSTALL);
    AddControl(L"BUTTON", L"卸载自动切换", WS_TABSTOP | BS_PUSHBUTTON, 248, 244, 160, 36, IDC_UNINSTALL);

    AddControl(L"STATIC", L"", SS_LEFT, 24, 306, 450, 24, IDC_STATUS);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        gMainWindow = hwnd;
        CreateMainControls();
        RefreshState(L"准备就绪。");
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_REFRESH:
            RefreshState();
            return 0;
        case IDC_TOGGLE:
            ToggleThemeFromWindow();
            return 0;
        case IDC_APPLY_NOW:
            ApplyCurrentTimeFromWindow();
            return 0;
        case IDC_INSTALL:
            InstallTasksFromWindow();
            return 0;
        case IDC_UNINSTALL:
            UninstallTasksFromWindow();
            return 0;
        default:
            break;
        }
        break;
    case WM_DESTROY:
        if (gFont) {
            DeleteObject(gFont);
        }
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int RunCommandMode(int argc, wchar_t* argv[]) {
    if (argc >= 2 && std::wstring(argv[1]) == L"--install") {
        if (!IsRunAsAdmin() || argc != 4) {
            return 1;
        }

        SwitchTimes times;
        if (!MakeSwitchTimes(argv[2], argv[3], times)) {
            return 1;
        }

        fs::path dir = GetExeDirectory();
        std::wstring errorMessage;
        return RegisterAutoThemeTasks(dir / L"AutoThemeSwitcher.exe", dir, times, errorMessage) ? 0 : 1;
    }

    if (argc >= 2 && std::wstring(argv[1]) == L"--uninstall") {
        if (!IsRunAsAdmin()) {
            return 1;
        }

        std::wstring errorMessage;
        return DeleteAutoThemeTasks(errorMessage) ? 0 : 1;
    }

    return -1;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;

    SetProcessDPIAware();

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        int commandResult = RunCommandMode(argc, argv);
        LocalFree(argv);
        if (commandResult >= 0) {
            return commandResult;
        }
    }

    const wchar_t* className = L"AutoThemeSwitcherGuiWindow";
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    RegisterClassW(&windowClass);

    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"AutoThemeSwitcher 控制面板",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
