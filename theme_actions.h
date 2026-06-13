#pragma once

#include <string>

#include "theme_common.h"

enum class ThemeMode {
    Light,
    Dark,
    Mixed,
    Unknown
};

struct ThemeState {
    ThemeMode mode = ThemeMode::Unknown;
    bool systemLight = false;
    bool appsLight = false;
};

bool ReadThemeState(ThemeState& state);
bool ApplyTheme(bool isLight);
bool ToggleTheme();
bool ApplyThemeForCurrentTime(const SwitchTimes& times);
std::wstring ThemeModeText(ThemeMode mode);
