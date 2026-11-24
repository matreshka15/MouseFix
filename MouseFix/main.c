#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define STRICT
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <Windowsx.h>
#include <Commctrl.h>
#include <shellapi.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>
#include <stdlib.h>
#include <time.h>
#include <Strsafe.h>
#include <stdio.h>
#include "resource.h"
#include "menu_ids.h"
#include "version.h"

// Include modular headers
#include "src/core/mouse_hook.h"
#include "src/core/debouncer.h"
#include "src/core/settings_migration.h"
#include "src/core/time_manager.h"
#include "src/ui/tray_icon.h"
#include "src/ui/context_menu.h"
#include "src/utils/logger.h"
#include "src/utils/localization.h"
#include "src/utils/store_license.h"
#include "src/utils/review_prompt.h"

// Global application state
typedef struct
{
	HINSTANCE hInstance;
	HWND hWnd;
	HANDLE mutex;

	// Modules
	MouseHookManager mouse_hook;
	DebounceManager debounce;
	TimeManager time_manager;
	TrayIconManager tray_icon;
	ContextMenuManager context_menu;
	Logger logger;

	// First failure during setup, shown once the window cannot be created.
	LocId init_error;
	DWORD init_error_code;

	// The tray may not be ready yet when the app starts with Windows;
	// retry adding the icon instead of exiting.
	bool tray_retry_pending;
	bool first_run_hint_pending;
	bool release_timer_active;     // the 15ms release timer is running; do not reset it per event

	// Set only when InitializeApp got all the way through. A failed setup
	// still cleans up on WM_DESTROY but must not post WM_QUIT: wWinMain is
	// about to show the reason.
	bool initialized;

	// Store license and review state, filled in asynchronously at startup.
	StoreLicenseInfo license;
	bool license_pending;
	uint64_t cumulative_blocks;    // filtered clicks, survives "Reset Statistics"
	uint64_t last_seen_blocks;     // debouncer total at the last sample
	uint64_t first_run_unix_seconds; // 0 when never recorded
	bool review_prompted;          // persisted, shown once ever
	bool review_shown_this_run;    // guard against a double prompt in one run
	bool trial_expired_notified;   // persisted, one balloon only
} AppState;

static AppState g_app = {0};

// Constants
#define MUTEX_GUID L"{05B95384-625D-491A-A326-94758957C021}"
#define INPUT_BOX_CLASS_NAME L"InputBoxClass"
#define INPUT_BOX_WIDTH 320
#define INPUT_BOX_HEIGHT 160
#define INPUT_BOX_PROMPT_BUFFER_SIZE 128
#define INPUT_BOX_INPUT_BUFFER_SIZE 32
#define INPUT_BOX_ERROR_BUFFER_SIZE 1024
#define THRESHOLD_MIN_VALUE 1
#define THRESHOLD_MAX_VALUE 200
#define REG_SETTINGS_KEY L"Software\\MouseFix"
#define WINDOW_CLASS_NAME L"MouseDebouncerWndClass"
#define STORE_PAGE_URL L"https://apps.microsoft.com/detail/9PD5WFXK17TT"
#define TRIAL_PURCHASE_URL L"ms-windows-store://pdp/?ProductId=9PD5WFXK17TT"
#define REVIEW_URL L"ms-windows-store://review/?ProductId=9PD5WFXK17TT"
#define REVIEW_RECHECK_INTERVAL_MS 21600000 /* 6 hours */

// Function declarations
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK OnMouseHookCallback(const MouseEvent *event, void *user_data);
static bool RegisterInvisibleClass(const HINSTANCE hInstance);
static bool InitializeApp(void);
static void ShutdownApp(void);
static void ShowContextMenu(const HWND hWnd, const int x, const int y);
static void OpenStorePage(void);
static void CDECL ShowErrorMessageBox(LPCWSTR message, ...);
static void ToggleEnableAll(void);
static void ToggleButton(MouseButton button);
static void ToggleWheel(void);
static void ToggleSmartDrag(MouseButton button);
static void ApplyPreset(const DebouncePreset *preset);
static void SetButtonThreshold(MouseButton button, int threshold_ms);
static bool InputBox(LPCWSTR prompt, LPWSTR buffer, int buffer_size, int initial_value, int min_value, int max_value);
static void MaybeShowFirstRunHint(void);
static void SaveSettings(void);
static bool LoadSettings(void);
static DWORD WINAPI LicenseQueryThread(LPVOID param);
static void MaybeShowReviewPrompt(void);
static bool ShowReviewPrompt(void);
static void OpenReviewPage(void);
static void OpenTrialPurchasePage(void);
static void AccumulateCumulativeBlocks(void);

// Mouse hook callback
static LRESULT CALLBACK OnMouseHookCallback(const MouseEvent *event, void *user_data)
{
	AppState *app = (AppState *)user_data;

	// Trial over and not purchased: filter nothing, pass every event through.
	if (app->license.state == STORE_LICENSE_EXPIRED)
		return 0;

	// Process event directly - avoid creating intermediate structure
	bool blocked = debounce_process_event(&app->debounce, event);

	// A release may now be held back for confirmation; make sure the timer
	// that flushes it is running. SetTimer with the same id restarts the
	// countdown, so only arm it when it is not already running; otherwise a
	// continuous stream of events could keep pushing the release back.
	if (debounce_has_pending_releases(&app->debounce) && !app->release_timer_active)
	{
		if (SetTimer(app->hWnd, 1, 15, NULL))
			app->release_timer_active = true;
	}

	if (blocked)
		AccumulateCumulativeBlocks();

	// Non-zero means "block it"; the hook procedure calls CallNextHookEx
	// with the original arguments.
	return blocked ? 1 : 0;
}

