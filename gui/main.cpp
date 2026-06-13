#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <cwchar>
#include <string>

#include "task_manager.h"
#include "theme_actions.h"
#include "solar_calculator.h"

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

namespace fs = std::filesystem;

namespace {

constexpr int IDC_LIGHT_EDIT = 1003;
constexpr int IDC_DARK_EDIT = 1004;
constexpr int IDC_TOGGLE = 1006;
constexpr int IDC_INSTALL = 1008;
constexpr int IDC_UNINSTALL = 1009;
constexpr int IDC_LATITUDE_EDIT = 1010;
constexpr int IDC_LONGITUDE_EDIT = 1011;
constexpr int IDC_SOLAR_APPLY = 1012;
constexpr int IDC_CITY_COMBO = 1013;
constexpr UINT WM_APP_REFRESH_STATE = WM_APP + 1;

constexpr int WINDOW_WIDTH = 640;
constexpr int WINDOW_HEIGHT = 620;
constexpr int MIN_WINDOW_WIDTH = 640;
constexpr int MIN_WINDOW_HEIGHT = 620;

enum class ButtonKind {
    Primary,
    Secondary,
    DangerLink
};

struct Palette {
    COLORREF window;
    COLORREF card;
    COLORREF cardAlt;
    COLORREF text;
    COLORREF subText;
    COLORREF border;
    COLORREF accent;
    COLORREF accentHover;
    COLORREF accentText;
    COLORREF success;
    COLORREF warning;
    COLORREF danger;
    COLORREF button;
    COLORREF buttonHover;
};

struct ButtonInfo {
    ButtonKind kind = ButtonKind::Secondary;
    bool hovered = false;
};

HWND gMainWindow = nullptr;
HWND gLightEdit = nullptr;
HWND gDarkEdit = nullptr;
HWND gLatitudeEdit = nullptr;
HWND gLongitudeEdit = nullptr;
HWND gCityCombo = nullptr;
bool gRefreshing = false;
bool gTimesDirty = false;
bool gSolarMode = false;
double gSolarLatitude = 0.0;
double gSolarLongitude = 0.0;
HFONT gTitleFont = nullptr;
HFONT gSubtitleFont = nullptr;
HFONT gBodyFont = nullptr;
HFONT gCaptionFont = nullptr;
HFONT gTimeFont = nullptr;
HFONT gButtonFont = nullptr;
HFONT gChineseCaptionFont = nullptr;
HFONT gChineseBodyFont = nullptr;
HBRUSH gEditBrush = nullptr;
HBRUSH gWindowBrush = nullptr;
HBRUSH gCardBrush = nullptr;
Palette gPalette = {};
ThemeState gThemeState = {};
TaskStatus gTaskStatus = {};
std::wstring gStatusText;

struct CityOption {
    const wchar_t* name;
    double latitude;
    double longitude;
};

const CityOption CITY_OPTIONS[] = {
    { L"北京", 39.9042, 116.4074 },
    { L"上海", 31.2304, 121.4737 },
    { L"广州", 23.1291, 113.2644 },
    { L"深圳", 22.5431, 114.0579 },
    { L"天津", 39.3434, 117.3616 },
    { L"重庆", 29.5630, 106.5516 },
    { L"杭州", 30.2741, 120.1551 },
    { L"南京", 32.0603, 118.7969 },
    { L"武汉", 30.5928, 114.3055 },
    { L"成都", 30.5728, 104.0668 },
    { L"西安", 34.3416, 108.9398 },
    { L"苏州", 31.2989, 120.5853 },
    { L"郑州", 34.7466, 113.6254 },
    { L"长沙", 28.2282, 112.9388 },
    { L"青岛", 36.0671, 120.3826 },
    { L"沈阳", 41.8057, 123.4315 },
    { L"宁波", 29.8683, 121.5440 },
    { L"东莞", 23.0207, 113.7518 },
    { L"无锡", 31.4912, 120.3119 },
    { L"佛山", 23.0215, 113.1214 },
    { L"合肥", 31.8206, 117.2272 },
    { L"福州", 26.0745, 119.2965 },
    { L"厦门", 24.4798, 118.0894 },
    { L"哈尔滨", 45.8038, 126.5349 },
    { L"济南", 36.6512, 117.1201 },
    { L"长春", 43.8171, 125.3235 },
    { L"大连", 38.9140, 121.6147 },
    { L"昆明", 25.0389, 102.7183 },
    { L"南昌", 28.6820, 115.8579 },
    { L"南宁", 22.8170, 108.3669 },
    { L"太原", 37.8706, 112.5489 },
    { L"石家庄", 38.0428, 114.5149 },
    { L"贵阳", 26.6470, 106.6302 },
    { L"兰州", 36.0611, 103.8343 },
    { L"海口", 20.0442, 110.1999 },
    { L"呼和浩特", 40.8426, 111.7492 },
    { L"乌鲁木齐", 43.8256, 87.6168 },
    { L"银川", 38.4872, 106.2309 },
    { L"西宁", 36.6171, 101.7782 },
};

struct Layout {
    RECT mainCard;
    RECT planLabel;
    RECT lightCard;
    RECT darkCard;
    RECT solarCard;
    RECT statusText;
    RECT installButton;
    RECT toggleButton;
    RECT uninstallButton;
    int lightEditX = 0;
    int lightEditY = 0;
    int darkEditX = 0;
    int darkEditY = 0;
    int latitudeEditX = 0;
    int latitudeEditY = 0;
    int longitudeEditX = 0;
    int longitudeEditY = 0;
    int cityComboX = 0;
    int cityComboY = 0;
    int cityComboWidth = 0;
    RECT solarButton;
    int timeEditWidth = 112;
    int coordinateEditWidth = 96;
};

Layout MakeLayout(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;

    int margin = 32;
    int gap = 24;
    int contentWidth = width - margin * 2;
    int timeCardWidth = (contentWidth - gap) / 2;
    int buttonTop = height - 118;
    int bottomY = buttonTop + 56;

    Layout layout;
    layout.mainCard = { margin, 32, width - margin, 164 };
    layout.planLabel = { margin, 186, margin + 170, 210 };
    layout.lightCard = { margin, 216, margin + timeCardWidth, 292 };
    layout.darkCard = { layout.lightCard.right + gap, 216, width - margin, 292 };
    layout.solarCard = { margin, 312, width - margin, 428 };
    layout.installButton = { margin, buttonTop, margin + (contentWidth - 16) / 2, buttonTop + 40 };
    layout.toggleButton = { layout.installButton.right + 16, buttonTop, width - margin, buttonTop + 40 };
    layout.statusText = { margin, bottomY, width - margin - 138, bottomY + 24 };
    layout.uninstallButton = { width - margin - 116, bottomY, width - margin, bottomY + 24 };
    layout.lightEditX = layout.lightCard.left + 26;
    layout.lightEditY = layout.lightCard.top + 36;
    layout.darkEditX = layout.darkCard.left + 26;
    layout.darkEditY = layout.darkCard.top + 36;
    layout.cityComboX = layout.solarCard.left + 78;
    layout.cityComboY = layout.solarCard.top + 18;
    layout.cityComboWidth = layout.solarCard.right - layout.cityComboX - 20;
    layout.latitudeEditX = layout.solarCard.left + 78;
    layout.latitudeEditY = layout.solarCard.top + 68;
    layout.longitudeEditX = layout.solarCard.left + 236;
    layout.longitudeEditY = layout.latitudeEditY;
    layout.solarButton = { layout.solarCard.right - 172, layout.solarCard.top + 64, layout.solarCard.right - 18, layout.solarCard.top + 98 };
    layout.timeEditWidth = 112;
    return layout;
}

std::wstring GetExePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

std::wstring FormatCoordinate(double value) {
    wchar_t buffer[32];
    swprintf(buffer, 32, L"%.4f", value);
    return buffer;
}

bool SameCoordinate(double a, double b) {
    return std::abs(a - b) < 0.0006;
}

fs::path GetExeDirectory() {
    return fs::path(GetExePath()).parent_path();
}

COLORREF Rgb(unsigned char r, unsigned char g, unsigned char b) {
    return RGB(r, g, b);
}

bool IsDarkUi() {
    ThemeState state;
    return ReadThemeState(state) && state.mode == ThemeMode::Dark;
}

Palette MakePalette(bool dark) {
    if (dark) {
        return {
            Rgb(32, 32, 32),
            Rgb(43, 43, 43),
            Rgb(48, 48, 48),
            Rgb(243, 243, 243),
            Rgb(160, 160, 160),
            Rgb(58, 58, 58),
            Rgb(96, 165, 250),
            Rgb(125, 184, 255),
            Rgb(255, 255, 255),
            Rgb(74, 222, 128),
            Rgb(245, 158, 11),
            Rgb(248, 113, 113),
            Rgb(55, 55, 55),
            Rgb(66, 66, 66),
        };
    }

    return {
        Rgb(245, 245, 247),
        Rgb(255, 255, 255),
        Rgb(250, 250, 251),
        Rgb(31, 31, 31),
        Rgb(107, 107, 107),
        Rgb(229, 229, 229),
        Rgb(37, 99, 235),
        Rgb(29, 78, 216),
        Rgb(255, 255, 255),
        Rgb(22, 163, 74),
        Rgb(180, 83, 9),
        Rgb(220, 38, 38),
        Rgb(255, 255, 255),
        Rgb(243, 244, 246),
    };
}

void ResetBrush(HBRUSH& brush, COLORREF color) {
    if (brush) {
        DeleteObject(brush);
    }
    brush = CreateSolidBrush(color);
}

HFONT MakeFont(int pointSize, int weight = FW_NORMAL) {
    HDC hdc = GetDC(nullptr);
    int height = -MulDiv(pointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, hdc);
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable");
}

HFONT MakeFontFace(const wchar_t* faceName, int pointSize, int weight = FW_NORMAL) {
    HDC hdc = GetDC(nullptr);
    int height = -MulDiv(pointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, hdc);
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, faceName);
}

