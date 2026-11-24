#include "context_menu.h"
#include "../../menu_ids.h"
#include "../../version.h"
#include "../utils/localization.h"
#include <strsafe.h>

// Constants
#define STATISTICS_BUFFER_SIZE 128
#define THRESHOLD_TEXT_BUFFER_SIZE 32
#define BUTTON_MENU_TEXT_BUFFER_SIZE 64

// Button thresholds (3 presets matching global Presets + custom)
const int BUTTON_THRESHOLDS[BUTTON_THRESHOLD_COUNT] = {40, 50, 60};
const int WHEEL_THRESHOLDS[BUTTON_THRESHOLD_COUNT] = {20, 30, 35};

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
static void AddThresholdMenuItems(HMENU hMenu, MouseButton button, const int *thresholds, int threshold_count, uint32_t current_threshold)
{
	for (int i = 0; i < threshold_count; i++)
	{
		wchar_t threshold_text[THRESHOLD_TEXT_BUFFER_SIZE];
		StringCchPrintf(threshold_text, THRESHOLD_TEXT_BUFFER_SIZE, L"%dms", thresholds[i]);

		UINT flags = MF_BYPOSITION | MF_STRING;
		if (current_threshold == (uint32_t)thresholds[i])
			flags |= MF_CHECKED;
		InsertMenu(hMenu, -1, flags,
			   IDM_THRESHOLD_BASE + button * IDM_THRESHOLD_STRIDE + i, threshold_text);
	}
}

// Add custom threshold option to a submenu
// Parameters:
//   hMenu - Handle to the submenu to add item to
//   button - The mouse button this submenu is for
//   thresholds - Array of predefined threshold values in milliseconds
//   threshold_count - Number of thresholds in the array
//   debounce - Pointer to DebounceManager for current threshold values
static void AddCustomThresholdOption(HMENU hMenu, MouseButton button, const int *thresholds, int threshold_count, uint32_t current_threshold)
{
	UINT flags = MF_BYPOSITION | MF_STRING;
	bool is_custom = true;
	for (int i = 0; i < threshold_count; i++)
	{
		if (current_threshold == (uint32_t)thresholds[i])
		{
			is_custom = false;
			break;
		}
	}
	if (is_custom)
		flags |= MF_CHECKED;
	InsertMenu(hMenu, -1, flags, IDM_THRESHOLD_CUSTOM_BASE + button, loc_str(LOC_CUSTOM));
}

// Per-button Smart Drag toggle. Inserted right below Enable so the two
// switches for a button sit together.
static void AddSmartDragToggle(HMENU hMenu, MouseButton button, bool enabled)
{
	UINT flags = MF_BYPOSITION | MF_STRING;
	if (enabled)
		flags |= MF_CHECKED;
	InsertMenu(hMenu, 1, flags, IDM_SMART_DRAG_BASE + button, loc_str(LOC_SMART_DRAG));
}

