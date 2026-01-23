#include "context_menu.h"
#include "../../menu_ids.h"
#include "../../version.h"
#include <strsafe.h>

// Constants
#define STATISTICS_BUFFER_SIZE 128
#define THRESHOLD_TEXT_BUFFER_SIZE 32
#define BUTTON_MENU_TEXT_BUFFER_SIZE 64
#define MENU_ID_BUTTON_MULTIPLIER 100

// Button thresholds (3 presets matching global Presets + custom)
static const int BUTTON_THRESHOLDS[] = {40, 50, 60};
static const int WHEEL_THRESHOLDS[] = {20, 30, 35};
static const int BUTTON_THRESHOLD_COUNT = 3;
static const int WHEEL_THRESHOLD_COUNT = 3;

// Button configuration
static const MouseButton BUTTON_TYPES[] = {
	MOUSE_BUTTON_LEFT,
	MOUSE_BUTTON_RIGHT,
	MOUSE_BUTTON_MIDDLE,
	MOUSE_BUTTON_X1,
	MOUSE_BUTTON_X2};
static const int BUTTON_TYPE_COUNT = 5;

// Add threshold menu items to a submenu
// Parameters:
//   hMenu - Handle to the submenu to add items to
//   button - The mouse button this submenu is for
//   thresholds - Array of threshold values in milliseconds
//   threshold_count - Number of thresholds in the array
//   debounce - Pointer to DebounceManager for current threshold values
static void AddThresholdMenuItems(HMENU hMenu, MouseButton button, const int *thresholds, int threshold_count, const DebounceManager *debounce)
{
	for (int i = 0; i < threshold_count; i++)
	{
		UINT threshold_id = IDM_THRESHOLD_BASE + i;
		wchar_t threshold_text[THRESHOLD_TEXT_BUFFER_SIZE];
		StringCchPrintf(threshold_text, THRESHOLD_TEXT_BUFFER_SIZE, L"%dms", thresholds[i]);

		UINT flags = MF_BYPOSITION | MF_STRING;
		if (debounce->buttons[button].thresholdMs == thresholds[i])
			flags |= MF_CHECKED;
		InsertMenu(hMenu, -1, flags, threshold_id + button * MENU_ID_BUTTON_MULTIPLIER, threshold_text);
	}
}

// Add custom threshold option to a submenu
// Parameters:
//   hMenu - Handle to the submenu to add item to
//   button - The mouse button this submenu is for
//   thresholds - Array of predefined threshold values in milliseconds
//   threshold_count - Number of thresholds in the array
//   debounce - Pointer to DebounceManager for current threshold values
static void AddCustomThresholdOption(HMENU hMenu, MouseButton button, const int *thresholds, int threshold_count, const DebounceManager *debounce)
{
	UINT flags = MF_BYPOSITION | MF_STRING;
	bool is_custom = true;
	for (int i = 0; i < threshold_count; i++)
	{
		if (debounce->buttons[button].thresholdMs == thresholds[i])
		{
			is_custom = false;
			break;
		}
	}
	if (is_custom)
		flags |= MF_CHECKED;
	InsertMenu(hMenu, -1, flags, IDM_THRESHOLD_CUSTOM + button * MENU_ID_BUTTON_MULTIPLIER, L"Custom...");
}

// Initialize context menu manager
bool context_menu_init(ContextMenuManager *manager, ContextMenuCallback callback, void *user_data)
{
	if (!manager)
		return false;

	manager->menu = NULL;
	manager->callback = callback;
	manager->user_data = user_data;

	return true;
}

