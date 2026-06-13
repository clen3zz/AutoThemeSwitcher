#include <windows.h>

#include <sstream>
#include <string>
#include <vector>

#include "theme_actions.h"
#include "theme_common.h"

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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)nCmdShow;

    std::vector<std::wstring> args = SplitCommandLine(pCmdLine);
    if (args.empty()) {
        return ToggleTheme() ? 0 : 1;
    }

    SwitchTimes times = ReadSwitchTimes(args);
    return ApplyThemeForCurrentTime(times) ? 0 : 1;
}