// Initialize application
static bool InitializeApp(void)
{
#ifndef NDEBUG
	// Initialize logger (only in Debug mode)
	if (!logger_init(&g_app.logger, LOG_LEVEL_INFO, "mouse_debouncer.log"))
	{
		MessageBox(NULL, L"Failed to initialize logger", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	LOG_INFO(&g_app.logger, "MouseFix starting...");
#endif

	// Initialize time manager
	if (!time_manager_init(&g_app.time_manager))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize time manager");
#endif
		g_app.init_error = LOC_ERR_INIT;
		g_app.init_error_code = GetLastError();
		return false;
	}

	// Initialize debounce manager
	if (!debounce_init(&g_app.debounce))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize debounce manager");
#endif
		g_app.init_error = LOC_ERR_INIT;
		g_app.init_error_code = GetLastError();
		return false;
	}

	// Load saved settings; debounce_init filled in the defaults for any missing value.
	bool first_run = !LoadSettings();

	// Record the first run so the review prompt's 7-day trigger has a date.
	if (g_app.first_run_unix_seconds == 0)
		g_app.first_run_unix_seconds = (uint64_t)time(NULL);

	// Initialize mouse hook
	if (!mouse_hook_init(&g_app.mouse_hook, OnMouseHookCallback, &g_app))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize mouse hook");
#endif
		g_app.init_error = LOC_ERR_HOOK;
		g_app.init_error_code = GetLastError();
		return false;
	}

	// Give the hook the shared time source (microseconds)
	g_app.mouse_hook.time_manager = &g_app.time_manager;

	// Install mouse hook
	if (!mouse_hook_install(&g_app.mouse_hook))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to install mouse hook");
#endif
		g_app.init_error = LOC_ERR_HOOK;
		g_app.init_error_code = GetLastError();
		return false;
	}

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Mouse hook installed successfully");
#endif

	// Initialize tray icon
	if (!tray_icon_init(&g_app.tray_icon, g_app.hWnd, g_app.hInstance, IDI_NOTIFYICON))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize tray icon");
#endif
		g_app.init_error = LOC_ERR_INIT;
		g_app.init_error_code = GetLastError();
		return false;
	}

	// Add tray icon. The notification area may still be starting when this
	// app launches at login, so keep retrying instead of exiting.
	if (tray_icon_add(&g_app.tray_icon))
	{
		tray_icon_set_tooltip(&g_app.tray_icon, loc_str(LOC_TOOLTIP));
	}
	else
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to add tray icon, will retry");
#endif
		g_app.tray_retry_pending = true;
		SetTimer(g_app.hWnd, 2, 2000, NULL);
	}

	g_app.first_run_hint_pending = first_run;
	MaybeShowFirstRunHint();

	// Query the Store license on a worker thread; it posts back when done.
	// Re-check the review trigger periodically so a 7-day session still fires.
	g_app.license_pending = true;
	HANDLE lic = CreateThread(NULL, 0, LicenseQueryThread, NULL, 0, NULL);
	if (!lic)
	{
		// Thread failed: run as full version and still do the review check.
		g_app.license_pending = false;
		g_app.license.state = STORE_LICENSE_UNKNOWN;
		g_app.license.days_remaining = 0;
		PostMessage(g_app.hWnd, WM_MOUSEFIX_LICENSE, (WPARAM)g_app.license.state, (LPARAM)g_app.license.days_remaining);
	}
	else
	{
		CloseHandle(lic);
	}
	SetTimer(g_app.hWnd, 3, REVIEW_RECHECK_INTERVAL_MS, NULL);

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Application initialized successfully");
#endif
	// The deferred-release timer is started on demand from the hook callback
	// whenever a release is held back, and stopped once none are pending.
	g_app.initialized = true;
	return true;
}

// Show the first-run hint once the icon is really in the tray, then save the
// settings key so it is not shown again.
static void MaybeShowFirstRunHint(void)
{
	if (!g_app.first_run_hint_pending || !g_app.tray_icon.added)
		return;

	tray_icon_show_balloon(&g_app.tray_icon,
						   loc_str(LOC_FIRST_RUN_TITLE),
						   loc_str(LOC_FIRST_RUN_TEXT),
						   NIIF_INFO, 8000);
	SaveSettings();
	g_app.first_run_hint_pending = false;
}

