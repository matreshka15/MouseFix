# MouseFix

<div align="center">

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Size](https://img.shields.io/badge/size-120KB-orange.svg)

**Stop False Clicks - Fix Your Mouse Issues Instantly**

[中文文档](README.zh-CN.md) | [🚀 Quick Start](#quick-start) | [📥 Download](#download)

</div>

---

## 🎯 What is MouseFix?

**Solve these annoying mouse problems:**

- 😤 **Double-click becomes single-click** or vice versa
- 😤 **Wheel scrolling jumps or reverses**
- 😤 **Mouse buttons feel unresponsive**
- 😤 **Accidental clicks ruin your work**

**The root cause?**

Most likely "switch bounce" - your mouse's micro-switch contacts have worn out, causing one click to be detected as multiple. It's a hardware issue, but MouseFix can fix it in software!

**MouseFix Solution:**

✅ **Smart debounce filtering** - Filters out rapid repeated clicks  
✅ **Wheel glitch prevention** - Stops reverse scrolling and page jumps  
✅ **Plug and play** - Works immediately with default settings  
✅ **Zero interference** - Runs silently in the background  
✅ **Completely free** - Open source, forever free

---

## ✨ Features

### 🎯 Full Protection

- **All mouse buttons supported**: Left, Right, Middle, X1 (Back), X2 (Forward), and Wheel
- **Wheel debounce**: Prevents reverse scrolling and page jumps
- **High performance**: Optimized with cache line alignment, minimal CPU usage
- **Silent operation**: No log files, no interference

### 🎨 Simple Interface

- **System tray integration**: Runs quietly in the notification area
- **Right-click menu**: All settings accessible via context menu
- **Visual feedback**: Checkmarks show enabled buttons
- **Real-time statistics**: View blocked events count

### ⚡ Three Preset Modes

| Preset | Button Threshold | Wheel Threshold | Best For |
|--------|------------------|-----------------|----------|
| 🎯 **Default** | 60ms | 30ms | Daily use, balanced |
| 💼 **Office** | 80ms | 40ms | Document work, stricter filtering |
| 🎮 **Strict** | 40ms | 20ms | Gaming, precision tasks |

### 🔧 Individual Button Control

Toggle each mouse button independently:
- ✓ Left Button
- ✓ Right Button
- ✓ Middle Button
- ✓ X1 Button (Back)
- ✓ X2 Button (Forward)
- ✓ Wheel Scroll

---

## 📥 Download

### Latest Release

| Architecture | Size | Download |
|--------------|------|----------|
| 32-bit (x86) | ~112 KB | [MouseFix (x86)](https://github.com/marvinlehmann/Mouse-Debouncer/releases/latest) |
| 64-bit (x64) | ~117 KB | [MouseFix (x64)](https://github.com/marvinlehmann/Mouse-Debouncer/releases/latest) |

**Which version?**
- **64-bit** for most modern computers (recommended)
- **32-bit** for older 32-bit systems

---

## 🚀 Quick Start

1. **Download** and extract MouseFix (64-bit recommended)
2. **Run** `MouseFix.exe` - app appears in system tray
3. **Right-click** tray icon to configure:
   - Toggle buttons (Left, Right, Middle, X1, X2, Wheel)
   - Select preset (Default/Office/Strict) or custom threshold
4. **Exit** via right-click menu when done

**Note:** Only one instance can run. Settings reset on restart.

---

## ❓ Common Issues

**Q: Still having false clicks?**
A: Switch to **Office mode** (80ms) or set custom threshold to 100-150ms.

**Q: Wheel scrolling feels delayed?**
A: Disable wheel debounce (uncheck Wheel) or lower wheel threshold to 20ms.

**Q: App won't start, "MouseFix is already running!"?**
A: Check if MouseFix icon is already in system tray. If not, restart your computer.

**Q: Settings reset after restart?**
A: Current version doesn't save settings. Reconfigure each time or add to startup.

**Q: Antivirus alerts?**
A: Add to antivirus whitelist. App uses system-level mouse hooks but doesn't collect data.

---

## 📄 License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details.

---

## 🙏 Credits

- Original concept: [marvinlehmann](https://github.com/marvinlehmann)
- Optparse library: [skeeto](https://github.com/skeeto/Optparse)

---

<div align="center">

**If MouseFix helped you, give us a ⭐ Star!**

[Back to Top](#mousefix)

</div>