void RebuildThemeResources() {
    gPalette = MakePalette(IsDarkUi());
    ResetBrush(gWindowBrush, gPalette.window);
    ResetBrush(gCardBrush, gPalette.card);
    ResetBrush(gEditBrush, gPalette.card);
}

void ConfigureDwm(HWND hwnd) {
    BOOL dark = IsDarkUi() ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    int corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    int backdrop = DWMSBT_MAINWINDOW;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
}

void SetEditText(HWND edit, const std::wstring& text) {
    SetWindowTextW(edit, text.c_str());
}

std::wstring GetEditText(HWND edit) {
    int length = GetWindowTextLengthW(edit);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(edit, text.data(), length + 1);
    text.resize(length);
    return text;
}

void SetStatus(const std::wstring& text) {
    gStatusText = text;
    InvalidateRect(gMainWindow, nullptr, TRUE);
}

std::wstring TaskStatusText(const TaskStatus& status) {
    if (!status.queryOk) {
        return L"读取失败";
    }
    if (status.immediateExists && status.scheduledExists) {
        return status.solarRefreshExists ? L"日出日落切换已安装" : L"已安装";
    }
    if (!status.immediateExists && !status.scheduledExists && !status.solarRefreshExists) {
        return L"未安装";
    }
    return L"部分安装";
}

