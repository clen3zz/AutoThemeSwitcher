#pragma once

#include <string>

struct SolarTimes {
    int sunriseMinutes = 0;
    int sunsetMinutes = 0;
    std::wstring sunriseText;
    std::wstring sunsetText;
};

bool CalculateTodaySolarTimes(double latitude, double longitude, SolarTimes& times);
bool ParseCoordinate(const std::wstring& text, double minValue, double maxValue, double& value);
