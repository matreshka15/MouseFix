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
#include <Strsafe.h>
#include <stdio.h>
#include "resource.h"
#include "menu_ids.h"
#include "version.h"

// Include modular headers
#include "src/core/mouse_hook.h"
#include "src/core/debouncer.h"
#include "src/core/time_manager.h"
#include "src/ui/tray_icon.h"
#include "src/ui/context_menu.h"
#include "src/utils/logger.h"
#include "src/utils/error_handler.h"

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
	ErrorHandler error_handler;

	// Application settings
	bool should_exit;
} AppState;

static AppState g_app = {0};

// Constants
#define MUTEX_GUID L"{05B95384-625D-491A-A326-94758957C021}"
#define INPUT_BOX_CLASS_NAME L"InputBoxClass"
#define INPUT_BOX_TITLE L"Set Threshold"
#define INPUT_BOX_WIDTH 320
#define INPUT_BOX_HEIGHT 160
#define INPUT_BOX_PROMPT_BUFFER_SIZE 128
#define INPUT_BOX_INPUT_BUFFER_SIZE 32
#define INPUT_BOX_ERROR_BUFFER_SIZE 1024
#define THRESHOLD_MIN_VALUE 1
#define THRESHOLD_MAX_VALUE 200
#define REG_SETTINGS_KEY L"Software\\MouseFix"

// Preset configuration structure
typedef struct
{
	int left;
	int right;
	int middle;
	int x1;
	int x2;
	int wheel;
} PresetConfig;

// Preset definitions
static const PresetConfig PRESET_DEFAULT = {50, 50, 50, 50, 50, 30};
static const PresetConfig PRESET_OFFICE = {60, 60, 60, 60, 60, 35};
static const PresetConfig PRESET_STRICT = {40, 40, 40, 40, 40, 20};

// Function declarations
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK OnMouseHookCallback(const MouseEvent *event, void *user_data);
static bool RegisterInvisibleClass(const HINSTANCE hInstance);
static bool InitializeApp(void);
static void ShutdownApp(void);
static void ShowContextMenu(const HWND hWnd, const int x, const int y);
static void CDECL ShowErrorMessageBox(LPCWSTR message, ...);
static void ToggleEnableAll(void);
static void ToggleButton(MouseButton button);
static void ToggleWheel(void);
static void ToggleHybridHeuristic(void);
static void ApplyPreset(const PresetConfig *preset);
static void SetButtonThreshold(MouseButton button, int threshold_ms);
static bool InputBox(LPCWSTR prompt, LPWSTR buffer, int buffer_size);
static void SaveSettings(void);
static void LoadSettings(void);

// Mouse hook callback
static LRESULT CALLBACK OnMouseHookCallback(const MouseEvent *event, void *user_data)
{
	AppState *app = (AppState *)user_data;

	// Process event directly - avoid creating intermediate structure
	if (debounce_process_event(&app->debounce, event))
	{
		// Event should be blocked
		return 1;
	}

	// Event should pass through
	return CallNextHookEx(NULL, 0, 0, 0);
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

	// Initialize error handler
	if (!error_handler_init(&g_app.error_handler))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize error handler");
#endif
		return false;
	}

	// Initialize time manager
	if (!time_manager_init(&g_app.time_manager))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize time manager");
#endif
		return false;
	}

	// Initialize debounce manager
	if (!debounce_init(&g_app.debounce))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize debounce manager");
#endif
		return false;
	}

	// Apply Default Preset (50ms for buttons, 30ms for wheel)
	// We use a silent version of ApplyPreset for initialization to avoid redundant SaveSettings
	g_app.debounce.buttons[MOUSE_BUTTON_LEFT].thresholdMs = PRESET_DEFAULT.left;
	g_app.debounce.buttons[MOUSE_BUTTON_RIGHT].thresholdMs = PRESET_DEFAULT.right;
	g_app.debounce.buttons[MOUSE_BUTTON_MIDDLE].thresholdMs = PRESET_DEFAULT.middle;
	g_app.debounce.buttons[MOUSE_BUTTON_X1].thresholdMs = PRESET_DEFAULT.x1;
	g_app.debounce.buttons[MOUSE_BUTTON_X2].thresholdMs = PRESET_DEFAULT.x2;
	g_app.debounce.buttons[MOUSE_BUTTON_WHEEL].thresholdMs = PRESET_DEFAULT.wheel;

	// Load saved settings from Registry (overwrites defaults if they exist)
	LoadSettings();

	// Initialize mouse hook
	if (!mouse_hook_init(&g_app.mouse_hook, OnMouseHookCallback, &g_app))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize mouse hook");