COLORREF TaskStatusColor(const TaskStatus& status) {
    if (status.immediateExists && status.scheduledExists) {
        return gPalette.success;
    }
    if (!status.immediateExists && !status.scheduledExists && !status.solarRefreshExists) {
        return gPalette.subText;
    }
    return gPalette.warning;
}

std::wstring CurrentThemeTitle() {
    switch (gThemeState.mode) {
    case ThemeMode::Light:
        return L"☀  浅色模式";
    case ThemeMode::Dark:
        return L"☾  深色模式";
    case ThemeMode::Mixed:
        return L"◐  混合模式";
    case ThemeMode::Unknown:
    default:
        return L"○  未知模式";
    }
}

int CurrentMinutes() {
    time_t t = time(nullptr);
    tm now;
    localtime_s(&now, &t);
    return now.tm_hour * 60 + now.tm_min;
}

std::wstring NextSwitchText() {
    SwitchTimes times;
    if (!MakeSwitchTimes(GetEditText(gLightEdit), GetEditText(gDarkEdit), times)) {
        return L"下一次切换：请先输入有效时间";
    }

    int now = CurrentMinutes();
    bool lightNow = IsInLightPeriod(now, times);
    int target = lightNow ? times.darkStartMinutes : times.lightStartMinutes;
    std::wstring targetText = lightNow ? times.darkStartText : times.lightStartText;
    std::wstring targetMode = lightNow ? L"深色模式" : L"浅色模式";
    std::wstring day = now < target ? L"今天 " : L"明天 ";
    return L"下一次切换：" + day + targetText + L" 切换到" + targetMode;
}

bool ReadTimesFromControls(SwitchTimes& times) {
    std::wstring lightStart = GetEditText(gLightEdit);
    std::wstring darkStart = GetEditText(gDarkEdit);
    if (!MakeSwitchTimes(lightStart, darkStart, times)) {
        SetStatus(L"时间格式不正确，或浅色/深色开始时间相同。请使用 HH:MM。");
        return false;
    }
    return true;
}

