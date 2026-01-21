#pragma once

#include <windows.h>
#include <stdbool.h>

#define WM_NOTIFYICON (WM_USER + 1)

// Tray icon manager
typedef struct
{
	HWND hwnd;
	NOTIFYICONDATA notify_data;
	UINT taskbar_created_msg;
	bool added;
} TrayIconManager;

// Initialize tray icon manager
bool tray_icon_init(TrayIconManager* manager, HWND hwnd, HINSTANCE hInstance, int icon_id);

// Add icon to system tray
bool tray_icon_add(TrayIconManager* manager);

// Remove icon from system tray
void tray_icon_remove(TrayIconManager* manager);

// Restore icon after explorer crash
bool tray_icon_restore(TrayIconManager* manager);

// Update tooltip text
bool tray_icon_set_tooltip(TrayIconManager* manager, const wchar_t* tooltip);

// Show balloon notification
bool tray_icon_show_balloon(TrayIconManager* manager, const wchar_t* title, const wchar_t* text, DWORD info_flags, UINT timeout);

// Handle taskbar recreated message
void tray_icon_handle_taskbar_created(TrayIconManager* manager);

// Update icon with overlay indicator
bool tray_icon_update_with_indicator(TrayIconManager* manager, bool is_enabled, HINSTANCE hInstance);