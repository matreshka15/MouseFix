#include "tray_icon.h"
#include "../../resource.h"
#include <shellapi.h>
#include <strsafe.h>
#include <commctrl.h>

// Initialize tray icon manager
bool tray_icon_init(TrayIconManager *manager, HWND hwnd, HINSTANCE hInstance, int icon_id)
{
	if (!manager || !hwnd)
		return false;

	manager->hwnd = hwnd;
	manager->added = false;

	// Initialize NOTIFYICONDATA
	memset(&manager->notify_data, 0, sizeof(NOTIFYICONDATA));
	manager->notify_data.cbSize = sizeof(NOTIFYICONDATA);
	manager->notify_data.hWnd = hwnd;
	manager->notify_data.uID = icon_id;
	manager->notify_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
	manager->notify_data.uCallbackMessage = WM_NOTIFYICON;
	manager->notify_data.uVersion = NOTIFYICON_VERSION_4;

	// Load icon
	if (FAILED(LoadIconMetric(hInstance, MAKEINTRESOURCE(icon_id), LIM_SMALL, &manager->notify_data.hIcon)))
	{
		return false;
	}

	// Register taskbar created message
	manager->taskbar_created_msg = RegisterWindowMessage(L"TaskbarCreated");

	return true;
}

// Add icon to system tray
bool tray_icon_add(TrayIconManager *manager)
{
	if (!manager)
		return false;

	if (!Shell_NotifyIcon(NIM_ADD, &manager->notify_data))
		return false;

	if (!Shell_NotifyIcon(NIM_SETVERSION, &manager->notify_data))
		return false;

	manager->added = true;
	return true;
}

// Remove icon from system tray
void tray_icon_remove(TrayIconManager *manager)
{
	if (!manager)
		return;

	if (manager->added)
	{
		Shell_NotifyIcon(NIM_DELETE, &manager->notify_data);
		manager->added = false;
	}

	if (manager->notify_data.hIcon)
	{
		DestroyIcon(manager->notify_data.hIcon);
		manager->notify_data.hIcon = NULL;
	}
}

// Restore icon after explorer crash or screen resolution change
bool tray_icon_restore(TrayIconManager *manager)
{
	if (!manager)
		return false;

	if (manager->added)
		tray_icon_remove(manager);

	// Try to add icon back - silently handle errors
	// This fixes Issue #4: Error every time screen resolution changes
	// Even if Shell_NotifyIcon fails, the program continues to work correctly
	tray_icon_add(manager);

	return true;
}

// Update tooltip text
bool tray_icon_set_tooltip(TrayIconManager *manager, const wchar_t *tooltip)
{
	if (!manager || !tooltip)
		return false;

	StringCchCopy(manager->notify_data.szTip, sizeof(manager->notify_data.szTip), tooltip);

	if (manager->added)
	{
		return Shell_NotifyIcon(NIM_MODIFY, &manager->notify_data) != 0;
	}

	return true;
}

// Show balloon notification
bool tray_icon_show_balloon(TrayIconManager *manager, const wchar_t *title, const wchar_t *text, DWORD info_flags, UINT timeout)
{
	if (!manager || !title || !text)
		return false;

	manager->notify_data.uFlags |= NIF_INFO;
	StringCchCopy(manager->notify_data.szInfoTitle, sizeof(manager->notify_data.szInfoTitle), title);
	StringCchCopy(manager->notify_data.szInfo, sizeof(manager->notify_data.szInfo), text);
	manager->notify_data.dwInfoFlags = info_flags;
	manager->notify_data.uTimeout = timeout;

	if (manager->added)
	{
		bool result = Shell_NotifyIcon(NIM_MODIFY, &manager->notify_data) != 0;
		manager->notify_data.uFlags &= ~NIF_INFO;
		return result;
	}

	return false;
}

// Handle taskbar recreated message
void tray_icon_handle_taskbar_created(TrayIconManager *manager)
{
	if (!manager)
		return;

	if (manager->taskbar_created_msg)
	{
		tray_icon_restore(manager);
	}
}