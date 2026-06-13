#include "solar_calculator.h"

#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double ZENITH = 90.833;

double DegToRad(double degrees) {
    return degrees * PI / 180.0;
}

double RadToDeg(double radians) {
    return radians * 180.0 / PI;
}

double NormalizeDegrees(double value) {
    value = std::fmod(value, 360.0);
    if (value < 0.0) {
        value += 360.0;
    }
    return value;
}

double NormalizeHours(double value) {
    value = std::fmod(value, 24.0);
    if (value < 0.0) {
        value += 24.0;
    }
    return value;
}

int DayOfYear(const tm& date) {
    return date.tm_yday + 1;
}

double LocalUtcOffsetHours() {
    time_t now = time(nullptr);
    tm localNow;
    tm utcNow;
    localtime_s(&localNow, &now);
    gmtime_s(&utcNow, &now);

    time_t localAsTime = mktime(&localNow);
    time_t utcAsLocalTime = mktime(&utcNow);
    return difftime(localAsTime, utcAsLocalTime) / 3600.0;
}

bool CalculateSolarEvent(int dayOfYear, double latitude, double longitude, bool sunrise, double utcOffsetHours, int& minutes) {
    double longitudeHour = longitude / 15.0;
    double approximateTime = dayOfYear + ((sunrise ? 6.0 : 18.0) - longitudeHour) / 24.0;
    double meanAnomaly = (0.9856 * approximateTime) - 3.289;
    double trueLongitude = NormalizeDegrees(meanAnomaly
        + (1.916 * std::sin(DegToRad(meanAnomaly)))
        + (0.020 * std::sin(2.0 * DegToRad(meanAnomaly)))
        + 282.634);

    double rightAscension = NormalizeDegrees(RadToDeg(std::atan(0.91764 * std::tan(DegToRad(trueLongitude)))));
    double longitudeQuadrant = std::floor(trueLongitude / 90.0) * 90.0;
    double rightAscensionQuadrant = std::floor(rightAscension / 90.0) * 90.0;
    rightAscension += longitudeQuadrant - rightAscensionQuadrant;
    rightAscension /= 15.0;

    double sinDeclination = 0.39782 * std::sin(DegToRad(trueLongitude));
    double cosDeclination = std::cos(std::asin(sinDeclination));
    double cosHourAngle = (std::cos(DegToRad(ZENITH)) - (sinDeclination * std::sin(DegToRad(latitude))))
        / (cosDeclination * std::cos(DegToRad(latitude)));

    if (cosHourAngle < -1.0 || cosHourAngle > 1.0) {
        return false;
    }

    double hourAngle = sunrise
        ? 360.0 - RadToDeg(std::acos(cosHourAngle))
        : RadToDeg(std::acos(cosHourAngle));
    hourAngle /= 15.0;

    double localMeanTime = hourAngle + rightAscension - (0.06571 * approximateTime) - 6.622;
    double utcTime = localMeanTime - longitudeHour;
    double localTime = NormalizeHours(utcTime + utcOffsetHours);
    minutes = static_cast<int>(std::round(localTime * 60.0)) % (24 * 60);
    return true;
}

std::wstring FormatMinutes(int minutes) {
    int hour = minutes / 60;
    int minute = minutes % 60;
    std::wostringstream stream;
    stream << std::setw(2) << std::setfill(L'0') << hour
        << L":"
        << std::setw(2) << std::setfill(L'0') << minute;
    return stream.str();
}

} // namespace

bool ParseCoordinate(const std::wstring& text, double minValue, double maxValue, double& value) {
    if (text.empty()) {
        return false;
    }

    wchar_t* end = nullptr;
    value = wcstod(text.c_str(), &end);
    if (end == text.c_str()) {
        return false;
    }

    while (*end == L' ' || *end == L'\t') {
        ++end;
    }

    return *end == L'\0' && value >= minValue && value <= maxValue;
}

bool CalculateTodaySolarTimes(double latitude, double longitude, SolarTimes& times) {
    time_t now = time(nullptr);
    tm localDate;
    localtime_s(&localDate, &now);

    int sunrise = 0;
    int sunset = 0;
    double utcOffsetHours = LocalUtcOffsetHours();
    int day = DayOfYear(localDate);

    if (!CalculateSolarEvent(day, latitude, longitude, true, utcOffsetHours, sunrise)
        || !CalculateSolarEvent(day, latitude, longitude, false, utcOffsetHours, sunset)
        || sunrise == sunset) {
        return false;
    }

    times.sunriseMinutes = sunrise;
    times.sunsetMinutes = sunset;
    times.sunriseText = FormatMinutes(sunrise);
    times.sunsetText = FormatMinutes(sunset);
    return true;
}