void ApplySolarTimesFromWindow() {
    double latitude = 0.0;
    double longitude = 0.0;
    if (!ParseCoordinate(GetEditText(gLatitudeEdit), -90.0, 90.0, latitude)
        || !ParseCoordinate(GetEditText(gLongitudeEdit), -180.0, 180.0, longitude)) {
        SetStatus(L"请输入有效经纬度：纬度 -90 到 90，经度 -180 到 180。");
        return;
    }

    SolarTimes solarTimes;
    if (!CalculateTodaySolarTimes(latitude, longitude, solarTimes)) {
        SetStatus(L"当前位置今天无法计算正常日出/日落时间。");
        return;
    }

    SetEditText(gLightEdit, solarTimes.sunriseText);
    SetEditText(gDarkEdit, solarTimes.sunsetText);
    gTimesDirty = true;
    gSolarMode = true;
    gSolarLatitude = latitude;
    gSolarLongitude = longitude;
    SetStatus(L"已按今天日出/日落设置时间：" + solarTimes.sunriseText + L" / " + solarTimes.sunsetText + L"。");
}

void ApplySelectedCityCoordinates() {
    int index = static_cast<int>(SendMessageW(gCityCombo, CB_GETCURSEL, 0, 0));
    if (index < 0 || index >= static_cast<int>(std::size(CITY_OPTIONS))) {
        return;
    }

    const CityOption& city = CITY_OPTIONS[index];
    SetEditText(gLatitudeEdit, FormatCoordinate(city.latitude));
    SetEditText(gLongitudeEdit, FormatCoordinate(city.longitude));
    SetStatus(std::wstring(L"已选择城市：") + city.name + L"，经纬度已填入。");
}

void SelectMatchingCity(double latitude, double longitude) {
    if (!gCityCombo) {
        return;
    }

    int matchIndex = -1;
    for (int i = 0; i < static_cast<int>(std::size(CITY_OPTIONS)); ++i) {
        if (SameCoordinate(latitude, CITY_OPTIONS[i].latitude) && SameCoordinate(longitude, CITY_OPTIONS[i].longitude)) {
            matchIndex = i;
            break;
        }
    }

    SendMessageW(gCityCombo, CB_SETCURSEL, matchIndex, 0);
}

void RefreshSolarControlsFromTask() {
    double latitude = 0.0;
    double longitude = 0.0;
    if (!gTaskStatus.solarRefreshExists || !ReadInstalledSolarCoordinates(latitude, longitude)) {
        return;
    }

    gSolarMode = true;
    gSolarLatitude = latitude;
    gSolarLongitude = longitude;
    SetEditText(gLatitudeEdit, FormatCoordinate(latitude));
    SetEditText(gLongitudeEdit, FormatCoordinate(longitude));
    SelectMatchingCity(latitude, longitude);
}

void RefreshState(const std::wstring& successMessage = L"状态已刷新。", bool forceTimeRefresh = false) {
    if (gRefreshing) {
        return;
    }
    gRefreshing = true;

    RebuildThemeResources();
    ConfigureDwm(gMainWindow);

    if (!ReadThemeState(gThemeState)) {
        gThemeState = {};
    }

    gTaskStatus = QueryAutoThemeTaskStatus();
    RefreshSolarControlsFromTask();

    SwitchTimes installedTimes;
    if ((forceTimeRefresh || !gTimesDirty) && ReadInstalledSwitchTimes(installedTimes)) {
        SetEditText(gLightEdit, installedTimes.lightStartText);
        SetEditText(gDarkEdit, installedTimes.darkStartText);
        if (forceTimeRefresh) {
            gTimesDirty = false;
        }
    }
    else if ((forceTimeRefresh || !gTimesDirty) && !gTaskStatus.immediateExists && !gTaskStatus.scheduledExists) {
        SwitchTimes defaults;
        SetEditText(gLightEdit, defaults.lightStartText);
        SetEditText(gDarkEdit, defaults.darkStartText);
        if (forceTimeRefresh) {
            gTimesDirty = false;
        }
    }

    gStatusText = successMessage;
    InvalidateRect(gMainWindow, nullptr, TRUE);
    gRefreshing = false;
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
    std::wstring parameters;
    if (gSolarMode) {
        parameters = L"--install-solar " + QuoteArgument(FormatCoordinate(gSolarLatitude)) + L" " + QuoteArgument(FormatCoordinate(gSolarLongitude));
    }
    else {
        parameters = L"--install " + QuoteArgument(times.lightStartText) + L" " + QuoteArgument(times.darkStartText);
    }
    if (!RunElevatedSelfAndWait(parameters, exitCode)) {
        SetStatus(L"管理员权限请求被取消，未更新计划任务。");
        return;
    }

    if (exitCode == 0) {
        gTimesDirty = false;
    }
    RefreshState(exitCode == 0
        ? (gSolarMode ? L"自动切换已更新，日出日落后台计算已开启。" : L"自动切换已安装/更新，日出日落后台计算已关闭。")
        : L"计划任务更新失败，请确认 AutoThemeSwitcher.exe 与 GUI 在同一目录。");
}

