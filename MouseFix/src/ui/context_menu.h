#pragma once

#include <windows.h>
#include "../core/debouncer.h"

// Menu identifiers
#define IDM_EXIT (WM_USER + 10)
#define IDM_TOGGLE_ENABLE (WM_USER + 11)
#define IDM_RESET_STATS (WM_USER + 12)
#define IDM_PRESET_DEFAULT (WM_USER + 20)
#define IDM_PRESET_GAME (WM_USER + 21)
#define IDM_PRESET_OFFICE (WM_USER + 22)
#define IDM_PRESET_STRICT (WM_USER + 23)
#define IDM_TOGGLE_LEFT (WM_USER + 30)
#define IDM_TOGGLE_RIGHT (WM_USER + 31)
#define IDM_TOGGLE_MIDDLE (WM_USER + 32)
#define IDM_TOGGLE_X1 (WM_USER + 33)
#define IDM_TOGGLE_X2 (WM_USER + 34)
#define IDM_TOGGLE_WHEEL (WM_USER + 35)

// Context menu callback
typedef void (*ContextMenuCallback)(int menu_id, void* user_data);

// Context menu manager
typedef struct
{
	HMENU menu;
	ContextMenuCallback callback;
	void* user_data;
} ContextMenuManager;

// Initialize context menu manager
bool context_menu_init(ContextMenuManager* manager, ContextMenuCallback callback, void* user_data);

// Show context menu at specified position
bool context_menu_show(ContextMenuManager* manager, HWND hwnd, int x, int y, const DebounceManager* debounce);

// Create and populate menu items
bool context_menu_create(ContextMenuManager* manager, const DebounceManager* debounce);

// Destroy context menu
void context_menu_destroy(ContextMenuManager* manager);

// Update menu with current statistics
bool context_menu_update(ContextMenuManager* manager, const DebounceManager* debounce);