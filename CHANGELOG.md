# Changelog

MouseFix ships through the Microsoft Store. This file records what changed in the source.

## 1.0.9 — 2026-11-24

- Smart Drag is a per-button setting now, and it starts on the left button only. An install coming from an older version keeps Smart Drag on the left button and turns it off everywhere else.
- A precision touchpad that reports the same button-down twice no longer swallows the real release (#6).
- One filtered press counts as one block, not two.
- The release timer no longer restarts on every incoming event.
- The version string lives in one file, `version_defs.h`.
- 42 test groups, 183 assertions, all passing.

中文

- Smart Drag 改成按按键单独开关，默认只开左键；从旧版本升级的，左键保持开启，其余关闭。
- 触摸板把一次按下报成两次时，不再吞掉真正的松开（#6）。
- 一次被过滤的按下只计一次，不再算两次。
- 松开定时器不再被每个新事件重置。
- 版本号统一放在 `version_defs.h`。
- 42 组、183 项单测全部通过。

## 1.0.6 — 2026-11-16

- The UI follows the Windows display language, English or Chinese.
- Per-button custom thresholds no longer write to the left button.
- Smart Drag uses one timebase for event and confirmation timestamps.
- One block per filtered press.
- The tray icon retries at logon and shows a first-run hint.
- The repository became source-only; new builds ship through the Store.

中文

- 界面跟随 Windows 显示语言，中英双语。
- 按键自定义阈值不再写错到左键。
- Smart Drag 的事件和确认时间戳统一时基。
- 每次过滤只计一次。
- 登录时重试托盘图标，并给首次运行一个提示。
- 仓库转为只放源码，新版本走商店。

## 1.0.4 — 2026-07-13

The last standalone build: a drag protection overhaul, x64 and x86 installers, still available under Releases.

中文

最后一个独立安装包：拖拽保护重做，x64 和 x86 两份，仍在 Releases 里。