void UninstallTasksFromWindow() {
    int result = MessageBoxW(
        gMainWindow,
        L"确定要卸载自动切换吗？\n\n卸载后，系统将不再按设定时间自动切换主题。",
        L"卸载自动切换",
        MB_ICONWARNING | MB_OKCANCEL | MB_DEFBUTTON2);
    if (result != IDOK) {
        SetStatus(L"已取消卸载。");
        return;
    }

    SetStatus(L"正在请求管理员权限并卸载计划任务...");
    DWORD exitCode = 1;
    if (!RunElevatedSelfAndWait(L"--uninstall", exitCode)) {
        SetStatus(L"管理员权限请求被取消，未卸载计划任务。");
        return;
    }

    if (exitCode == 0) {
        gTimesDirty = false;
    }
    RefreshState(exitCode == 0 ? L"自动切换已卸载。" : L"计划任务卸载失败，请重试。");
}

void ToggleThemeFromWindow() {
    RefreshState(ToggleTheme() ? L"已立即切换系统和应用主题。" : L"主题切换失败，无法访问当前用户主题注册表。");
}

void FillRoundRect(HDC hdc, RECT rect, int radius, COLORREF fill, COLORREF border = CLR_INVALID) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border == CLR_INVALID ? fill : border);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawTextLine(HDC hdc, const std::wstring& text, RECT rect, HFONT font, COLORREF color, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text.c_str(), -1, &rect, format);
    SelectObject(hdc, oldFont);
}

