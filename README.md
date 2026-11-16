# MouseFix

Windows 托盘小工具，处理鼠标微动老化后的误触：单击变双击、滚轮反向跳、拖拽半路松手。

A Windows tray app that filters bounce from worn mouse switches.

**日常使用请装微软商店版。** 这个仓库是社区版源码，程序固定在 v1.0.6，不再更新，也不再发布安装包。

[**从 Microsoft Store 安装**](https://apps.microsoft.com/detail/9PD5WFXK17TT)　·　[从源码构建](#build-from-source)　·　[English](#english)

![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

> 本仓库不再提供 exe 下载。历史 Release 只作存档，不建议继续使用。

<a name="chinese"></a>
## 商店版和社区版的区别

商店版由微软商店签名、审核和分发，安装卸载走 Windows，默认随系统启动，修复自动推送。这个仓库留作阅读、审计和自行编译；以后的修复只进商店版。

| | 微软商店版 | 社区版（本仓库） |
| :--- | :--- | :--- |
| 获取 | 商店安装 | 自己编译 |
| 版本 | 持续更新 | 固定 v1.0.6 |
| 更新 | 自动 | 不再更新 |
| 安装 | MSIX，Windows 管理 | 手动运行 exe |
| 开机启动 | 默认开启，系统设置里可以关 | 运行 `add_to_startup.ps1` |
| 签名 | 微软商店签名 | 无签名，可能出现“未知发布者”提示 |
| 价格 | 中国区 ¥12.90，促销价以商店页为准 | 免费（MIT） |

商店版省事的地方很具体：不用每次下载新 exe，不用自己配开机启动，卸载在系统设置里点一下，也不会遇到 SmartScreen 的未知发布者提示。

两个版本不要同时装。第二个启动会提示已经在运行，然后退出；换版本之前先退出并卸载另一个。

## 它解决什么毛病

鼠标用久了会出现三种情况：

- **单击变双击。** 点一下文件名，直接进入重命名；游戏里开一枪，出去两发。
- **滚轮反向跳。** 明明往下滚，页面突然往上窜一段。
- **拖拽中途断开。** 拖文件、选文字，拖到一半自己松手。

原因多半在微动开关和滚轮编码器上。触点磨损、氧化之后，按一次会抖出好几个通断信号，Windows 把这些信号都当成独立操作。换微动是最彻底的办法，但要拆鼠标、动烙铁。

MouseFix 挂一个低级鼠标钩子，把短时间窗内的重复信号丢掉，真实点击照常放行。

## 它不做什么

- **不修硬件。** 微动彻底坏了——按下去没反应，或者一直连击——只能换微动。MouseFix 处理的是“还能用，但抖得烦人”的情况。
- **不改鼠标参数。** 不碰 DPI、加速度、按键映射，也不改指针速度。
- **不拦截模拟点击。** 其他软件生成的鼠标事件直接放行。

## 安装和使用

1. 打开[商店页面](https://apps.microsoft.com/detail/9PD5WFXK17TT)，点安装。
2. 启动后，托盘出现鼠标图标。图标可能收在 `^` 里。
3. 左键或右键点图标打开菜单：总开关、六个按键的单独开关、三档预设、自定义阈值、拦截计数、关于、退出。
4. 默认随 Windows 启动。不想开机启动，到 **设置 > 应用 > 启动** 里关掉。
5. 卸载：**设置 > 应用 > 已安装的应用**，找到 MouseFix，点卸载。

界面语言跟随 Windows 显示语言：中文系统显示中文，其他语言显示英文，繁体中文系统也显示英文。

## 预设

| 预设 | 按键阈值 | 滚轮阈值 | 适合 |
| :--- | :---: | :---: | :--- |
| 默认 | 50ms | 30ms | 日常使用 |
| 办公 | 60ms | 35ms | 抖动比较严重 |
| 严格 | 40ms | 20ms | 需要快速连点，比如游戏 |

阈值是两次信号之间允许的最小间隔。调大，过滤更狠，真实的快速连点也可能被吃掉；调小，响应更快，漏掉的抖动会多一些。拿不准就先用默认，再给单个按键微调。自定义范围是 1–200ms。

## 1.0.6 修了什么

- 给右键、中键、侧键、滚轮设自定义阈值时，原来会写到左键上；现在各改各的。
- Smart Drag 的事件时间和确认时间原来用了两种单位，150ms 超时会算错；现在统一了。
- 一次抖动在拦截计数里原来算两次，现在算一次。
- 拖拽确认期里再按一次鼠标，原来这次点击会被吞掉；现在先补上松开，再放行。
- 左键点托盘图标也能打开菜单；重复启动不再弹错误框。
- 阈值输入框预填当前值，回车确定、Esc 取消，超出 1–200 会留在框里提示，高分屏按 DPI 缩放。

## 隐私

不联网、不写日志、不上传数据。钩子只读取按键类型、光标位置和滚轮方向，用来判断抖动；不记录你点了什么，也不记录键盘输入。

<a name="build-from-source"></a>
## 从源码构建

用 Visual Studio 打开 `MouseFix/MouseFix.sln`，选 x64 Release 编译。自己编译的 exe 没有签名，SmartScreen 可能提示“未知发布者”。

设置保存在 `HKCU\Software\MouseFix`。需要开机自启，把 `add_to_startup.ps1` 和 exe 放同一目录运行。

## 系统要求

Windows 10 1809 或更高版本，64 位。

## 许可证与出处

MIT License。原始项目由 [marvinlehmann](https://github.com/marvinlehmann) 编写，当前版本由 [matreshka15](https://github.com/matreshka15) 维护。

问题、误报和兼容性反馈请提到 [Issues](https://github.com/matreshka15/MouseFix/issues)，或发邮件到 matreshka999@icloud.com。提问题时带上鼠标型号、Windows 版本和当时的阈值，方便复现。

---

<a name="english"></a>
## English

A Windows tray app for mice whose switches have worn out. One click turns into two, the wheel jumps backwards, and drags let go halfway.

**For daily use, install the Microsoft Store version.** This repository is the community source. Its code is frozen at v1.0.6, it receives no further updates, and it no longer publishes installers.

[**Install from Microsoft Store**](https://apps.microsoft.com/detail/9PD5WFXK17TT) · [Build from source](#build-from-source-en) · [中文](#chinese)

> No executables are published here anymore. Older releases are kept as an archive and should not be used.

## Store build vs community source

The Store build is signed, reviewed and distributed by Microsoft. Windows handles installation and removal, it starts with Windows by default, and fixes are delivered automatically. This repository stays available for reading, auditing and building the app yourself. Future fixes go only to the Store build.

| | Microsoft Store | Community source |
| :--- | :--- | :--- |
| Get it | Install from the Store | Build it yourself |
| Version | Updated continuously | Frozen at v1.0.6 |
| Updates | Automatic | None |
| Install | MSIX, managed by Windows | Run the exe |
| Autostart | On by default; turn off in Windows Settings | Run `add_to_startup.ps1` |
| Signing | Signed by the Store | Unsigned; SmartScreen may warn |
| Price | ¥12.90 in China; local price shown in the Store | Free (MIT) |

What the Store build saves you: no downloading a new exe, no startup script to configure, uninstalling takes one click in Settings, and there is no SmartScreen warning about an unknown publisher.

Don't install both. The second copy exits with an "already running" message. Quit and uninstall one before switching.

## What it fixes

After a few years, a mouse starts doing three things:

- **One click registers as two.** You click a filename and land in rename mode. You fire once in a game and get two shots.
- **The wheel jumps backwards.** You scroll down and the page lurches up.
- **Drags release halfway.** Files drop while you are still moving the cursor.

The usual cause is a worn switch or wheel encoder. Worn contacts bounce: one press produces several make/break signals, and Windows treats each as a separate click. Replacing the switch is the real fix, but it means opening the mouse and soldering.

MouseFix installs a low-level mouse hook and drops duplicate signals inside a short time window. Real clicks pass through.

## What it doesn't do

- **It doesn't repair hardware.** If the switch is dead — no click at all, or constant chatter — replace it. MouseFix is for switches that still work but bounce.
- **It doesn't change mouse settings.** No DPI, acceleration, button mapping or pointer speed.
- **It doesn't block synthetic clicks.** Mouse events generated by other software pass through untouched.

## Install and use

1. Open the [Store page](https://apps.microsoft.com/detail/9PD5WFXK17TT) and install.
2. Launch it. A mouse icon appears in the notification area, possibly under `^`.
3. Left- or right-click the icon for the global switch, per-button switches, presets, custom thresholds, the blocked counter, About and Exit.
4. It starts with Windows by default. To stop that, go to **Settings > Apps > Startup**.
5. To uninstall, go to **Settings > Apps > Installed apps**, find MouseFix and remove it.

The interface follows the Windows display language: Chinese on Chinese systems, English everywhere else. Traditional Chinese systems also get English.

## Presets

| Preset | Button threshold | Wheel threshold | Use |
| :--- | :---: | :---: | :--- |
| Default | 50ms | 30ms | Everyday use |
| Office | 60ms | 35ms | Heavier bounce |
| Strict | 40ms | 20ms | Fast clicking, e.g. games |

The threshold is the minimum gap between two signals. Higher filters more but can swallow intentional fast clicks; lower is more responsive but lets more bounce through. Start with Default and adjust individual buttons if needed. Custom values run from 1 to 200ms.

## Fixed in 1.0.6

- Setting a custom threshold on the right, middle, side or wheel button used to write to the left button. Each button now keeps its own value.
- Smart Drag compared event timestamps in milliseconds with a confirmation clock in microseconds. Both now use the same clock.
- A single bounce could be counted twice in the blocked counter. It is counted once.
- Pressing the mouse again while a drag was waiting for confirmation swallowed that click. The release is now sent first, then the new press.
- Left-clicking the tray icon opens the menu. Launching a second copy shows a message instead of an error dialog.
- The threshold box is prefilled, Enter confirms, Esc cancels, values outside 1–200 stay in the box with a hint, and it scales with DPI.

## Privacy

No network access, no log files, no telemetry. The hook reads button type, cursor position and wheel direction to detect bounce. It does not record what you click or what you type.

<a name="build-from-source-en"></a>
## Build from source

Open `MouseFix/MouseFix.sln` in Visual Studio and build x64 Release. The resulting exe is unsigned, so SmartScreen may warn about an unknown publisher.

Settings live in `HKCU\Software\MouseFix`. For autostart, put `add_to_startup.ps1` in the same folder as the exe and run it.

## Requirements

Windows 10 version 1809 or later, 64-bit.

## License and credits

MIT License. Original project by [marvinlehmann](https://github.com/marvinlehmann); current version maintained by [matreshka15](https://github.com/matreshka15).

Bugs, false positives and compatibility reports: [Issues](https://github.com/matreshka15/MouseFix/issues) or matreshka999@icloud.com. Include your mouse model, Windows version and the threshold you were using.
