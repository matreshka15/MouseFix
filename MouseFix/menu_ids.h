#pragma once

// Main menu IDs
#define WM_NOTIFYICON (WM_USER + 1)
#define WM_MOUSEFIX_SHOW (WM_USER + 2)
#define WM_MOUSEFIX_LICENSE (WM_USER + 3)
#define WM_MOUSEFIX_REVIEW (WM_USER + 4)
#define IDM_EXIT (WM_USER + 10)
#define IDM_TOGGLE_ENABLE (WM_USER + 11)
#define IDM_RESET_STATS (WM_USER + 12)

// Preset menu IDs
#define IDM_PRESET_DEFAULT (WM_USER + 20)
#define IDM_PRESET_OFFICE (WM_USER + 22)
#define IDM_PRESET_STRICT (WM_USER + 23)

// Button threshold menu IDs: base + button * stride + preset index.
#define IDM_THRESHOLD_BASE (WM_USER + 100)
#define IDM_THRESHOLD_STRIDE 10
#define IDM_THRESHOLD_CUSTOM_BASE (WM_USER + 200)

// Button toggle menu IDs
#define IDM_TOGGLE_LEFT (WM_USER + 30)
#define IDM_TOGGLE_RIGHT (WM_USER + 31)
#define IDM_TOGGLE_MIDDLE (WM_USER + 32)
#define IDM_TOGGLE_X1 (WM_USER + 33)
#define IDM_TOGGLE_X2 (WM_USER + 34)
#define IDM_TOGGLE_WHEEL (WM_USER + 35)

// Per-button Smart Drag menu IDs: base + button index.
#define IDM_SMART_DRAG_BASE (WM_USER + 50)

// About menu IDs
#define IDM_ABOUT_VERSION (WM_USER + 42)
#define IDM_ABOUT_OPEN_STORE (WM_USER + 43)
#define IDM_TRIAL_BUY (WM_USER + 44)