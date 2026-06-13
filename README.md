# AutoThemeSwitcher

轻量级 Windows 深色/浅色模式自动切换工具，只有一个图形界面程序：`AutoThemeSwitcher.exe`。

## 功能

- 在图形界面中查看当前主题和自动切换状态。
- 设置固定浅色/深色开始时间，并创建或更新 Windows 计划任务。
- 按城市或经纬度计算今天的日出/日落时间，并用日出作为浅色开始、日落作为深色开始。
- 日出日落模式会创建后台刷新任务，每 7 天重新计算并更新切换时间。
- 手动修改固定时间并保存时，会自动关闭日出日落后台刷新任务。
- 支持立即切换当前系统和应用主题。
- 支持卸载自动切换任务。

## 构建

需要 Windows、CMake 和 GCC/MinGW。

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

构建产物位于：

```text
build/bin/
└─ AutoThemeSwitcher.exe
```

## 使用

双击运行：

```powershell
.\AutoThemeSwitcher.exe
```

点击“保存并更新”或“卸载自动切换”时，Windows 会请求管理员权限，用于创建或删除计划任务。

计划任务会在登录、解锁、睡眠唤醒后立即检测，也会在浅色/深色开始时间触发。程序被计划任务以隐藏命令模式调用时不会显示窗口。

## 计划任务

程序会按需要创建这些任务：

- `AutoThemeSwitcher`：登录、解锁、睡眠唤醒后立即检测并切换。
- `AutoThemeSwitcher_Scheduled`：每天在浅色/深色开始时间触发。
- `AutoThemeSwitcher_SolarRefresh`：日出日落模式下每 7 天重新计算并更新切换时间。