void DrawStatusPill(HDC hdc, int x, int y) {
    std::wstring taskText = TaskStatusText(gTaskStatus);
    std::wstring text = taskText == L"日出日落切换已安装"
        ? L"日出日落切换  ● 已安装"
        : L"自动切换  ● " + taskText;
    RECT textMeasure = { 0, 0, 220, 24 };
    HGDIOBJ oldFont = SelectObject(hdc, gCaptionFont);
    DrawTextW(hdc, text.c_str(), -1, &textMeasure, DT_CALCRECT | DT_LEFT | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    RECT pill = { x, y, x + (textMeasure.right - textMeasure.left) + 22, y + 26 };
    FillRoundRect(hdc, pill, 13, gPalette.cardAlt, gPalette.border);
    RECT label = { pill.left + 11, pill.top, pill.right - 10, pill.bottom };
    DrawTextLine(hdc, text, label, gCaptionFont, TaskStatusColor(gTaskStatus));
}

void DrawMainCard(HDC hdc) {
    Layout layout = MakeLayout(gMainWindow);
    RECT card = layout.mainCard;
    FillRoundRect(hdc, card, 18, gPalette.card, gPalette.border);

    DrawTextLine(hdc, CurrentThemeTitle(), { card.left + 26, card.top + 24, card.left + 398, card.top + 54 }, gTitleFont, gPalette.text);
    DrawStatusPill(hdc, card.left + 260, card.top + 26);

    DrawTextLine(hdc, NextSwitchText(), { card.left + 28, card.bottom - 42, card.right - 28, card.bottom - 18 }, gChineseCaptionFont, gPalette.subText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void DrawTimeCard(HDC hdc, RECT card, const std::wstring& title, const std::wstring& icon) {
    FillRoundRect(hdc, card, 16, gPalette.card, gPalette.border);
    DrawTextLine(hdc, icon + L" " + title, { card.left + 22, card.top + 14, card.right - 18, card.top + 38 }, gChineseCaptionFont, gPalette.subText);
}

void DrawSolarCard(HDC hdc, RECT card) {
    FillRoundRect(hdc, card, 16, gPalette.card, gPalette.border);
    DrawTextLine(hdc, L"城市", { card.left + 22, card.top + 20, card.left + 70, card.top + 46 }, gChineseCaptionFont, gPalette.subText);
    DrawTextLine(hdc, L"纬度", { card.left + 22, card.top + 70, card.left + 70, card.top + 96 }, gChineseCaptionFont, gPalette.subText);
    DrawTextLine(hdc, L"经度", { card.left + 180, card.top + 70, card.left + 228, card.top + 96 }, gChineseCaptionFont, gPalette.subText);
}

void PaintWindow(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT client;
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, gWindowBrush);

    DrawMainCard(hdc);

    Layout layout = MakeLayout(hwnd);
    DrawTextLine(hdc, L"切换计划", layout.planLabel, gChineseBodyFont, gPalette.text);
    DrawTimeCard(hdc, layout.lightCard, L"浅色开始时间", L"☀");
    DrawTimeCard(hdc, layout.darkCard, L"深色开始时间", L"☾");
    DrawSolarCard(hdc, layout.solarCard);

    DrawTextLine(hdc, gStatusText, layout.statusText, gCaptionFont, gPalette.subText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    EndPaint(hwnd, &ps);
}

ButtonInfo ButtonInfoForId(int id) {
    if (id == IDC_INSTALL) {
        return { ButtonKind::Primary, false };
    }
    if (id == IDC_SOLAR_APPLY) {
        return { ButtonKind::Secondary, false };
    }
    if (id == IDC_UNINSTALL) {
        return { ButtonKind::DangerLink, false };
    }
    return { ButtonKind::Secondary, false };
}

void DrawOwnerButton(const DRAWITEMSTRUCT* item) {
    int id = static_cast<int>(item->CtlID);
    ButtonInfo info = ButtonInfoForId(id);
    bool hot = (item->itemState & ODS_HOTLIGHT) != 0;
    bool pressed = (item->itemState & ODS_SELECTED) != 0;

    wchar_t text[128] = {};
    GetWindowTextW(item->hwndItem, text, 128);

    COLORREF fill = gPalette.button;
    COLORREF border = gPalette.border;
    COLORREF textColor = gPalette.text;
    int radius = 10;

    if (info.kind == ButtonKind::Primary) {
        fill = hot || pressed ? gPalette.accentHover : gPalette.accent;
        border = fill;
        textColor = gPalette.accentText;
    }
    else if (info.kind == ButtonKind::DangerLink) {
        fill = gPalette.window;
        border = gPalette.window;
        textColor = hot || pressed ? gPalette.danger : gPalette.subText;
        radius = 0;
    }
    else if (hot || pressed) {
        fill = gPalette.buttonHover;
    }

    FillRoundRect(item->hDC, item->rcItem, radius, fill, border);
    RECT textRect = item->rcItem;
    if (pressed && info.kind != ButtonKind::DangerLink) {
        OffsetRect(&textRect, 0, 1);
    }
    DrawTextLine(item->hDC, text, textRect, gChineseCaptionFont, textColor, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

HWND AddButton(const wchar_t* text, int x, int y, int width, int height, int id) {
    HWND control = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x,
        y,
        width,
        height,
        gMainWindow,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(gButtonFont), TRUE);
    return control;
}

HWND AddTimeEdit(const wchar_t* text, int x, int y, int width, int id) {
    HWND control = CreateWindowExW(
        0,
        L"EDIT",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_CENTER,
        x,
        y,
        width,
        30,
        gMainWindow,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(gTimeFont), TRUE);
    return control;
}

HWND AddCoordinateEdit(const wchar_t* text, int x, int y, int width, int id) {
    HWND control = CreateWindowExW(
        0,
        L"EDIT",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_CENTER,
        x,
        y,
        width,
        28,
        gMainWindow,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(gChineseCaptionFont), TRUE);
    return control;
}

HWND AddCityCombo(int x, int y, int width, int id) {
    HWND control = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        x,
        y,
        width,
        240,
        gMainWindow,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(gChineseCaptionFont), TRUE);
    for (const CityOption& city : CITY_OPTIONS) {
        SendMessageW(control, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(city.name));
    }
    return control;
}

void CreateMainControls() {
    gTitleFont = MakeFont(16, FW_SEMIBOLD);
    gSubtitleFont = MakeFont(9);
    gBodyFont = MakeFont(10);
    gCaptionFont = MakeFont(9);
    gTimeFont = MakeFont(16, FW_SEMIBOLD);
    gButtonFont = MakeFont(9, FW_SEMIBOLD);
    gChineseCaptionFont = MakeFontFace(L"Microsoft YaHei UI", 9);
    gChineseBodyFont = MakeFontFace(L"Microsoft YaHei UI", 10);

    Layout layout = MakeLayout(gMainWindow);
    gLightEdit = AddTimeEdit(DEFAULT_LIGHT_START, layout.lightEditX, layout.lightEditY, layout.timeEditWidth, IDC_LIGHT_EDIT);
    gDarkEdit = AddTimeEdit(DEFAULT_DARK_START, layout.darkEditX, layout.darkEditY, layout.timeEditWidth, IDC_DARK_EDIT);
    gCityCombo = AddCityCombo(layout.cityComboX, layout.cityComboY, layout.cityComboWidth, IDC_CITY_COMBO);
    gLatitudeEdit = AddCoordinateEdit(L"", layout.latitudeEditX, layout.latitudeEditY, layout.coordinateEditWidth, IDC_LATITUDE_EDIT);
    gLongitudeEdit = AddCoordinateEdit(L"", layout.longitudeEditX, layout.longitudeEditY, layout.coordinateEditWidth, IDC_LONGITUDE_EDIT);
    AddButton(L"按日出日落设置", layout.solarButton.left, layout.solarButton.top,
        layout.solarButton.right - layout.solarButton.left, layout.solarButton.bottom - layout.solarButton.top, IDC_SOLAR_APPLY);

    AddButton(L"保存并更新", layout.installButton.left, layout.installButton.top,
        layout.installButton.right - layout.installButton.left, layout.installButton.bottom - layout.installButton.top, IDC_INSTALL);
    AddButton(L"立即切换主题", layout.toggleButton.left, layout.toggleButton.top,
        layout.toggleButton.right - layout.toggleButton.left, layout.toggleButton.bottom - layout.toggleButton.top, IDC_TOGGLE);
    AddButton(L"卸载自动切换", layout.uninstallButton.left, layout.uninstallButton.top,
        layout.uninstallButton.right - layout.uninstallButton.left, layout.uninstallButton.bottom - layout.uninstallButton.top, IDC_UNINSTALL);
}

void LayoutControls(HWND hwnd) {
    if (!gLightEdit || !gDarkEdit || !gLatitudeEdit || !gLongitudeEdit || !gCityCombo) {
        return;
    }

    Layout layout = MakeLayout(hwnd);
    MoveWindow(gLightEdit, layout.lightEditX, layout.lightEditY, layout.timeEditWidth, 30, TRUE);
    MoveWindow(gDarkEdit, layout.darkEditX, layout.darkEditY, layout.timeEditWidth, 30, TRUE);
    MoveWindow(gCityCombo, layout.cityComboX, layout.cityComboY, layout.cityComboWidth, 240, TRUE);
    MoveWindow(gLatitudeEdit, layout.latitudeEditX, layout.latitudeEditY, layout.coordinateEditWidth, 28, TRUE);
    MoveWindow(gLongitudeEdit, layout.longitudeEditX, layout.longitudeEditY, layout.coordinateEditWidth, 28, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_SOLAR_APPLY), layout.solarButton.left, layout.solarButton.top,
        layout.solarButton.right - layout.solarButton.left, layout.solarButton.bottom - layout.solarButton.top, TRUE);

    MoveWindow(GetDlgItem(hwnd, IDC_INSTALL), layout.installButton.left, layout.installButton.top,
        layout.installButton.right - layout.installButton.left, layout.installButton.bottom - layout.installButton.top, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_TOGGLE), layout.toggleButton.left, layout.toggleButton.top,
        layout.toggleButton.right - layout.toggleButton.left, layout.toggleButton.bottom - layout.toggleButton.top, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UNINSTALL), layout.uninstallButton.left, layout.uninstallButton.top,
        layout.uninstallButton.right - layout.uninstallButton.left, layout.uninstallButton.bottom - layout.uninstallButton.top, TRUE);
}