// Create and populate menu items
bool context_menu_create(ContextMenuManager *manager, DebounceManager *debounce, const StoreLicenseInfo *license)
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
	DebounceButtonConfig configs[MOUSE_BUTTON_COUNT];
	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
		configs[i] = debounce_get_button_config(debounce, (MouseButton)i);

	// Trial status goes above everything so it is the first thing a trial or
	// expired user sees. Expired is a live entry that opens the Store.
	if (license && license->state == STORE_LICENSE_TRIAL)
	{
		wchar_t trial_text[THRESHOLD_TEXT_BUFFER_SIZE];
		StringCchPrintf(trial_text, THRESHOLD_TEXT_BUFFER_SIZE, loc_str(LOC_TRIAL_DAYS_LEFT), license->days_remaining);
		InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, 0, trial_text);
		InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	}
	else if (license && license->state == STORE_LICENSE_EXPIRED)
	{
		InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING, IDM_TRIAL_BUY, loc_str(LOC_TRIAL_EXPIRED_MENU));
		InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	}

	// Add general statistics
	bool is_enabled = debounce_is_any_monitored(debounce);
	StringCchPrintf(buffer, STATISTICS_BUFFER_SIZE, loc_str(LOC_TOTAL_BLOCKED), total_blocks);
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
		if (manager->menu)
		{
			DestroyMenu(manager->menu);
			manager->menu = NULL;
		}
		return false;
	}

	// Add threshold items for each button
	AddThresholdMenuItems(hLeftMenu, MOUSE_BUTTON_LEFT, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_LEFT].threshold_ms);
	AddThresholdMenuItems(hRightMenu, MOUSE_BUTTON_RIGHT, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_RIGHT].threshold_ms);
	AddThresholdMenuItems(hMiddleMenu, MOUSE_BUTTON_MIDDLE, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_MIDDLE].threshold_ms);
	AddThresholdMenuItems(hX1Menu, MOUSE_BUTTON_X1, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_X1].threshold_ms);
	AddThresholdMenuItems(hX2Menu, MOUSE_BUTTON_X2, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_X2].threshold_ms);

	// Add threshold items for wheel
	AddThresholdMenuItems(hWheelMenu, MOUSE_BUTTON_WHEEL, WHEEL_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_WHEEL].threshold_ms);

	// Add custom threshold option for buttons
	AddCustomThresholdOption(hLeftMenu, MOUSE_BUTTON_LEFT, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_LEFT].threshold_ms);
	AddCustomThresholdOption(hRightMenu, MOUSE_BUTTON_RIGHT, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_RIGHT].threshold_ms);
	AddCustomThresholdOption(hMiddleMenu, MOUSE_BUTTON_MIDDLE, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_MIDDLE].threshold_ms);
	AddCustomThresholdOption(hX1Menu, MOUSE_BUTTON_X1, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_X1].threshold_ms);
	AddCustomThresholdOption(hX2Menu, MOUSE_BUTTON_X2, BUTTON_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_X2].threshold_ms);

	// Add custom threshold option for wheel
	AddCustomThresholdOption(hWheelMenu, MOUSE_BUTTON_WHEEL, WHEEL_THRESHOLDS, BUTTON_THRESHOLD_COUNT, configs[MOUSE_BUTTON_WHEEL].threshold_ms);

	// Add toggle and separator to each submenu
	InsertMenu(hLeftMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hRightMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hMiddleMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hX1Menu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hX2Menu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hWheelMenu, 0, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	UINT left_toggle_flags = MF_BYPOSITION | MF_STRING | (configs[MOUSE_BUTTON_LEFT].monitored ? MF_CHECKED : 0);
	UINT right_toggle_flags = MF_BYPOSITION | MF_STRING | (configs[MOUSE_BUTTON_RIGHT].monitored ? MF_CHECKED : 0);
	UINT middle_toggle_flags = MF_BYPOSITION | MF_STRING | (configs[MOUSE_BUTTON_MIDDLE].monitored ? MF_CHECKED : 0);
	UINT x1_toggle_flags = MF_BYPOSITION | MF_STRING | (configs[MOUSE_BUTTON_X1].monitored ? MF_CHECKED : 0);
	UINT x2_toggle_flags = MF_BYPOSITION | MF_STRING | (configs[MOUSE_BUTTON_X2].monitored ? MF_CHECKED : 0);
	UINT wheel_toggle_flags = MF_BYPOSITION | MF_STRING | (configs[MOUSE_BUTTON_WHEEL].monitored ? MF_CHECKED : 0);

	InsertMenu(hLeftMenu, 0, left_toggle_flags, IDM_TOGGLE_LEFT, loc_str(LOC_ENABLE));
	InsertMenu(hRightMenu, 0, right_toggle_flags, IDM_TOGGLE_RIGHT, loc_str(LOC_ENABLE));
	InsertMenu(hMiddleMenu, 0, middle_toggle_flags, IDM_TOGGLE_MIDDLE, loc_str(LOC_ENABLE));
	InsertMenu(hX1Menu, 0, x1_toggle_flags, IDM_TOGGLE_X1, loc_str(LOC_ENABLE));
	InsertMenu(hX2Menu, 0, x2_toggle_flags, IDM_TOGGLE_X2, loc_str(LOC_ENABLE));
	InsertMenu(hWheelMenu, 0, wheel_toggle_flags, IDM_TOGGLE_WHEEL, loc_str(LOC_ENABLE));

	// Smart Drag is a drag-release protection; it makes no sense for the wheel.
	AddSmartDragToggle(hLeftMenu, MOUSE_BUTTON_LEFT, configs[MOUSE_BUTTON_LEFT].smart_drag_enabled);
	AddSmartDragToggle(hRightMenu, MOUSE_BUTTON_RIGHT, configs[MOUSE_BUTTON_RIGHT].smart_drag_enabled);
	AddSmartDragToggle(hMiddleMenu, MOUSE_BUTTON_MIDDLE, configs[MOUSE_BUTTON_MIDDLE].smart_drag_enabled);
	AddSmartDragToggle(hX1Menu, MOUSE_BUTTON_X1, configs[MOUSE_BUTTON_X1].smart_drag_enabled);
	AddSmartDragToggle(hX2Menu, MOUSE_BUTTON_X2, configs[MOUSE_BUTTON_X2].smart_drag_enabled);

	// Add button submenus to main menu with current threshold display and checkmark for enabled state
	wchar_t left_text[64];
	StringCchPrintf(left_text, 64, loc_str(LOC_LEFT_FMT), configs[MOUSE_BUTTON_LEFT].threshold_ms);
	UINT left_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (configs[MOUSE_BUTTON_LEFT].monitored)
		left_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, left_menu_flags, (UINT_PTR)hLeftMenu, left_text);

	wchar_t right_text[64];
	StringCchPrintf(right_text, 64, loc_str(LOC_RIGHT_FMT), configs[MOUSE_BUTTON_RIGHT].threshold_ms);
	UINT right_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (configs[MOUSE_BUTTON_RIGHT].monitored)
		right_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, right_menu_flags, (UINT_PTR)hRightMenu, right_text);

	wchar_t middle_text[64];
	StringCchPrintf(middle_text, 64, loc_str(LOC_MIDDLE_FMT), configs[MOUSE_BUTTON_MIDDLE].threshold_ms);
	UINT middle_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (configs[MOUSE_BUTTON_MIDDLE].monitored)
		middle_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, middle_menu_flags, (UINT_PTR)hMiddleMenu, middle_text);

	wchar_t x1_text[64];
	StringCchPrintf(x1_text, 64, loc_str(LOC_X1_FMT), configs[MOUSE_BUTTON_X1].threshold_ms);
	UINT x1_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (configs[MOUSE_BUTTON_X1].monitored)
		x1_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, x1_menu_flags, (UINT_PTR)hX1Menu, x1_text);

	wchar_t x2_text[64];
	StringCchPrintf(x2_text, 64, loc_str(LOC_X2_FMT), configs[MOUSE_BUTTON_X2].threshold_ms);
	UINT x2_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (configs[MOUSE_BUTTON_X2].monitored)
		x2_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, x2_menu_flags, (UINT_PTR)hX2Menu, x2_text);

	wchar_t wheel_text[64];
	StringCchPrintf(wheel_text, 64, loc_str(LOC_WHEEL_FMT), configs[MOUSE_BUTTON_WHEEL].threshold_ms);
	UINT wheel_menu_flags = MF_BYPOSITION | MF_POPUP;
	if (configs[MOUSE_BUTTON_WHEEL].monitored)
		wheel_menu_flags |= MF_CHECKED;
	InsertMenu(manager->menu, -1, wheel_menu_flags, (UINT_PTR)hWheelMenu, wheel_text);

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add control section
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING, IDM_TOGGLE_ENABLE,
		   is_enabled ? loc_str(LOC_DISABLE_ALL) : loc_str(LOC_ENABLE_ALL));

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING, IDM_RESET_STATS, loc_str(LOC_RESET_STATS));

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add presets submenu, marking the set that is currently in effect
	HMENU hPresets = CreatePopupMenu();
	const LocId preset_labels[DEBOUNCE_PRESET_COUNT] = {
		LOC_PRESET_DEFAULT,
		LOC_PRESET_OFFICE,
		LOC_PRESET_STRICT,
	};
	const UINT preset_ids[DEBOUNCE_PRESET_COUNT] = {
		IDM_PRESET_DEFAULT,
		IDM_PRESET_OFFICE,
		IDM_PRESET_STRICT,
	};
	for (int i = 0; i < DEBOUNCE_PRESET_COUNT; i++)
	{
		UINT preset_flags = MF_BYPOSITION | MF_STRING;
		if (debounce_matches_preset(debounce, &DEBOUNCE_PRESETS[i]))
			preset_flags |= MF_CHECKED;
		InsertMenu(hPresets, -1, preset_flags, preset_ids[i], loc_str(preset_labels[i]));
	}
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hPresets, loc_str(LOC_PRESETS));

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add About submenu. The only link is the Store page; it must not send
	// users to the free repository, and nothing else leaves the machine.
	HMENU hAbout = CreatePopupMenu();

	wchar_t version_text[64];
	StringCchPrintf(version_text, 64, L"v%d.%d.%d", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, IDM_ABOUT_VERSION, version_text);

	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, 0, loc_str(LOC_ABOUT_TAGLINE));
	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, 0, loc_str(LOC_ABOUT_PRIVACY));
	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_STRING, IDM_ABOUT_OPEN_STORE, loc_str(LOC_ABOUT_STORE));

	InsertMenu(hAbout, -1, MF_BYPOSITION | MF_STRING | MF_GRAYED, 0, loc_str(LOC_ABOUT_COPYRIGHT));

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hAbout, loc_str(LOC_ABOUT));

	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

	// Add exit
	InsertMenu(manager->menu, -1, MF_BYPOSITION | MF_STRING, IDM_EXIT, loc_str(LOC_EXIT));

	return true;
}

// Show context menu at specified position
bool context_menu_show(ContextMenuManager *manager, HWND hwnd, int x, int y, DebounceManager *debounce, const StoreLicenseInfo *license)
{
	if (!manager || !hwnd)
		return false;

	if (!context_menu_create(manager, debounce, license))
		return false;

	SetForegroundWindow(hwnd);
	TrackPopupMenu(manager->menu, TPM_BOTTOMALIGN, x, y, 0, hwnd, NULL);
	// Without this the menu can stay on screen after a click elsewhere.
	PostMessage(hwnd, WM_NULL, 0, 0);

	return true;
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