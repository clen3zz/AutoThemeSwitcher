#pragma once

#include <cwctype>
#include <string>

inline constexpr const wchar_t* DEFAULT_LIGHT_START = L"07:00";
inline constexpr const wchar_t* DEFAULT_DARK_START = L"17:00";

struct SwitchTimes {
    std::wstring lightStartText = DEFAULT_LIGHT_START;
    std::wstring darkStartText = DEFAULT_DARK_START;
    int lightStartMinutes = 7 * 60;
    int darkStartMinutes = 17 * 60;
};

inline bool ParseTime(const std::wstring& value, int& minutes) {
    if (value.size() != 5 || value[2] != L':') {
        return false;
    }
    if (!iswdigit(value[0]) || !iswdigit(value[1]) || !iswdigit(value[3]) || !iswdigit(value[4])) {
        return false;
    }

    int hour = (value[0] - L'0') * 10 + (value[1] - L'0');
    int minute = (value[3] - L'0') * 10 + (value[4] - L'0');
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    minutes = hour * 60 + minute;
    return true;
}

inline bool MakeSwitchTimes(const std::wstring& lightStart, const std::wstring& darkStart, SwitchTimes& times) {
    int lightMinutes = 0;
    int darkMinutes = 0;
    if (!ParseTime(lightStart, lightMinutes) || !ParseTime(darkStart, darkMinutes) || lightMinutes == darkMinutes) {
        return false;
    }

    times.lightStartText = lightStart;
    times.darkStartText = darkStart;
    times.lightStartMinutes = lightMinutes;
    times.darkStartMinutes = darkMinutes;
    return true;
}

inline bool IsInLightPeriod(int currentMinutes, const SwitchTimes& times) {
    if (times.lightStartMinutes < times.darkStartMinutes) {
        return currentMinutes >= times.lightStartMinutes && currentMinutes < times.darkStartMinutes;
    }

    return currentMinutes >= times.lightStartMinutes || currentMinutes < times.darkStartMinutes;
}
