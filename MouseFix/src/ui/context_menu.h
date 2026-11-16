#pragma once

#include <windows.h>
#include "../core/debouncer.h"

// Quick threshold choices shown in each button submenu. Shared with the
// WM_COMMAND handler in main.c so the values live in one place.
#define BUTTON_THRESHOLD_COUNT 3
extern const int BUTTON_THRESHOLDS[BUTTON_THRESHOLD_COUNT];
extern const int WHEEL_THRESHOLDS[BUTTON_THRESHOLD_COUNT];

// Context menu manager
typedef struct
{
	HMENU menu;
} ContextMenuManager;

// Show context menu at specified position
bool context_menu_show(ContextMenuManager *manager, HWND hwnd, int x, int y, DebounceManager *debounce);

// Create and populate menu items
bool context_menu_create(ContextMenuManager *manager, DebounceManager *debounce);

// Destroy context menu
void context_menu_destroy(ContextMenuManager *manager);