// Create and populate menu items
bool context_menu_create(ContextMenuManager *manager, DebounceManager *debounce)
{
	if (!manager || !debounce)
		return false;

	if (manager->menu)
		context_menu_destroy(manager);

	manager->menu = CreatePopupMenu();
	if (!manager->menu)
		return false;

	wchar_t buffer[STATISTICS_BUFFER_SIZE];
	uint32_t total_blocks = debounce_get_total_blocks(debounce);

	// Add general statistics
	bool is_enabled = debounce_is_any_monitored(debounce);
	StringCchPrintf(buffer, STATISTICS_BUFFER_SIZE, L"Total Blocked: %I32u events", total_blocks);
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, 0, buffer);

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add button submenus with threshold settings
	HMENU hLeftMenu = CreatePopupMenu();
	HMENU hRightMenu = CreatePopupMenu();
	HMENU hMiddleMenu = CreatePopupMenu();
	HMENU hX1Menu = CreatePopupMenu();
	HMENU hX2Menu = CreatePopupMenu();
	HMENU hWheelMenu = CreatePopupMenu();

	// Check if all submenus were created successfully
	if (!hLeftMenu || !hRightMenu || !hMiddleMenu || !hX1Menu || !hX2Menu || !hWheelMenu)
	{
		// Cleanup any successfully created menus
		if (hLeftMenu)
			DestroyMenu(hLeftMenu);
		if (hRightMenu)
			DestroyMenu(hRightMenu);
		if (hMiddleMenu)
			DestroyMenu(hMiddleMenu);
		if (hX1Menu)
			DestroyMenu(hX1Menu);
		if (hX2Menu)
			DestroyMenu(hX2Menu);
		if (hWheelMenu)
			DestroyMenu(hWheelMenu);
		return false;
	}

	// Add threshold items for each button
	AddThresholdMenuItems(hLeftMenu, MOUSE_BUTTON_LEFT, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);
	AddThresholdMenuItems(hRightMenu, MOUSE_BUTTON_RIGHT, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);
	AddThresholdMenuItems(hMiddleMenu, MOUSE_BUTTON_MIDDLE, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);
	AddThresholdMenuItems(hX1Menu, MOUSE_BUTTON_X1, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);
	AddThresholdMenuItems(hX2Menu, MOUSE_BUTTON_X2, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);

	// Add threshold items for wheel
	AddThresholdMenuItems(hWheelMenu, MOUSE_BUTTON_WHEEL, WHEEL_THRESHOLDS, WHEEL_THRESHOLD_COUNT, debounce);

	// Add custom threshold option for buttons
	AddCustomThresholdOption(hLeftMenu, MOUSE_BUTTON_LEFT, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);
	AddCustomThresholdOption(hRightMenu, MOUSE_BUTTON_RIGHT, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);
	AddCustomThresholdOption(hMiddleMenu, MOUSE_BUTTON_MIDDLE, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);
	AddCustomThresholdOption(hX1Menu, MOUSE_BUTTON_X1, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);
	AddCustomThresholdOption(hX2Menu, MOUSE_BUTTON_X2, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, debounce);

	// Add custom threshold option for wheel
	AddCustomThresholdOption(hWheelMenu, MOUSE_BUTTON_WHEEL, WHEEL_THRESHOLDS, WHEEL_THRESHOLD_COUNT, debounce);

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
	InsertMenu(hPresets, -1, MF_BYPOSITION | MF_STRING, IDM_PRESET_DEFAULT, L"Default (50ms)");
	InsertMenu(hPresets, -1, MF_BYPOSITION | MF_STRING, IDM_PRESET_OFFICE, L"Office Mode (60ms)");
	InsertMenu(hPresets, -1, MF_BYPOSITION | MF_STRING, IDM_PRESET_STRICT, L"Strict Mode (40ms)");
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hPresets, L"Presets");

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add About submenu
	HMENU hAbout = CreatePopupMenu();
	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_STRING, IDM_ABOUT_GITHUB, L"GitHub Repository");

	wchar_t version_text[64];
	StringCchPrintf(version_text, 64, L"v%d.%d.%d", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, IDM_ABOUT_VERSION, version_text);

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hAbout, L"About");

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add exit
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");

	return true;
}

// Show context menu at specified position
bool context_menu_show(ContextMenuManager *manager, HWND hwnd, int x, int y, DebounceManager *debounce)
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
bool context_menu_update(ContextMenuManager *manager, DebounceManager *debounce)
{
	if (!manager || !debounce)
		return false;

	context_menu_destroy(manager);
	return context_menu_create(manager, debounce);
}

// Destroy context menu
void context_menu_destroy(ContextMenuManager *manager)
{
	if (!manager)
		return;

	if (manager->menu)
	{
		DestroyMenu(manager->menu);
		manager->menu = NULL;
	}
}