#endif
		return false;
	}

	// Install mouse hook
	if (!mouse_hook_install(&g_app.mouse_hook))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to install mouse hook");
#endif
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
		return false;
	}

	// Add tray icon
	if (!tray_icon_add(&g_app.tray_icon))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to add tray icon");
#endif
		return false;
	}

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Application initialized successfully");
#endif
	// Set timer for checking deferred releases (Hybrid Heuristic)
	// Check every 15ms (approx 66Hz) to ensure timely release of dragged items
	SetTimer(g_app.hWnd, 1, 15, NULL);
	return true;
}

// Shutdown application
static void ShutdownApp(void)
{
#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Shutting down application...");
#endif

	// Stop the hybrid heuristic timer
	KillTimer(g_app.hWnd, 1);

	// Remove tray icon
	tray_icon_remove(&g_app.tray_icon);

	// Uninstall mouse hook
	mouse_hook_uninstall(&g_app.mouse_hook);

	// Cleanup modules
	debounce_cleanup(&g_app.debounce);
#ifndef NDEBUG
	logger_cleanup(&g_app.logger);
#endif
	error_handler_cleanup(&g_app.error_handler);

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Application shutdown complete");
#endif
}

// Toggle enable/disable all buttons
// Toggle hybrid heuristic
static void ToggleHybridHeuristic(void)
{
	bool new_state = !g_app.debounce.use_hybrid_heuristic;
	debounce_set_hybrid_heuristic(&g_app.debounce, new_state);
	SaveSettings();
#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Hybrid heuristic set to %s", new_state ? "Enabled" : "Disabled");
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

	bool current_state = g_app.debounce.buttons[button].isMonitored;
	debounce_set_monitored(&g_app.debounce, button, !current_state);

	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "%S button %s",
			 debounce_get_button_name(button),
			 !current_state ? "enabled" : "disabled");
#endif
}

// Toggle wheel scrolling
static void ToggleWheel(void)
{
	bool current_state = g_app.debounce.buttons[MOUSE_BUTTON_WHEEL].isMonitored;
	debounce_set_monitored(&g_app.debounce, MOUSE_BUTTON_WHEEL, !current_state);

	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Wheel scrolling %s", !current_state ? "enabled" : "disabled");
#endif
}

// Apply preset configuration
static void ApplyPreset(const PresetConfig *preset)
{
	if (!preset)
		return;

	// Apply preset thresholds
	debounce_set_threshold(&g_app.debounce, MOUSE_BUTTON_LEFT, preset->left, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);
	debounce_set_threshold(&g_app.debounce, MOUSE_BUTTON_RIGHT, preset->right, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);
	debounce_set_threshold(&g_app.debounce, MOUSE_BUTTON_MIDDLE, preset->middle, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);
	debounce_set_threshold(&g_app.debounce, MOUSE_BUTTON_X1, preset->x1, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);
	debounce_set_threshold(&g_app.debounce, MOUSE_BUTTON_X2, preset->x2, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);
	debounce_set_threshold(&g_app.debounce, MOUSE_BUTTON_WHEEL, preset->wheel, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);

	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Applied preset: L=%dms, R=%dms, M=%dms, X1=%dms, X2=%dms, W=%dms",
			 preset->left, preset->right, preset->middle, preset->x1, preset->x2, preset->wheel);
#endif
}

