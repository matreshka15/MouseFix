# MouseFix

<div align="center">

![Version](https://img.shields.io/badge/version-1.0.3-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Size](https://img.shields.io/badge/size-120KB-orange.svg)

**Stop False Clicks - Fix Your Mouse Issues Instantly**

[中文文档](README.zh-CN.md) | [🚀 Quick Start](#quick-start) | [📥 Download](#download)

</div>

---

## 🎯 What is MouseFix?

**Is your mouse betraying you?**

- 💥 **Phantom Double-Clicks**: One click registers as two, ruining your games and workflow.
- 🌀 **Ghost Scrolling**: The wheel jumps up when you scroll down.
- 💢 **Drag Drops**: Files drop halfway while dragging, text selection gets interrupted.

**Hardware failing? Software fix!**

It's likely "switch bounce" from aging micro-switches. Don't buy a new mouse yet—MouseFix injects new life into your hardware with **industrial-grade algorithms**.

**The MouseFix Advantage:**

- 🛡️ **Smart Click Filtering**: Millisecond-precision filtering that blocks noise but lets real clicks through.
- 🧠 **Smart Drag Protection**: Exclusive Hybrid Heuristic algorithm fixes drag-drop interruptions. Rock solid.
- 🎡 **Wheel Glitch Fix**: Eliminates reverse scrolling caused by worn encoders.
- 🚀 **Zero-Touch Operation**: Invisible background running, near-zero resource usage.
- 💎 **Pure Open Source**: No ads, no tracking, forever free.

---

## ✨ Features

### 🎯 Full Protection

- **All mouse buttons supported**: Left, Right, Middle, X1 (Back), X2 (Forward), and Wheel
- **Smart Drag (Hybrid Heuristic)**: Distinguishes between drags and clicks to prevent accidental drops while maintaining fast response
- **Wheel debounce**: Prevents reverse scrolling and page jumps
- **Extreme Performance**: Written in C with Cache Line Alignment optimization, near-zero CPU usage
- **Silent operation**: No log files, no interference

### 🎨 Simple Interface

- **System tray integration**: Runs quietly in the notification area
- **Right-click menu**: All settings accessible via context menu
- **Visual feedback**: Checkmarks show enabled buttons
- **Real-time statistics**: View blocked events count

### ⚡ Advanced Technologies

- **Smart Drag (Hybrid Heuristic)**:
  Traditional debouncers can interrupt drag-and-drop operations if the switch bounces during release.
  MouseFix uses a smart algorithm that detects "drag" vs "click":
  - **Click**: Fast response, standard debounce.
  - **Drag**: If you hold the button (>200ms) or move the mouse (>5px), it defers the release slightly to ensure it's intentional.
  - **Result**: Reliable dragging without accidental drops, zero impact on fast clicking.

- **Industrial-Grade Kernel Timing**:
  Powered by `GetTickCount64` for 24/7 reliability, breaking through the physical time limits of 32-bit systems for uninterrupted operation.

- **Low Latency Architecture**:
  Optimized C implementation ensures the input lag is virtually non-existent (<1ms overhead).

### ⚡ Three Preset Modes

| Preset | Button Threshold | Wheel Threshold | Best For |
|--------|------------------|-----------------|----------|
| 🎯 **Default** | 50ms | 30ms | Daily use, balanced |
| 💼 **Office** | 60ms | 40ms | Document work, stricter filtering |
| 🎮 **Strict** | 40ms | 20ms | Gaming, precision tasks |

### 🔧 Individual Button Control

Toggle each mouse button independently:
- ✓ Left Button
- ✓ Right Button
- ✓ Middle Button
- ✓ X1 Button (Back)
- ✓ X2 Button (Forward)
- ✓ Wheel Scroll (Custom 1-200ms)

---

## 📥 Download

### Latest Release

[View all releases](https://github.com/matreshka15/MouseFix/releases)

**Which version?**
- **64-bit** for most modern computers (recommended)
- **32-bit** for older 32-bit systems

---

## 🚀 Quick Start

1. **Download** and extract MouseFix (64-bit recommended)
2. **Run** `MouseFix-x64.exe` (or `MouseFix-x86.exe`) - app appears in system tray
3. **Right-click** tray icon to configure:
   - Toggle buttons (Left, Right, Middle, X1, X2, Wheel)
   - Select preset (Default/Office/Strict) or custom threshold
4. **Exit** via right-click menu when done

**Note:** Only one instance can run. Settings are automatically saved to the Windows Registry.

---

## 🔧 Auto-Startup Configuration

Use the provided PowerShell script to configure MouseFix to start automatically:

1. **Run** `add_to_startup.ps1` (right-click and select "Run with PowerShell")
2. **Select** a startup method:
   - **Startup Folder** (recommended) - Easy to manage, visible in startup apps
   - **Registry** - Hidden system-level startup
3. **Done** - MouseFix will launch automatically on next login

To remove auto-startup, run the script again and select "Remove Startup".

**Tip:** Press `Win+R` and type `shell:startup` to view startup folder contents.

---

## ❓ Common Issues

**Q: Still having false clicks?**
A: Switch to **Office mode** (60ms) or set custom threshold to 80-120ms.

**Q: Wheel scrolling feels delayed?**
A: Disable wheel debounce (uncheck Wheel) or lower wheel threshold to 20ms.

**Q: App won't start, "MouseFix is already running!"?**
A: Check if MouseFix icon is already in system tray. If not, restart your computer.

**Q: Settings reset after restart?**
A: Settings are now persistent via the Windows Registry (`HKCU\Software\MouseFix`). Your configuration will be restored automatically.

**Q: Antivirus alerts?**
A: Add to antivirus whitelist. App uses system-level mouse hooks but doesn't collect data.

---

## 📄 License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details.

---

## 🙏 Credits

- Original concept: [marvinlehmann](https://github.com/marvinlehmann)
- Forked and improved by: [matreshka15](https://github.com/matreshka15)

---

<div align="center">

**If MouseFix helped you, give us a ⭐ Star!**

[Back to Top](#mousefix)

</div>