// Shutdown application
static void ShutdownApp(void)
{
#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Shutting down application...");
#endif

	// Stop the deferred-release timer, the tray retry timer and the review re-check
	KillTimer(g_app.hWnd, 1);
	g_app.release_timer_active = false;
	KillTimer(g_app.hWnd, 2);
	KillTimer(g_app.hWnd, 3);

	// Persist the cumulative block count and any review/trial flags. Only on a
	// fully-initialized run: a failed setup must not clobber saved settings.
	if (g_app.initialized)
		SaveSettings();

	// Release the popup menu, if one was built
	context_menu_destroy(&g_app.context_menu);

	// Remove tray icon
	tray_icon_remove(&g_app.tray_icon);

	// Uninstall mouse hook
	mouse_hook_uninstall(&g_app.mouse_hook);

	// The hook is off, so any held-back release is now safe to send directly.
	debounce_flush_pending_releases(&g_app.debounce);

	// Cleanup modules
	debounce_cleanup(&g_app.debounce);
#ifndef NDEBUG
	logger_cleanup(&g_app.logger);
#endif

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Application shutdown complete");
#endif
}

// Toggle Smart Drag for one button. It is per button because the right
// button is often used for gestures, where holding the release back is
// exactly the wrong thing to do.
static void ToggleSmartDrag(MouseButton button)
{
	if (button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	DebounceButtonConfig config = debounce_get_button_config(&g_app.debounce, button);
	debounce_set_smart_drag(&g_app.debounce, button, !config.smart_drag_enabled);
	SaveSettings();
#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "%s Smart Drag %s", debounce_get_button_name(button),
			 !config.smart_drag_enabled ? "enabled" : "disabled");
#endif
}

static void ToggleEnableAll(void)
{
	bool is_enabled = debounce_is_any_monitored(&g_app.debounce);

	// Toggle all buttons
	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		debounce_set_monitored(&g_app.debounce, i, !is_enabled);
	}

	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "All buttons %s", !is_enabled ? "enabled" : "disabled");
#endif
}

// Toggle individual button
static void ToggleButton(MouseButton button)
{
	if (button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	bool current_state = debounce_get_button_config(&g_app.debounce, button).monitored;
	debounce_set_monitored(&g_app.debounce, button, !current_state);

	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "%s button %s",
			 debounce_get_button_name(button),
			 !current_state ? "enabled" : "disabled");
#endif
}

// Toggle wheel scrolling
static void ToggleWheel(void)
{
	bool current_state = debounce_get_button_config(&g_app.debounce, MOUSE_BUTTON_WHEEL).monitored;
	debounce_set_monitored(&g_app.debounce, MOUSE_BUTTON_WHEEL, !current_state);

	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Wheel scrolling %s", !current_state ? "enabled" : "disabled");
#endif
}

// Apply preset configuration
static void ApplyPreset(const DebouncePreset *preset)
{
	if (!preset)
		return;

	debounce_apply_preset(&g_app.debounce, preset);
	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Applied preset: L=%ums, R=%ums, M=%ums, X1=%ums, X2=%ums, W=%ums",
			 preset->left, preset->right, preset->middle, preset->x1, preset->x2, preset->wheel);
#endif
}

// Simple input box dialog state
typedef struct
{
	LPWSTR buffer;
	int buffer_size;
	int min_value;
	int max_value;
	bool ok_pressed;
} InputBoxState;

static int scale_for_dpi(int value, UINT dpi)
{
	return MulDiv(value, (int)dpi, 96);
}

