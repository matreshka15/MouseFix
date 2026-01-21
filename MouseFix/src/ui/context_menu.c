#include "context_menu.h"
#include "../../menu_ids.h"
#include <strsafe.h>

// Initialize context menu manager
bool context_menu_init(ContextMenuManager* manager, ContextMenuCallback callback, void* user_data)
{
	if (!manager)
		return false;

	manager->menu = NULL;
	manager->callback = callback;
	manager->user_data = user_data;

	return true;
}

// Create and populate menu items
bool context_menu_create(ContextMenuManager* manager, const DebounceManager* debounce)
{
	if (!manager || !debounce)
		return false;

	if (manager->menu)
		context_menu_destroy(manager);

	manager->menu = CreatePopupMenu();
	if (!manager->menu)
		return false;

	wchar_t buffer[128];
	uint32_t total_blocks = debounce_get_total_blocks(debounce);

	// Add general statistics
	bool is_enabled = debounce_is_any_monitored(debounce);
	StringCchPrintf(buffer, 128, L"Total Blocked: %I32u events", total_blocks);
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, 0, buffer);

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add button submenus with threshold settings
	HMENU hLeftMenu = CreatePopupMenu();
	HMENU hRightMenu = CreatePopupMenu();
	HMENU hMiddleMenu = CreatePopupMenu();
	HMENU hX1Menu = CreatePopupMenu();
	HMENU hX2Menu = CreatePopupMenu();
	HMENU hWheelMenu = CreatePopupMenu();

	// Button thresholds (3 presets matching global Presets + custom)
	const int button_thresholds[] = {40, 60, 80};
	const int wheel_thresholds[] = {20, 30, 40};
	const int button_threshold_count = sizeof(button_thresholds) / sizeof(button_thresholds[0]);
	const int wheel_threshold_count = sizeof(wheel_thresholds) / sizeof(wheel_thresholds[0]);

	// Add threshold items for buttons
	for (int i = 0; i < button_threshold_count; i++)
	{
		UINT threshold_id = IDM_THRESHOLD_BASE + i;
		wchar_t threshold_text[32];
		StringCchPrintf(threshold_text, 32, L"%dms", button_thresholds[i]);

		// Left button submenu
		UINT left_threshold_flags = MF_BYPOSITION | MF_STRING;
		if (debounce->buttons[MOUSE_BUTTON_LEFT].thresholdMs == button_thresholds[i])
			left_threshold_flags |= MF_CHECKED;
		InsertMenu(hLeftMenu, -1, left_threshold_flags, threshold_id + MOUSE_BUTTON_LEFT * 10, threshold_text);

		// Right button submenu
		UINT right_threshold_flags = MF_BYPOSITION | MF_STRING;
		if (debounce->buttons[MOUSE_BUTTON_RIGHT].thresholdMs == button_thresholds[i])
			right_threshold_flags |= MF_CHECKED;
		InsertMenu(hRightMenu, -1, right_threshold_flags, threshold_id + MOUSE_BUTTON_RIGHT * 10, threshold_text);

		// Middle button submenu
		UINT middle_threshold_flags = MF_BYPOSITION | MF_STRING;
		if (debounce->buttons[MOUSE_BUTTON_MIDDLE].thresholdMs == button_thresholds[i])
			middle_threshold_flags |= MF_CHECKED;
		InsertMenu(hMiddleMenu, -1, middle_threshold_flags, threshold_id + MOUSE_BUTTON_MIDDLE * 10, threshold_text);

		// X1 button submenu
		UINT x1_threshold_flags = MF_BYPOSITION | MF_STRING;
		if (debounce->buttons[MOUSE_BUTTON_X1].thresholdMs == button_thresholds[i])
			x1_threshold_flags |= MF_CHECKED;
		InsertMenu(hX1Menu, -1, x1_threshold_flags, threshold_id + MOUSE_BUTTON_X1 * 10, threshold_text);

		// X2 button submenu
		UINT x2_threshold_flags = MF_BYPOSITION | MF_STRING;
		if (debounce->buttons[MOUSE_BUTTON_X2].thresholdMs == button_thresholds[i])
			x2_threshold_flags |= MF_CHECKED;
		InsertMenu(hX2Menu, -1, x2_threshold_flags, threshold_id + MOUSE_BUTTON_X2 * 10, threshold_text);
	}

	// Add threshold items for wheel
	for (int i = 0; i < wheel_threshold_count; i++)
	{
		UINT threshold_id = IDM_THRESHOLD_BASE + i;
		wchar_t threshold_text[32];
		StringCchPrintf(threshold_text, 32, L"%dms", wheel_thresholds[i]);

		UINT wheel_threshold_flags = MF_BYPOSITION | MF_STRING;
		if (debounce->buttons[MOUSE_BUTTON_WHEEL].thresholdMs == wheel_thresholds[i])
			wheel_threshold_flags |= MF_CHECKED;
		InsertMenu(hWheelMenu, -1, wheel_threshold_flags, threshold_id + MOUSE_BUTTON_WHEEL * 10, threshold_text);
	}

	// Add custom threshold option for buttons
	UINT left_custom_flags = MF_BYPOSITION | MF_STRING;
	bool left_is_custom = true;
	for (int i = 0; i < button_threshold_count; i++)
	{
		if (debounce->buttons[MOUSE_BUTTON_LEFT].thresholdMs == button_thresholds[i])
			left_is_custom = false;
	}
	if (left_is_custom) left_custom_flags |= MF_CHECKED;
	InsertMenu(hLeftMenu, -1, left_custom_flags, IDM_THRESHOLD_CUSTOM + MOUSE_BUTTON_LEFT * 10, L"Custom...");

	UINT right_custom_flags = MF_BYPOSITION | MF_STRING;
	bool right_is_custom = true;
	for (int i = 0; i < button_threshold_count; i++)
	{
		if (debounce->buttons[MOUSE_BUTTON_RIGHT].thresholdMs == button_thresholds[i])
			right_is_custom = false;
	}
	if (right_is_custom) right_custom_flags |= MF_CHECKED;
	InsertMenu(hRightMenu, -1, right_custom_flags, IDM_THRESHOLD_CUSTOM + MOUSE_BUTTON_RIGHT * 10, L"Custom...");

	UINT middle_custom_flags = MF_BYPOSITION | MF_STRING;
	bool middle_is_custom = true;
	for (int i = 0; i < button_threshold_count; i++)
	{
		if (debounce->buttons[MOUSE_BUTTON_MIDDLE].thresholdMs == button_thresholds[i])
			middle_is_custom = false;
	}
	if (middle_is_custom) middle_custom_flags |= MF_CHECKED;
	InsertMenu(hMiddleMenu, -1, middle_custom_flags, IDM_THRESHOLD_CUSTOM + MOUSE_BUTTON_MIDDLE * 10, L"Custom...");

	UINT x1_custom_flags = MF_BYPOSITION | MF_STRING;
	bool x1_is_custom = true;
	for (int i = 0; i < button_threshold_count; i++)
	{
		if (debounce->buttons[MOUSE_BUTTON_X1].thresholdMs == button_thresholds[i])
			x1_is_custom = false;
	}
	if (x1_is_custom) x1_custom_flags |= MF_CHECKED;
	InsertMenu(hX1Menu, -1, x1_custom_flags, IDM_THRESHOLD_CUSTOM + MOUSE_BUTTON_X1 * 10, L"Custom...");

	UINT x2_custom_flags = MF_BYPOSITION | MF_STRING;
	bool x2_is_custom = true;
	for (int i = 0; i < button_threshold_count; i++)
	{
		if (debounce->buttons[MOUSE_BUTTON_X2].thresholdMs == button_thresholds[i])
			x2_is_custom = false;
	}
	if (x2_is_custom) x2_custom_flags |= MF_CHECKED;
	InsertMenu(hX2Menu, -1, x2_custom_flags, IDM_THRESHOLD_CUSTOM + MOUSE_BUTTON_X2 * 10, L"Custom...");

	// Add custom threshold option for wheel
	UINT wheel_custom_flags = MF_BYPOSITION | MF_STRING;
	bool wheel_is_custom = true;
	for (int i = 0; i < wheel_threshold_count; i++)
	{
		if (debounce->buttons[MOUSE_BUTTON_WHEEL].thresholdMs == wheel_thresholds[i])
			wheel_is_custom = false;
	}
	if (wheel_is_custom) wheel_custom_flags |= MF_CHECKED;
	InsertMenu(hWheelMenu, -1, wheel_custom_flags, IDM_THRESHOLD_CUSTOM + MOUSE_BUTTON_WHEEL * 10, L"Custom...");

	// Add toggle and separator to each submenu
	InsertMenu(hLeftMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hRightMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hMiddleMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hX1Menu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hX2Menu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hWheelMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	UINT left_toggle_flags = MF_BYPOSITION | MF_STRING | (debounce->buttons[MOUSE_BUTTON_LEFT].isMonitored ? MF_CHECKED : 0);
	UINT right_toggle_flags = MF_BYPOSITION | MF_STRING | (debounce->buttons[MOUSE_BUTTON_RIGHT].isMonitored ? MF_CHECKED : 0);
	UINT middle_toggle_flags = MF_BYPOSITION | MF_STRING | (debounce->buttons[MOUSE_BUTTON_MIDDLE].isMonitored ? MF_CHECKED : 0);
	UINT x1_toggle_flags = MF_BYPOSITION | MF_STRING | (debounce->buttons[MOUSE_BUTTON_X1].isMonitored ? MF_CHECKED : 0);
	UINT x2_toggle_flags = MF_BYPOSITION | MF_STRING | (debounce->buttons[MOUSE_BUTTON_X2].isMonitored ? MF_CHECKED : 0);
	UINT wheel_toggle_flags = MF_BYPOSITION | MF_STRING | (debounce->buttons[MOUSE_BUTTON_WHEEL].isMonitored ? MF_CHECKED : 0);

	InsertMenu(hLeftMenu, 0, left_toggle_flags, IDM_TOGGLE_LEFT, L"Enable");
	InsertMenu(hRightMenu, 0, right_toggle_flags, IDM_TOGGLE_RIGHT, L"Enable");
	InsertMenu(hMiddleMenu, 0, middle_toggle_flags, IDM_TOGGLE_MIDDLE, L"Enable");
	InsertMenu(hX1Menu, 0, x1_toggle_flags, IDM_TOGGLE_X1, L"Enable");
	InsertMenu(hX2Menu, 0, x2_toggle_flags, IDM_TOGGLE_X2, L"Enable");
	InsertMenu(hWheelMenu, 0, wheel_toggle_flags, IDM_TOGGLE_WHEEL, L"Enable");

	// Add button submenus to main menu with current threshold display and checkmark for enabled state
	wchar_t left_text[64];
	StringCchPrintf(left_text, 64, L"Left (%dms)", debounce->buttons[MOUSE_BUTTON_LEFT].thresholdMs);
	UINT left_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (debounce->buttons[MOUSE_BUTTON_LEFT].isMonitored)
		left_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, left_menu_flags, (UINT_PTR)hLeftMenu, left_text);

	wchar_t right_text[64];
	StringCchPrintf(right_text, 64, L"Right (%dms)", debounce->buttons[MOUSE_BUTTON_RIGHT].thresholdMs);
	UINT right_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (debounce->buttons[MOUSE_BUTTON_RIGHT].isMonitored)
		right_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, right_menu_flags, (UINT_PTR)hRightMenu, right_text);

	wchar_t middle_text[64];
	StringCchPrintf(middle_text, 64, L"Middle (%dms)", debounce->buttons[MOUSE_BUTTON_MIDDLE].thresholdMs);
	UINT middle_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (debounce->buttons[MOUSE_BUTTON_MIDDLE].isMonitored)
		middle_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, middle_menu_flags, (UINT_PTR)hMiddleMenu, middle_text);

	wchar_t x1_text[64];
	StringCchPrintf(x1_text, 64, L"X1 (%dms)", debounce->buttons[MOUSE_BUTTON_X1].thresholdMs);
	UINT x1_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (debounce->buttons[MOUSE_BUTTON_X1].isMonitored)
		x1_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, x1_menu_flags, (UINT_PTR)hX1Menu, x1_text);

	wchar_t x2_text[64];
	StringCchPrintf(x2_text, 64, L"X2 (%dms)", debounce->buttons[MOUSE_BUTTON_X2].thresholdMs);
	UINT x2_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (debounce->buttons[MOUSE_BUTTON_X2].isMonitored)
		x2_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, x2_menu_flags, (UINT_PTR)hX2Menu, x2_text);

	wchar_t wheel_text[64];
	StringCchPrintf(wheel_text, 64, L"Wheel (%dms)", debounce->buttons[MOUSE_BUTTON_WHEEL].thresholdMs);
	UINT wheel_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (debounce->buttons[MOUSE_BUTTON_WHEEL].isMonitored)
		wheel_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, wheel_menu_flags, (UINT_PTR)hWheelMenu, wheel_text);

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add control section
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING, IDM_TOGGLE_ENABLE,
	           is_enabled ? L"Disable All" : L"Enable All");
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING, IDM_RESET_STATS, L"Reset Statistics");

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add presets submenu
	HMENU hPresets = CreatePopupMenu();
	InsertMenu(hPresets, -1, MF_BYPOSITION | MF_STRING, IDM_PRESET_DEFAULT, L"Default (60ms)");
	InsertMenu(hPresets, -1, MF_BYPOSITION | MF_STRING, IDM_PRESET_OFFICE, L"Office Mode (80ms)");
	InsertMenu(hPresets, -1, MF_BYPOSITION | MF_STRING, IDM_PRESET_STRICT, L"Strict Mode (40ms)");
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hPresets, L"Presets");

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add exit
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");

	return true;
}

// Show context menu at specified position
bool context_menu_show(ContextMenuManager* manager, HWND hwnd, int x, int y, const DebounceManager* debounce)
{
	if (!manager || !hwnd)
		return false;

	if (!context_menu_create(manager, debounce))
		return false;

	SetForegroundWindow(hwnd);
	TrackPopupMenu(manager->menu, TPM_BOTTOMALIGN, x, y, 0, hwnd, NULL);

	return true;
}

// Update menu with current statistics
bool context_menu_update(ContextMenuManager* manager, const DebounceManager* debounce)
{
	if (!manager || !debounce)
		return false;

	context_menu_destroy(manager);
	return context_menu_create(manager, debounce);
}

// Destroy context menu
void context_menu_destroy(ContextMenuManager* manager)
{
	if (!manager)
		return;

	if (manager->menu)
	{
		DestroyMenu(manager->menu);
		manager->menu = NULL;
	}
}