// Simple input box dialog state
typedef struct
{
	LPWSTR buffer;
	int buffer_size;
	bool ok_pressed;
} InputBoxState;

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
static bool InputBox(LPCWSTR prompt, LPWSTR buffer, int buffer_size)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	InputBoxState state = {buffer, buffer_size, false};

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
		WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
		INPUT_BOX_CLASS_NAME, INPUT_BOX_TITLE,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, INPUT_BOX_WIDTH, INPUT_BOX_HEIGHT,
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
								  20, 20, 280, 20, hWnd, NULL, hInstance, NULL);

	// Create edit box
	HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
								 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
								 20, 50, 280, 25, hWnd, (HMENU)1001, hInstance, NULL);

	// Create OK button
	HWND hOK = CreateWindowExW(0, L"BUTTON", L"OK",
							   WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
							   80, 90, 70, 30, hWnd, (HMENU)IDOK, hInstance, NULL);

	// Create Cancel button
	HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
								   WS_CHILD | WS_VISIBLE,
								   160, 90, 70, 30, hWnd, (HMENU)IDCANCEL, hInstance, NULL);

	// Set fonts
	HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessage(hOK, WM_SETFONT, (WPARAM)hFont, TRUE);
	SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

	// Limit input length
	SendMessage(hEdit, EM_SETLIMITTEXT, buffer_size - 1, 0);
	SetFocus(hEdit);
	ShowWindow(hWnd, SW_SHOW);

	// Message loop
	MSG msg;
	while (IsWindow(hWnd) && GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(hWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return state.ok_pressed;
}

static void SaveSettings(void)
{
	HKEY hKey;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_SETTINGS_KEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
	{
		// Save hybrid heuristic setting
		DWORD hybrid = g_app.debounce.use_hybrid_heuristic ? 1 : 0;
		RegSetValueExW(hKey, L"HybridHeuristic", 0, REG_DWORD, (BYTE *)&hybrid, sizeof(DWORD));

		for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
		{
			wchar_t valName[64];
			// Save threshold
			StringCchPrintfW(valName, 64, L"Btn%d_Threshold", i);
			DWORD threshold = (DWORD)g_app.debounce.buttons[i].thresholdMs;
			RegSetValueExW(hKey, valName, 0, REG_DWORD, (BYTE *)&threshold, sizeof(DWORD));

			// Save enabled state
			StringCchPrintfW(valName, 64, L"Btn%d_Enabled", i);
			DWORD enabled = g_app.debounce.buttons[i].isMonitored ? 1 : 0;
			RegSetValueExW(hKey, valName, 0, REG_DWORD, (BYTE *)&enabled, sizeof(DWORD));
		}
		RegCloseKey(hKey);
	}
}

static void LoadSettings(void)
{
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_SETTINGS_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD size = sizeof(DWORD);
		DWORD hybrid;

		// Load hybrid heuristic setting
		if (RegQueryValueExW(hKey, L"HybridHeuristic", NULL, NULL, (BYTE *)&hybrid, &size) == ERROR_SUCCESS)
		{
			debounce_set_hybrid_heuristic(&g_app.debounce, hybrid != 0);
		}

		for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
		{
			wchar_t valName[64];
			DWORD threshold, enabled, size = sizeof(DWORD);

			// Re-init size for each query
			size = sizeof(DWORD);
			// Load threshold
			StringCchPrintfW(valName, 64, L"Btn%d_Threshold", i);
			if (RegQueryValueExW(hKey, valName, NULL, NULL, (BYTE *)&threshold, &size) == ERROR_SUCCESS)
			{
				debounce_set_threshold(&g_app.debounce, i, (int)threshold, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);
			}

			// Load enabled state
			size = sizeof(DWORD);
			StringCchPrintfW(valName, 64, L"Btn%d_Enabled", i);
			if (RegQueryValueExW(hKey, valName, NULL, NULL, (BYTE *)&enabled, &size) == ERROR_SUCCESS)
			{
				debounce_set_monitored(&g_app.debounce, i, enabled != 0);
			}
		}
		RegCloseKey(hKey);
	}
}