// Enter and Esc in a plain child edit do not reach the parent by themselves.
static LRESULT CALLBACK InputEditProc(HWND hEdit, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclass_id, DWORD_PTR ref_data)
{
	(void)subclass_id;
	(void)ref_data;

	if (message == WM_KEYDOWN && wParam == VK_RETURN)
	{
		PostMessageW(GetParent(hEdit), WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
		return 0;
	}
	if (message == WM_KEYDOWN && wParam == VK_ESCAPE)
	{
		PostMessageW(GetParent(hEdit), WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
		return 0;
	}
	if (message == WM_CHAR && (wParam == VK_RETURN || wParam == VK_ESCAPE))
		return 0;

	return DefSubclassProc(hEdit, message, wParam, lParam);
}

// Input box window procedure
static LRESULT CALLBACK InputBoxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	InputBoxState *state = (InputBoxState *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			HWND hEdit = GetDlgItem(hWnd, 1001);
			if (hEdit && state)
			{
				GetWindowTextW(hEdit, state->buffer, state->buffer_size);
				wchar_t *end = NULL;
				long value = wcstol(state->buffer, &end, 10);
				if (end == state->buffer || *end != L'\0' ||
					value < state->min_value || value > state->max_value)
				{
					wchar_t text[INPUT_BOX_ERROR_BUFFER_SIZE];
					StringCchPrintfW(text, ARRAYSIZE(text), loc_str(LOC_INVALID_THRESHOLD),
									 state->min_value, state->max_value);
					MessageBoxW(hWnd, text, L"MouseFix", MB_OK | MB_ICONWARNING);
					SetFocus(hEdit);
					SendMessageW(hEdit, EM_SETSEL, 0, -1);
					return 0;
				}
				state->ok_pressed = true;
				DestroyWindow(hWnd);
			}
			return 0;
		}
		if (LOWORD(wParam) == IDCANCEL)
		{
			if (state)
				state->ok_pressed = false;
			DestroyWindow(hWnd);
			return 0;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;
	}
	return DefWindowProcW(hWnd, message, wParam, lParam);
}

// Simple input box dialog
static bool InputBox(LPCWSTR prompt, LPWSTR buffer, int buffer_size, int initial_value, int min_value, int max_value)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	InputBoxState state = {buffer, buffer_size, min_value, max_value, false};
	UINT dpi = GetDpiForSystem();

	// Register a simple window class if not already registered
	WNDCLASSW wc = {0};
	wc.lpfnWndProc = InputBoxProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszClassName = INPUT_BOX_CLASS_NAME;
	RegisterClassW(&wc);

	// Create dialog window
	HWND hWnd = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW,
		INPUT_BOX_CLASS_NAME, loc_str(LOC_SET_THRESHOLD),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT,
		scale_for_dpi(INPUT_BOX_WIDTH, dpi), scale_for_dpi(INPUT_BOX_HEIGHT, dpi),
		NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return false;
	}

	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)&state);

	// Center the window
	RECT rc;
	GetWindowRect(hWnd, &rc);
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	SetWindowPos(hWnd, NULL,
				 (screenWidth - (rc.right - rc.left)) / 2,
				 (screenHeight - (rc.bottom - rc.top)) / 2,
				 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	// Create label
	HWND hLabel = CreateWindowExW(0, L"STATIC", prompt,
								  WS_CHILD | WS_VISIBLE | SS_LEFT,
								  scale_for_dpi(20, dpi), scale_for_dpi(20, dpi),
								  scale_for_dpi(280, dpi), scale_for_dpi(20, dpi),
								  hWnd, NULL, hInstance, NULL);

	// Create edit box
	HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
								 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
								 scale_for_dpi(20, dpi), scale_for_dpi(50, dpi),
								 scale_for_dpi(280, dpi), scale_for_dpi(25, dpi),
								 hWnd, (HMENU)1001, hInstance, NULL);

	// Create OK button
	HWND hOK = CreateWindowExW(0, L"BUTTON", loc_str(LOC_OK),
							   WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
							   scale_for_dpi(80, dpi), scale_for_dpi(90, dpi),
							   scale_for_dpi(70, dpi), scale_for_dpi(30, dpi),
							   hWnd, (HMENU)IDOK, hInstance, NULL);

	// Create Cancel button
	HWND hCancel = CreateWindowExW(0, L"BUTTON", loc_str(LOC_CANCEL),
								   WS_CHILD | WS_VISIBLE,
								   scale_for_dpi(160, dpi), scale_for_dpi(90, dpi),
								   scale_for_dpi(70, dpi), scale_for_dpi(30, dpi),
								   hWnd, (HMENU)IDCANCEL, hInstance, NULL);

	// Use the system message font so the dialog scales with DPI and locale.
	HFONT hFont = NULL;
	bool own_font = false;
	NONCLIENTMETRICSW metrics;
	ZeroMemory(&metrics, sizeof(metrics));
	metrics.cbSize = sizeof(metrics);
	if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
		hFont = CreateFontIndirectW(&metrics.lfMessageFont);
	if (hFont)
		own_font = true;
	else
		hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessage(hOK, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

	// Start from the current threshold, all selected so typing replaces it.
	wchar_t initial[16];
	StringCchPrintfW(initial, ARRAYSIZE(initial), L"%d", initial_value);
	SetWindowTextW(hEdit, initial);
	SendMessage(hEdit, EM_SETLIMITTEXT, buffer_size - 1, 0);
	SendMessage(hEdit, EM_SETSEL, 0, -1);
	SetWindowSubclass(hEdit, InputEditProc, 1, 0);
	ShowWindow(hWnd, SW_SHOW);
	SetForegroundWindow(hWnd);
	SetFocus(hEdit);

	// Message loop
	MSG msg;
	while (IsWindow(hWnd))
	{
		BOOL got = GetMessage(&msg, NULL, 0, 0);
		if (got == 0)
		{
			// WM_QUIT arrived while this box was open (e.g. the user picked
			// Exit from the tray). Re-post it so the outer wWinMain loop also
			// sees it and the app exits instead of hanging.
			PostQuitMessage((int)msg.wParam);
			break;
		}
		if (got == -1)
			break;

		if (!IsDialogMessage(hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (own_font)
		DeleteObject(hFont);

	return state.ok_pressed;
}

static void SaveSettings(void)
{
	HKEY hKey;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SETTINGS_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
		{
			DebounceButtonConfig config = debounce_get_button_config(&g_app.debounce, (MouseButton)i);
			wchar_t valName[64];
			// Save threshold
			StringCchPrintfW(valName, 64, L"Btn%d_Threshold", i);
			DWORD threshold = (DWORD)config.threshold_ms;
			RegSetValueExW(hKey, valName, 0, REG_DWORD, (BYTE *)&threshold, sizeof(DWORD));

			// Save enabled state
			StringCchPrintfW(valName, 64, L"Btn%d_Enabled", i);
			DWORD enabled = config.monitored ? 1 : 0;
			RegSetValueExW(hKey, valName, 0, REG_DWORD, (BYTE *)&enabled, sizeof(DWORD));

			// Save per-button Smart Drag
			StringCchPrintfW(valName, 64, L"Btn%d_SmartDrag", i);
			DWORD smart_drag = config.smart_drag_enabled ? 1 : 0;
			RegSetValueExW(hKey, valName, 0, REG_DWORD, (BYTE *)&smart_drag, sizeof(DWORD));
		}

		// The old global switch has been replaced by the per-button values.
		RegDeleteValueW(hKey, L"HybridHeuristic");

		// Review / trial bookkeeping.
		RegSetValueExW(hKey, L"CumulativeBlocks", 0, REG_QWORD, (BYTE *)&g_app.cumulative_blocks, sizeof(uint64_t));
		RegSetValueExW(hKey, L"InstallTime", 0, REG_QWORD, (BYTE *)&g_app.first_run_unix_seconds, sizeof(uint64_t));
		DWORD review = g_app.review_prompted ? 1 : 0;
		RegSetValueExW(hKey, L"ReviewPrompted", 0, REG_DWORD, (BYTE *)&review, sizeof(DWORD));
		DWORD trial_notified = g_app.trial_expired_notified ? 1 : 0;
		RegSetValueExW(hKey, L"TrialExpiredNotified", 0, REG_DWORD, (BYTE *)&trial_notified, sizeof(DWORD));

		RegCloseKey(hKey);
	}
}

static bool LoadSettings(void)
{
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SETTINGS_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return false;

	DWORD value;
	DWORD size = sizeof(DWORD);

	// Builds before 1.0.9 had one global Smart Drag switch. Read it once so
	// existing installs keep their behavior, then replace it with per-button
	// values below and delete the old value.
	bool legacy_hybrid_present = false;
	bool legacy_hybrid_enabled = false;
	if (RegQueryValueExW(hKey, L"HybridHeuristic", NULL, NULL, (BYTE *)&value, &size) == ERROR_SUCCESS)
	{
		legacy_hybrid_present = true;
		legacy_hybrid_enabled = (value != 0);
	}

	// Load review / trial bookkeeping (missing values keep their zero default).
	uint64_t qvalue;
	DWORD qsize = sizeof(uint64_t);
	if (RegQueryValueExW(hKey, L"CumulativeBlocks", NULL, NULL, (BYTE *)&qvalue, &qsize) == ERROR_SUCCESS)
		g_app.cumulative_blocks = qvalue;

	qsize = sizeof(uint64_t);
	if (RegQueryValueExW(hKey, L"InstallTime", NULL, NULL, (BYTE *)&qvalue, &qsize) == ERROR_SUCCESS)
		g_app.first_run_unix_seconds = qvalue;

	size = sizeof(DWORD);
	if (RegQueryValueExW(hKey, L"ReviewPrompted", NULL, NULL, (BYTE *)&value, &size) == ERROR_SUCCESS)
		g_app.review_prompted = (value != 0);

	size = sizeof(DWORD);
	if (RegQueryValueExW(hKey, L"TrialExpiredNotified", NULL, NULL, (BYTE *)&value, &size) == ERROR_SUCCESS)
		g_app.trial_expired_notified = (value != 0);

	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		wchar_t valName[64];

		// Load threshold
		size = sizeof(DWORD);
		StringCchPrintfW(valName, 64, L"Btn%d_Threshold", i);
		if (RegQueryValueExW(hKey, valName, NULL, NULL, (BYTE *)&value, &size) == ERROR_SUCCESS)
		{
			debounce_set_threshold(&g_app.debounce, i, (int)value, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);
		}

		// Load enabled state
		size = sizeof(DWORD);
		StringCchPrintfW(valName, 64, L"Btn%d_Enabled", i);
		if (RegQueryValueExW(hKey, valName, NULL, NULL, (BYTE *)&value, &size) == ERROR_SUCCESS)
		{
			debounce_set_monitored(&g_app.debounce, i, value != 0);
		}

		// Load per-button Smart Drag, or migrate the old global switch.
		// The old build only ever enabled Smart Drag for the left button.
		size = sizeof(DWORD);
		StringCchPrintfW(valName, 64, L"Btn%d_SmartDrag", i);
		bool has_per_button = RegQueryValueExW(hKey, valName, NULL, NULL, (BYTE *)&value, &size) == ERROR_SUCCESS;
		debounce_set_smart_drag(&g_app.debounce, i,
			settings_resolve_smart_drag(has_per_button, has_per_button && value != 0,
										legacy_hybrid_present, legacy_hybrid_enabled,
										i == MOUSE_BUTTON_LEFT));
	}

	// The legacy value stays until SaveSettings has written the per-button
	// values and deleted it. If the process dies before that, the next start
	// still finds the old value and migrates again instead of silently
	// falling back to the fresh-install defaults.
	RegCloseKey(hKey);
	return true;
}

// Set threshold for a specific button
static void SetButtonThreshold(MouseButton button, int threshold_ms)
{
	if (button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	debounce_set_threshold(&g_app.debounce, button, threshold_ms, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);

	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "%s threshold set to %dms",
			 debounce_get_button_name(button),
			 threshold_ms);
#endif
}

// Register invisible window class
static bool RegisterInvisibleClass(const HINSTANCE hInstance)
{
	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = (WNDPROC)WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	return RegisterClassEx(&wc) != 0;
}

// Window procedure
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		g_app.hWnd = hWnd;
		if (!InitializeApp())
			return -1;

		return 0;

	case WM_TIMER:
		if (wParam == 1)
		{
			debounce_check_deferred_releases(&g_app.debounce);
			// Stop the timer once nothing is held back, so the app does not
			// keep waking every 15ms when there is no pending release.
			if (!debounce_has_pending_releases(&g_app.debounce))
			{
				KillTimer(hWnd, 1);
				g_app.release_timer_active = false;
			}
		}
		else if (wParam == 2 && g_app.tray_retry_pending)
		{
			if (tray_icon_add(&g_app.tray_icon))
			{
				g_app.tray_retry_pending = false;
				KillTimer(hWnd, 2);
				tray_icon_set_tooltip(&g_app.tray_icon, loc_str(LOC_TOOLTIP));
				MaybeShowFirstRunHint();
			}
		}
		else if (wParam == 3)
		{
			// Periodic review re-check so a 7-day session still fires.
			MaybeShowReviewPrompt();
		}
		return 0;

	case WM_NOTIFYICON:
		// NOTIFYICON_VERSION_4 puts the event in the low word and screen
		// coordinates in wParam. Left click and Enter open the menu too, so
		// the icon does something when people click it the obvious way.
		switch (LOWORD(lParam))
		{
		case WM_CONTEXTMENU:
		case NIN_SELECT:
		case NIN_KEYSELECT:
			ShowContextMenu(hWnd, GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
			return 0;
		}
		break;

	case WM_MOUSEFIX_SHOW:
		// A second copy was launched: point at the icon already in the tray.
		tray_icon_show_balloon(&g_app.tray_icon,
							   loc_str(LOC_ALREADY_RUNNING),
							   loc_str(LOC_ALREADY_RUNNING_TEXT),
							   NIIF_INFO, 5000);
		return 0;

	case WM_MOUSEFIX_LICENSE:
		{
			g_app.license.state = (StoreLicenseState)wParam;
			g_app.license.days_remaining = (int)lParam;
			g_app.license_pending = false;

			if (g_app.license.state == STORE_LICENSE_EXPIRED)
			{
				if (!g_app.trial_expired_notified)
				{
					tray_icon_show_balloon(&g_app.tray_icon,
										   loc_str(LOC_TRIAL_EXPIRED_TITLE),
										   loc_str(LOC_TRIAL_EXPIRED_TEXT),
										   NIIF_WARNING, 8000);
					g_app.trial_expired_notified = true;
					SaveSettings();
				}
			}
			else
			{
				if (g_app.license.state == STORE_LICENSE_UNKNOWN)
					OutputDebugStringW(L"MouseFix: Store license unavailable, running as full version.\n");
				// Full, trial, or unknown all keep filtering; check the review
				// trigger now that the trial state is known.
				MaybeShowReviewPrompt();
			}
		}
		return 0;

	case WM_MOUSEFIX_REVIEW:
		MaybeShowReviewPrompt();
		return 0;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDM_ABOUT_OPEN_STORE)
		{
			OpenStorePage();
			return 0;
		}
		if (LOWORD(wParam) == IDM_TRIAL_BUY)
		{
			OpenTrialPurchasePage();
			return 0;
		}
		if (LOWORD(wParam) == IDM_EXIT)
		{
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			return 0;
		}
		if (LOWORD(wParam) == IDM_TOGGLE_ENABLE)
		{
			ToggleEnableAll();
			return 0;
		}
		if (LOWORD(wParam) >= IDM_SMART_DRAG_BASE &&
			LOWORD(wParam) < IDM_SMART_DRAG_BASE + MOUSE_BUTTON_WHEEL)
		{
			ToggleSmartDrag((MouseButton)(LOWORD(wParam) - IDM_SMART_DRAG_BASE));
			return 0;
		}
		if (LOWORD(wParam) == IDM_RESET_STATS)
		{
			debounce_reset_statistics(&g_app.debounce);
			// The debouncer's counters start again at zero. Drop the sampling
			// baseline too, or the first blocks after a reset are lost.
			g_app.last_seen_blocks = 0;
#ifndef NDEBUG
			LOG_INFO(&g_app.logger, "Statistics reset");
#endif
			return 0;
		}
		if (LOWORD(wParam) == IDM_PRESET_DEFAULT)
		{
			ApplyPreset(&DEBOUNCE_PRESETS[DEBOUNCE_PRESET_DEFAULT]);
			return 0;
		}
		if (LOWORD(wParam) == IDM_PRESET_OFFICE)
		{
			ApplyPreset(&DEBOUNCE_PRESETS[DEBOUNCE_PRESET_OFFICE]);
			return 0;
		}
		if (LOWORD(wParam) == IDM_PRESET_STRICT)
		{
			ApplyPreset(&DEBOUNCE_PRESETS[DEBOUNCE_PRESET_STRICT]);
			return 0;
		}
		// Individual button toggles
		if (LOWORD(wParam) == IDM_TOGGLE_LEFT)
		{
			ToggleButton(MOUSE_BUTTON_LEFT);
			return 0;
		}
		if (LOWORD(wParam) == IDM_TOGGLE_RIGHT)
		{
			ToggleButton(MOUSE_BUTTON_RIGHT);
			return 0;
		}
		if (LOWORD(wParam) == IDM_TOGGLE_MIDDLE)
		{
			ToggleButton(MOUSE_BUTTON_MIDDLE);
			return 0;
		}
		if (LOWORD(wParam) == IDM_TOGGLE_X1)
		{
			ToggleButton(MOUSE_BUTTON_X1);
			return 0;
		}
		if (LOWORD(wParam) == IDM_TOGGLE_X2)
		{
			ToggleButton(MOUSE_BUTTON_X2);
			return 0;
		}
		if (LOWORD(wParam) == IDM_TOGGLE_WHEEL)
		{
			ToggleWheel();
			return 0;
		}

		// Quick threshold choices, laid out as base + button * stride + index.
		{
			UINT menu_id = LOWORD(wParam);
			if (menu_id >= IDM_THRESHOLD_BASE &&
				menu_id < IDM_THRESHOLD_BASE + MOUSE_BUTTON_COUNT * IDM_THRESHOLD_STRIDE)
			{
				int button_id = (int)(menu_id - IDM_THRESHOLD_BASE) / IDM_THRESHOLD_STRIDE;
				int threshold_index = (int)(menu_id - IDM_THRESHOLD_BASE) % IDM_THRESHOLD_STRIDE;

				if (threshold_index < BUTTON_THRESHOLD_COUNT)
				{
					const int *thresholds = (button_id == MOUSE_BUTTON_WHEEL) ? WHEEL_THRESHOLDS : BUTTON_THRESHOLDS;
					SetButtonThreshold((MouseButton)button_id, thresholds[threshold_index]);
					return 0;
				}
			}
		}

		// Custom threshold input
		{
			UINT menu_id = LOWORD(wParam);
			if (menu_id >= IDM_THRESHOLD_CUSTOM_BASE &&
				menu_id < IDM_THRESHOLD_CUSTOM_BASE + MOUSE_BUTTON_COUNT)
			{
				int button_id = (int)(menu_id - IDM_THRESHOLD_CUSTOM_BASE);
				wchar_t input[INPUT_BOX_INPUT_BUFFER_SIZE] = L"";
				wchar_t prompt[INPUT_BOX_PROMPT_BUFFER_SIZE];
				StringCchPrintf(prompt, INPUT_BOX_PROMPT_BUFFER_SIZE, loc_str(LOC_ENTER_THRESHOLD_FMT),
								loc_button_name(button_id), THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);

				DebounceButtonConfig config = debounce_get_button_config(&g_app.debounce, (MouseButton)button_id);
				if (InputBox(prompt, input, _countof(input),
						 (int)config.threshold_ms,
						 THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE))
				{
					SetButtonThreshold((MouseButton)button_id, _wtoi(input));
				}
				return 0;
			}
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		ShutdownApp();
		if (g_app.initialized)
			PostQuitMessage(0);
		return 0;

	default:
		if (uMsg == g_app.tray_icon.taskbar_created_msg)
		{
			tray_icon_restore(&g_app.tray_icon);
			MaybeShowFirstRunHint();
		}
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Show context menu
static void ShowContextMenu(const HWND hWnd, const int x, const int y)
{
	SetForegroundWindow(hWnd);
	context_menu_show(&g_app.context_menu, hWnd, x, y, &g_app.debounce, &g_app.license);
}

// Open the Store listing. This is the only thing in the app that talks to
// the outside, and it only happens when the person picks it from About.
static void OpenStorePage(void)
{
	HINSTANCE result = ShellExecuteW(g_app.hWnd, L"open", STORE_PAGE_URL, NULL, NULL, SW_SHOWNORMAL);
	if ((INT_PTR)result <= 32)
		ShowErrorMessageBox(loc_str(LOC_STORE_OPEN_FAILED));
}

// Open the trial-purchase page (used when the trial has ended).
static void OpenTrialPurchasePage(void)
{
	ShellExecuteW(g_app.hWnd, L"open", TRIAL_PURCHASE_URL, NULL, NULL, SW_SHOWNORMAL);
}

// Open the rate-and-review surface: the native Store dialog when available,
// the review URL otherwise.
static void OpenReviewPage(void)
{
	if (!store_license_request_rate_and_review())
		ShellExecuteW(g_app.hWnd, L"open", REVIEW_URL, NULL, NULL, SW_SHOWNORMAL);
}

// Worker thread: read the Store license, then hand the result back to the
// UI thread. The read may touch the network, so it never runs on the UI thread.
static DWORD WINAPI LicenseQueryThread(LPVOID param)
{
	(void)param;
	StoreLicenseInfo info;
	store_license_query(&info);
	PostMessage(g_app.hWnd, WM_MOUSEFIX_LICENSE, (WPARAM)info.state, (LPARAM)info.days_remaining);
	return 0;
}

// A native dialog with two buttons: "rate now" and "later". Returns false if
// the dialog could not be shown, so the caller can bail without persisting.
static bool ShowReviewPrompt(void)
{
	TASKDIALOG_BUTTON buttons[2] = {
		{ 1, loc_str(LOC_REVIEW_RATE) },
		{ 2, loc_str(LOC_REVIEW_LATER) },
	};

	TASKDIALOGCONFIG cfg = {0};
	cfg.cbSize = sizeof(cfg);
	cfg.hwndParent = g_app.hWnd;
	cfg.pszWindowTitle = L"MouseFix";
	cfg.pszMainInstruction = loc_str(LOC_REVIEW_TITLE);
	cfg.pszContent = loc_str(LOC_REVIEW_TEXT);
	cfg.cButtons = 2;
	cfg.pButtons = buttons;
	cfg.nDefaultButton = 1;

	int result = 0;
	if (SUCCEEDED(TaskDialogIndirect(&cfg, &result, NULL, NULL)) && result == 1)
		OpenReviewPage();

	return true;
}

// Show the review prompt once the trigger holds, and remember that it was
// shown. Never collides with the trial-ended prompt (that one wins).
static void MaybeShowReviewPrompt(void)
{
	if (g_app.review_prompted || g_app.review_shown_this_run)
		return;
	if (g_app.license_pending)
		return;
	if (g_app.license.state == STORE_LICENSE_EXPIRED)
		return;

	ReviewDecisionInput in;
	in.cumulative_blocks = g_app.cumulative_blocks;
	in.first_run_unix_seconds = g_app.first_run_unix_seconds;
	in.already_prompted = g_app.review_prompted;
	in.now_unix_seconds = (uint64_t)time(NULL);

	if (!review_should_prompt(&in))
		return;

	g_app.review_shown_this_run = true;
	g_app.review_prompted = true;

	if (ShowReviewPrompt())
		SaveSettings();
}

// Fold each newly blocked event into the persisted cumulative count. A "Reset
// Statistics" zeroes the debouncer's own counters but not this one, so the
// review trigger keeps its history.
static void AccumulateCumulativeBlocks(void)
{
	uint32_t total = debounce_get_total_blocks(&g_app.debounce);
	if (total >= g_app.last_seen_blocks)
		g_app.cumulative_blocks += (uint64_t)(total - g_app.last_seen_blocks);
	g_app.last_seen_blocks = total;

	if (!g_app.review_prompted && !g_app.license_pending &&
		g_app.license.state != STORE_LICENSE_EXPIRED &&
		g_app.cumulative_blocks >= REVIEW_BLOCK_THRESHOLD)
	{
		PostMessage(g_app.hWnd, WM_MOUSEFIX_REVIEW, 0, 0);
	}
}

// Show error message box
static void CDECL ShowErrorMessageBox(LPCWSTR message, ...)
{
	WCHAR buffer[INPUT_BOX_ERROR_BUFFER_SIZE];
	va_list args;
	va_start(args, message);
	StringCchVPrintf(buffer, ARRAYSIZE(buffer), message, args);
	va_end(args);

	MessageBox(NULL, buffer, L"MouseFix", MB_OK | MB_ICONERROR);
}

// Entry point
int CALLBACK wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR lpCmdLine, _In_ int nCmdShow)
{
	// Keep the tray menu and the threshold dialog sharp on scaled displays.
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// Limit to one instance
	g_app.mutex = CreateMutexW(NULL, TRUE, MUTEX_GUID);
	DWORD mutex_error = GetLastError();

	if (!g_app.mutex)
	{
		ShowErrorMessageBox(loc_str(LOC_ERR_MUTEX), mutex_error);
		return EXIT_FAILURE;
	}

	if (mutex_error == ERROR_ALREADY_EXISTS || mutex_error == ERROR_ACCESS_DENIED)
	{
		HWND existing = FindWindowW(WINDOW_CLASS_NAME, NULL);
		if (existing)
			PostMessageW(existing, WM_MOUSEFIX_SHOW, 0, 0);
		CloseHandle(g_app.mutex);
		return EXIT_SUCCESS;
	}

	g_app.hInstance = hInstance;
	g_app.init_error = LOC_COUNT;

	// Process command line arguments
	// TODO: Implement command line parsing

	// Register window class
	if (!RegisterInvisibleClass(hInstance))
	{
		ShowErrorMessageBox(loc_str(LOC_ERR_CLASS), GetLastError());
		ReleaseMutex(g_app.mutex);
		CloseHandle(g_app.mutex);
		return EXIT_FAILURE;
	}

	// Create window. InitializeApp runs from WM_CREATE, so a failure there
	// leaves the first real cause behind.
	if (!CreateWindowW(WINDOW_CLASS_NAME, L"MouseFix", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL))
	{
		LocId error = g_app.init_error != LOC_COUNT ? g_app.init_error : LOC_ERR_WINDOW;
		DWORD error_code = g_app.init_error != LOC_COUNT ? g_app.init_error_code : GetLastError();
		ShowErrorMessageBox(loc_str(error), error_code);
		ReleaseMutex(g_app.mutex);
		CloseHandle(g_app.mutex);
		return EXIT_FAILURE;
	}

	// Message loop
	MSG msg;
	BOOL ret;
	while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (ret != -1)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			DWORD error = GetLastError();
			ShowErrorMessageBox(loc_str(LOC_ERR_UNEXPECTED), error);
			ShutdownApp();
			ReleaseMutex(g_app.mutex);
			CloseHandle(g_app.mutex);
			return (int)error;
		}
	}

	// Cleanup
	UnregisterClass(WINDOW_CLASS_NAME, hInstance);
	UnregisterClass(INPUT_BOX_CLASS_NAME, hInstance);
	ReleaseMutex(g_app.mutex);
	CloseHandle(g_app.mutex);

	return (int)msg.wParam;
}