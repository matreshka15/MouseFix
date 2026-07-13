# MouseFix

<div align="center">

![Version](https://img.shields.io/badge/version-1.0.4-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Size](https://img.shields.io/badge/size-120KB-orange.svg)

<br>

**Stop False Clicks - Fix Your Mouse Issues Instantly**<br>
**让你的鼠标不再误触 —— 一键解决双击和滚轮故障**

<br>

[🇺🇸 English](#-english) | [🇨🇳 中文](#-chinese) | [📥 Download / 下载](#-download--下载)

</div>

---

<a name="-english"></a>
## 🇺🇸 English

### 🎯 What is MouseFix?

**Is your mouse betraying you?** Hardware switches wear out, causing "switch bounce" where one click registers as multiple. MouseFix injects **industrial-grade software algorithms** to extend your mouse's lifespan.

| 🚫 The Problem | ✅ The Solution |
| :--- | :--- |
| **Phantom Double-Clicks**<br>Ruins games and workflow. | **Smart Click Filtering**<br>Millisecond-precision filtering blocks noise, allowing only real clicks. |
| **Ghost Scrolling**<br>Wheel jumps up when scrolling down. | **Wheel Glitch Fix**<br>Eliminates reverse scrolling caused by worn encoders. |
| **Drag Drops**<br>Files drop halfway while dragging. | **Smart Drag Protection**<br>Exclusive Hybrid Heuristic algorithm fixes drag interruptions. |

### ✨ Key Features

*   **🛡️ Full Protection**: Supports Left, Right, Middle, X1 (Back), X2 (Forward), and Wheel.
*   **🧠 Smart Drag (Hybrid Heuristic)**: Distinguishes between drags and clicks. Prevents accidental drops while maintaining fast response.
*   **⚡ Extreme Performance**: Written in C with **Cache Line Alignment**, ensuring near-zero CPU usage (<1ms latency).
*   **⏱️ Industrial Stability**: Powered by `GetTickCount64` for 24/7 reliability, fixing the classic 49.7-day crash bug.
*   **🤫 Silent Operation**: No installation required, runs in the background, no log files.

### 🎛️ Presets & Configuration

Customize via the system tray right-click menu.

| Preset Mode | Button Threshold | Wheel Threshold | Best For |
| :--- | :---: | :---: | :--- |
| 🎯 **Default** | 50ms | 30ms | Balanced for daily use |
| 💼 **Office** | 60ms | 40ms | Strict filtering for work |
| 🎮 **Strict** | 40ms | 20ms | Low latency for gaming |

### 🚀 Quick Start

1.  **[Download](https://github.com/matreshka15/MouseFix/releases)** and extract MouseFix (**64-bit** recommended).
2.  **Run** `MouseFix-x64.exe` - verify the icon appears in the system tray.
3.  **Right-click** the tray icon to:
    *   Toggle specific buttons.
    *   Select a Preset or set a Custom Threshold.
4.  **Auto-Start**: Run the included `add_to_startup.ps1` script to launch on login.

---

<a name="-chinese"></a>
## 🇨🇳 中文

### 🎯 什么是 MouseFix？

**你的鼠标是否正在“背刺”你？** 微动开关老化会导致“回弹抖动”，让一次点击变成多次。MouseFix 使用**工业级算法**为你的鼠标注入第二生命，无需更换硬件。

| 🚫 痛点问题 | ✅ 解决方案 |
| :--- | :--- |
| **致命双击**<br>点一下变两下，游戏误操作。 | **智能点击过滤**<br>毫秒级精准识别，只放过真实点击，拦截杂波。 |
| **幽灵滚轮**<br>网页浏览时反向乱跳。 | **滚轮反向修正**<br>彻底消除滚轮编码器老化带来的反向信号。 |
| **拖拽断触**<br>拖拽文件或选中文字时突然断开。 | **Smart Drag 拖拽保护**<br>独家混合启发式算法，完美修复拖拽断连，稳如泰山。 |

### ✨ 核心功能

*   **🛡️ 全方位保护**：支持所有按键（左/右/中/X1/X2）及滚轮。
*   **🧠 智能拖拽 (Smart Drag)**：智能区分点击与拖拽，防止拖拽中途断触，同时保持极速点击响应。
*   **⚡ 极致性能**：底层 C 语言编写，采用**缓存行对齐 (Cache Line Alignment)** 优化，资源占用几乎为零。
*   **⏱️ 工业级稳定性**：基于 64 位 `GetTickCount64` 内核计时，实现 7x24 小时全天候零故障运行。
*   **🤫 零感运行**：绿色软件，无窗口、无干扰，不产生垃圾文件。

### 🎛️ 预设与配置

通过系统托盘图标右键菜单进行配置。

| 预设模式 | 按键阈值 | 滚轮阈值 | 适用场景 |
| :--- | :---: | :---: | :--- |
| 🎯 **默认模式** | 50ms | 30ms | 日常使用，平衡性能 |
| 💼 **办公模式** | 60ms | 40ms | 文档处理，严格过滤 |
| 🎮 **严格模式** | 40ms | 20ms | 游戏操作，极速响应 |

### 🚀 快速开始

1.  **[下载](https://github.com/matreshka15/MouseFix/releases)** 并解压 MouseFix（推荐 **64位** 版本）。
2.  **运行** `MouseFix-x64.exe` - 确认系统托盘出现图标。
3.  **右键点击** 托盘图标可以：
    *   单独开关某个按键的防抖。
    *   选择预设模式或输入自定义阈值。
4.  **开机自启**：运行目录下的 `add_to_startup.ps1` 脚本即可一键配置。

---

<a name="-download--下载"></a>
## 📥 Download / 下载

> **Latest Release / 最新版本: v1.0.4**

| File | Description |
| :--- | :--- |
| 📦 **[MouseFix-x64.exe](https://github.com/matreshka15/MouseFix/releases/latest/download/MouseFix-x64.exe)** | Recommended for modern PCs / 现代电脑推荐 |
| 📦 **[MouseFix-x86.exe](https://github.com/matreshka15/MouseFix/releases/latest/download/MouseFix-x86.exe)** | For legacy 32-bit systems / 旧版32位系统 |

<br>

## 📄 License & Credits

*   **License**: MIT License. Free forever.
*   **Credits**: Original concept by [marvinlehmann](https://github.com/marvinlehmann), improved by [matreshka15](https://github.com/matreshka15).

<div align="center">
<br>

**If MouseFix saved your mouse, give us a ⭐ Star!**<br>

[Back to Top / 回到顶部](#mousefix)

</div>