// Set threshold for a specific button
static void SetButtonThreshold(MouseButton button, int threshold_ms)
{
	if (button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	debounce_set_threshold(&g_app.debounce, button, threshold_ms, THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);

	SaveSettings();

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "%S threshold set to %dms",
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
	wc.lpszClassName = L"MouseDebouncerWndClass";

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
		}
		return 0;

	case WM_NOTIFYICON:
		if (LOWORD(lParam) == WM_CONTEXTMENU)
		{
			ShowContextMenu(hWnd, GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
			return 0;
		}
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDM_ABOUT_GITHUB)
		{
			ShellExecute(NULL, L"open", L"https://github.com/matreshka15/MouseFix", NULL, NULL, SW_SHOWNORMAL);
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
		if (LOWORD(wParam) == IDM_TOGGLE_HYBRID)
		{
			ToggleHybridHeuristic();
			return 0;
		}
		if (LOWORD(wParam) == IDM_RESET_STATS)
		{
			debounce_reset_statistics(&g_app.debounce);
#ifndef NDEBUG
			LOG_INFO(&g_app.logger, "Statistics reset");
#endif
			return 0;
		}
		if (LOWORD(wParam) == IDM_PRESET_DEFAULT)
		{
			ApplyPreset(&PRESET_DEFAULT);
			return 0;
		}
		if (LOWORD(wParam) == IDM_PRESET_OFFICE)
		{
			ApplyPreset(&PRESET_OFFICE);
			return 0;
		}
		if (LOWORD(wParam) == IDM_PRESET_STRICT)
		{
			ApplyPreset(&PRESET_STRICT);
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

		// Button threshold settings
		if (LOWORD(wParam) >= IDM_THRESHOLD_BASE && LOWORD(wParam) < IDM_THRESHOLD_BASE + 100)
		{
			// Calculate button_id and threshold_index more precisely
			UINT menu_id = LOWORD(wParam);
			int button_id = (menu_id - IDM_THRESHOLD_BASE) / 100;
			int threshold_index = (menu_id - IDM_THRESHOLD_BASE) % 100;

			const int button_thresholds[] = {40, 50, 60};
			const int wheel_thresholds[] = {20, 30, 35};

			if (button_id >= 0 && button_id < MOUSE_BUTTON_COUNT)
			{
				if (button_id == MOUSE_BUTTON_WHEEL)
				{
					if (threshold_index >= 0 && threshold_index < 3)
					{
						SetButtonThreshold(button_id, wheel_thresholds[threshold_index]);
						return 0;
					}
				}
				else
				{
					if (threshold_index >= 0 && threshold_index < 3)
					{
						SetButtonThreshold(button_id, button_thresholds[threshold_index]);
						return 0;
					}
				}
			}
		}

		// Custom threshold input
		if (LOWORD(wParam) >= IDM_THRESHOLD_CUSTOM && LOWORD(wParam) < IDM_THRESHOLD_CUSTOM + 1000)
		{
			int button_id = (LOWORD(wParam) - IDM_THRESHOLD_CUSTOM) / 100;

			if (button_id >= 0 && button_id < MOUSE_BUTTON_COUNT)
			{
				wchar_t input[INPUT_BOX_INPUT_BUFFER_SIZE] = L"";
				wchar_t prompt[INPUT_BOX_PROMPT_BUFFER_SIZE];
				StringCchPrintf(prompt, INPUT_BOX_PROMPT_BUFFER_SIZE, L"Enter threshold for %S (%d-%dms):", debounce_get_button_name(button_id), THRESHOLD_MIN_VALUE, THRESHOLD_MAX_VALUE);

				if (InputBox(prompt, input, _countof(input)))
				{
					int threshold = _wtoi(input);
					if (threshold >= THRESHOLD_MIN_VALUE && threshold <= THRESHOLD_MAX_VALUE)
					{
						SetButtonThreshold(button_id, threshold);
					}
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
		PostQuitMessage(0);
		return 0;

	default:
		if (uMsg == g_app.tray_icon.taskbar_created_msg)
			tray_icon_restore(&g_app.tray_icon);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Show context menu
static void ShowContextMenu(const HWND hWnd, const int x, const int y)
{
	SetForegroundWindow(hWnd);
	context_menu_show(&g_app.context_menu, hWnd, x, y, &g_app.debounce);
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
	// Limit to one instance
	g_app.mutex = CreateMutex(NULL, TRUE, MUTEX_GUID);
	if (!g_app.mutex)
	{
		ShowErrorMessageBox(L"The mutex could not be created. Error code: %lu", GetLastError());
		return EXIT_FAILURE;
	}

	if (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED)
	{
		ShowErrorMessageBox(L"MouseFix is already running");
		CloseHandle(g_app.mutex);
		return EXIT_SUCCESS;
	}

	g_app.hInstance = hInstance;

	// Process command line arguments
	// TODO: Implement command line parsing

	// Register window class
	if (!RegisterInvisibleClass(hInstance))
	{
		ShowErrorMessageBox(L"The window class could not be registered. Error code: %lu", GetLastError());
		return EXIT_FAILURE;
	}

	// Create window
	if (!CreateWindow(L"MouseDebouncerWndClass", L"MouseFix", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL))
	{
		ShowErrorMessageBox(L"The window could not be created. Error code: %lu", GetLastError());
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
			ShowErrorMessageBox(L"An unexpected error has occurred. Error code: %lu", GetLastError());
			ShutdownApp();
			ReleaseMutex(g_app.mutex);
			CloseHandle(g_app.mutex);
			return GetLastError();
		}
	}

	// Cleanup
	UnregisterClass(L"MouseDebouncerWndClass", hInstance);
	UnregisterClass(INPUT_BOX_CLASS_NAME, hInstance);
	ReleaseMutex(g_app.mutex);
	CloseHandle(g_app.mutex);

	return (int)msg.wParam;
}