void DeleteUiResources() {
    for (HFONT font : { gTitleFont, gSubtitleFont, gBodyFont, gCaptionFont, gTimeFont, gButtonFont, gChineseCaptionFont, gChineseBodyFont }) {
        if (font) {
            DeleteObject(font);
        }
    }
    if (gEditBrush) {
        DeleteObject(gEditBrush);
    }
    if (gWindowBrush) {
        DeleteObject(gWindowBrush);
    }
    if (gCardBrush) {
        DeleteObject(gCardBrush);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        gMainWindow = hwnd;
        RebuildThemeResources();
        ConfigureDwm(hwnd);
        CreateMainControls();
        gTimesDirty = false;
        RefreshState(L"准备就绪。", true);
        PostMessageW(hwnd, WM_APP_REFRESH_STATE, 1, 0);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CITY_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
            ApplySelectedCityCoordinates();
            return 0;
        }
        if (HIWORD(wParam) == EN_CHANGE && (LOWORD(wParam) == IDC_LIGHT_EDIT || LOWORD(wParam) == IDC_DARK_EDIT)) {
            if (!gRefreshing) {
                gTimesDirty = true;
                gSolarMode = false;
            }
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }
        switch (LOWORD(wParam)) {
        case IDC_TOGGLE:
            ToggleThemeFromWindow();
            return 0;
        case IDC_INSTALL:
            InstallTasksFromWindow();
            return 0;
        case IDC_SOLAR_APPLY:
            ApplySolarTimesFromWindow();
            return 0;
        case IDC_UNINSTALL:
            UninstallTasksFromWindow();
            return 0;
        default:
            break;
        }
        break;
    case WM_SIZE:
        LayoutControls(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
        RECT minRect = { 0, 0, MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT };
        AdjustWindowRect(&minRect, WS_OVERLAPPEDWINDOW, FALSE);
        info->ptMinTrackSize.x = minRect.right - minRect.left;
        info->ptMinTrackSize.y = minRect.bottom - minRect.top;
        return 0;
    }
    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE) {
            RefreshState(L"状态已自动更新。");
        }
        return 0;
    case WM_APP_REFRESH_STATE:
        RefreshState(L"状态已自动更新。", wParam != 0);
        return 0;
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_CTLCOLOREDIT:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        SetTextColor(reinterpret_cast<HDC>(wParam), gPalette.text);
        return reinterpret_cast<LRESULT>(gEditBrush);
    case WM_CTLCOLORBTN:
        return reinterpret_cast<LRESULT>(gWindowBrush);
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PAINT:
        PaintWindow(hwnd);
        return 0;
    case WM_DESTROY:
        DeleteUiResources();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int RunCommandMode(int argc, wchar_t* argv[]) {
    if (argc == 3) {
        SwitchTimes times;
        if (!MakeSwitchTimes(argv[1], argv[2], times)) {
            return 1;
        }
        return ApplyThemeForCurrentTime(times) ? 0 : 1;
    }

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
        if (!RegisterAutoThemeTasks(dir / L"AutoThemeSwitcher.exe", dir, times, errorMessage)) {
            return 1;
        }

        return DeleteSolarRefreshTask(errorMessage) ? 0 : 1;
    }

    if (argc >= 2 && (std::wstring(argv[1]) == L"--install-solar" || std::wstring(argv[1]) == L"--refresh-solar")) {
        if (!IsRunAsAdmin() || argc != 4) {
            return 1;
        }

        double latitude = 0.0;
        double longitude = 0.0;
        if (!ParseCoordinate(argv[2], -90.0, 90.0, latitude)
            || !ParseCoordinate(argv[3], -180.0, 180.0, longitude)) {
            return 1;
        }

        SolarTimes solarTimes;
        if (!CalculateTodaySolarTimes(latitude, longitude, solarTimes)) {
            return 1;
        }

        SwitchTimes times;
        if (!MakeSwitchTimes(solarTimes.sunriseText, solarTimes.sunsetText, times)) {
            return 1;
        }

        fs::path dir = GetExeDirectory();
        std::wstring errorMessage;
        if (!RegisterAutoThemeTasks(dir / L"AutoThemeSwitcher.exe", dir, times, errorMessage)) {
            return 1;
        }

        if (std::wstring(argv[1]) == L"--install-solar"
            && !RegisterSolarRefreshTask(dir / L"AutoThemeSwitcher.exe", dir, latitude, longitude, errorMessage)) {
            return 1;
        }

        return 0;
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

    const wchar_t* className = L"AutoThemeSwitcherWindow";
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    RegisterClassW(&windowClass);

    RECT rect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(
        0,
        className,
        L"AutoThemeSwitcher",
        WS_OVERLAPPEDWINDOW,
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
