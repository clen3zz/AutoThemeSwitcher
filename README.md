# AutoThemeSwitcher

轻量级 Windows 深色/浅色模式自动切换工具。

它通过 Windows 计划任务定时运行 `AutoThemeSwitcher.exe`，并在登录、解锁、睡眠唤醒后重新检测当前时间，自动把系统和应用主题切换到正确模式。

## 功能

- 默认 `07:00` 切换到浅色模式，`17:00` 切换到深色模式。
- 安装时可以自定义浅色/深色开始时间。
- 重复运行安装器会覆盖更新同名计划任务，可用于修改切换时间。
- 支持跨午夜时间段，例如 `22:00` 到 `06:00`。
- 双击 `AutoThemeSwitcher.exe` 不带参数运行时，会在当前深色/浅色模式之间手动切换。
- 安装器会自动请求管理员权限，用于创建或更新 Windows 计划任务。

## 构建

需要 Windows、CMake 和 GCC/MinGW。

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

构建产物位于：

```text
build/bin/
├─ AutoThemeSwitcher.exe
├─ installer.exe
└─ uninstaller.exe
```

## 安装和修改时间

进入构建产物目录：

```powershell
cd build\bin
```

双击或运行安装器：

```powershell
.\installer.exe
```

安装器会在命令行中提示输入时间：

```text
浅色开始时间 [07:00]:
深色开始时间 [17:00]:
```

直接回车使用默认值。时间格式必须是 24 小时制 `HH:MM`，例如 `08:30`、`22:15`。

也可以直接用命令行参数安装：

```powershell
.\installer.exe 08:30 22:15
```

重复运行 `installer.exe` 会更新计划任务时间。

安装器会创建两个计划任务：

- `AutoThemeSwitcher`：登录、解锁、睡眠唤醒后立即检测并切换。
- `AutoThemeSwitcher_Scheduled`：每天在浅色/深色开始时间触发，但会等待系统空闲后执行。

定时任务启用了空闲条件：计算机空闲超过 1 分钟后才会启动任务，并最多等待空闲 3 小时。因此在系统一直繁忙时，定时触发可能会延后执行；登录、解锁和睡眠唤醒不受这个空闲条件影响。

## 手动切换

双击或直接运行主程序：

```powershell
.\AutoThemeSwitcher.exe
```

不带参数运行时，程序会读取当前系统和应用主题，并切换到另一种模式。计划任务调用时会自动带上安装器配置的时间参数，因此仍按时间自动切换。

## 卸载

双击或运行卸载器：

```powershell
.\uninstaller.exe
```

卸载器会自动请求管理员权限，并删除 `AutoThemeSwitcher` 和 `AutoThemeSwitcher_Scheduled` 两个计划任务。它不会删除 exe 文件，也不会修改当前主题。
