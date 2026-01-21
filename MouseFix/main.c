#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define STRICT
#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API static
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
#include "optparse.h"
#include "resource.h"
#include "menu_ids.h"

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

// Function declarations
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK OnMouseHookCallback(const MouseEvent* event, void* user_data);
static bool RegisterInvisibleClass(const HINSTANCE hInstance);
static bool InitializeApp(void);
static void ShutdownApp(void);
static void ShowContextMenu(const HWND hWnd, const int x, const int y);
static void CDECL ShowErrorMessageBox(LPCWSTR message, ...);
static void ToggleEnableAll(void);
static void ToggleButton(MouseButton button);
static void ToggleWheel(void);
static void ApplyPreset(int threshold);
static void SetButtonThreshold(MouseButton button, int threshold_ms);
static bool InputBox(LPCWSTR prompt, LPWSTR buffer, int buffer_size);

// Mouse hook callback
static LRESULT CALLBACK OnMouseHookCallback(const MouseEvent* event, void* user_data)
{
	AppState* app = (AppState*)user_data;

	// Get current time - optimized path
	uint64_t current_time;
	if (app->time_manager.use_qpc)
	{
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		current_time = li.QuadPart;
	}
	else
	{
		current_time = event->timestamp;
	}

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

	// Initialize time manager with default settings (QPC disabled)
	if (!time_manager_init(&g_app.time_manager, false))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize time manager");
#endif
		return false;
	}

	// Initialize debounce manager
	if (!debounce_init(&g_app.debounce, false))
	{
#ifndef NDEBUG
		LOG_ERROR(&g_app.logger, "Failed to initialize debounce manager");
#endif
		return false;
	}

	// Set default threshold (60ms) and enable left button by default
	debounce_set_threshold(&g_app.debounce, MOUSE_BUTTON_LEFT, 60, 1, 500);
	debounce_set_monitored(&g_app.debounce, MOUSE_BUTTON_LEFT, true);

	// Set wheel scroll threshold (30ms, shorter than button threshold)
	debounce_set_threshold(&g_app.debounce, MOUSE_BUTTON_WHEEL, 30, 1, 500);

	// Apply Default Preset (60ms for buttons, 30ms for wheel)
	ApplyPreset(60);

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

	return true;
}

// Shutdown application
static void ShutdownApp(void)
{
#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Shutting down application...");
#endif

	// Remove tray icon
	tray_icon_remove(&g_app.tray_icon);

	// Uninstall mouse hook
	mouse_hook_uninstall(&g_app.mouse_hook);

	// Cleanup modules
#ifndef NDEBUG
	logger_cleanup(&g_app.logger);
#endif
	error_handler_cleanup(&g_app.error_handler);

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Application shutdown complete");
#endif
}

// Toggle enable/disable all buttons
static void ToggleEnableAll(void)
{
	bool is_enabled = debounce_is_any_monitored(&g_app.debounce);

	// Toggle all buttons
	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		debounce_set_monitored(&g_app.debounce, i, !is_enabled);
	}

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

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Wheel scrolling %s", !current_state ? "enabled" : "disabled");
#endif
}

// Apply preset threshold
static void ApplyPreset(int threshold)
{
	// Apply threshold to all buttons
	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		// Wheel scroll uses a shorter threshold (50% of button threshold, minimum 20ms)
		int wheel_threshold = (threshold / 2);
		if (wheel_threshold < 20)
			wheel_threshold = 20;

		if (i == MOUSE_BUTTON_WHEEL)
		{
			debounce_set_threshold(&g_app.debounce, i, wheel_threshold, 1, 500);
		}
		else
		{
			debounce_set_threshold(&g_app.debounce, i, threshold, 1, 500);
		}
	}

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "Applied preset: %dms", threshold);
#endif
}

// Simple input box dialog
static bool InputBox(LPCWSTR prompt, LPWSTR buffer, int buffer_size)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);

	// Register a simple window class
	WNDCLASSW wc = {0};
	wc.lpfnWndProc = DefWindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"InputBoxClass";
	if (!RegisterClassW(&wc))
	{
		return false;
	}

	// Create dialog window
	HWND hWnd = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
		L"InputBoxClass", L"Set Threshold",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 320, 160,
		NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		UnregisterClassW(L"InputBoxClass", hInstance);
		return false;
	}

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
		20, 50, 280, 25, hWnd, NULL, hInstance, NULL);

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

	// Message loop
	MSG msg;
	BOOL result = FALSE;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == WM_COMMAND)
		{
			if (LOWORD(msg.wParam) == IDOK)
			{
				GetWindowTextW(hEdit, buffer, buffer_size);
				result = TRUE;
				break;
			}
			if (LOWORD(msg.wParam) == IDCANCEL)
			{
				result = FALSE;
				break;
			}
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DestroyWindow(hWnd);
	UnregisterClassW(L"InputBoxClass", hInstance);
	return result;
}

// Set threshold for a specific button
static void SetButtonThreshold(MouseButton button, int threshold_ms)
{
	if (button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	debounce_set_threshold(&g_app.debounce, button, threshold_ms, 1, 500);

#ifndef NDEBUG
	LOG_INFO(&g_app.logger, "%S threshold set to %dms",
	         debounce_get_button_name(button),
	         threshold_ms);
#endif
}

// Register invisible window class
static bool RegisterInvisibleClass(const HINSTANCE hInstance)
{
	WNDCLASSEX wc = { 0 };
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

		case WM_NOTIFYICON:
			if (LOWORD(lParam) == WM_CONTEXTMENU)
			{
				ShowContextMenu(hWnd, GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
				return 0;
			}
			break;

		case WM_COMMAND:
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
				ApplyPreset(60);
				return 0;
			}
			if (LOWORD(wParam) == IDM_PRESET_OFFICE)
			{
				ApplyPreset(80);
				return 0;
			}
			if (LOWORD(wParam) == IDM_PRESET_STRICT)
			{
				ApplyPreset(40);
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
				int threshold_index = LOWORD(wParam) % 10;
				int button_id = (LOWORD(wParam) - IDM_THRESHOLD_BASE) / 10;

				const int button_thresholds[] = {40, 60, 80};
				const int wheel_thresholds[] = {20, 30, 40};

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
			if (LOWORD(wParam) >= IDM_THRESHOLD_CUSTOM && LOWORD(wParam) < IDM_THRESHOLD_CUSTOM + 100)
			{
				int button_id = (LOWORD(wParam) - IDM_THRESHOLD_CUSTOM) / 10;

				if (button_id >= 0 && button_id < MOUSE_BUTTON_COUNT)
				{
					wchar_t input[32] = L"";
					wchar_t prompt[128];
					StringCchPrintf(prompt, 128, L"Enter threshold for %s (1-500ms):", debounce_get_button_name(button_id));

					if (InputBox(prompt, input, sizeof(input) / sizeof(wchar_t)))
					{
						int threshold = _wtoi(input);
						if (threshold >= 1 && threshold <= 500)
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
	WCHAR buffer[1024];
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
	g_app.mutex = CreateMutex(NULL, TRUE, L"{05B95384-625D-491A-A326-94758957C021}");
	if (!g_app.mutex)
	{
		ShowErrorMessageBox(L"The mutex could not be created!\nError code: %lu", GetLastError());
		return EXIT_FAILURE;
	}

	if (GetLastError() == ERROR_ALREADY_EXISTS || GetLastError() == ERROR_ACCESS_DENIED)
	{
		ShowErrorMessageBox(L"MouseFix is already running!");
		return EXIT_SUCCESS;
	}

	g_app.hInstance = hInstance;

	// Process command line arguments
	// TODO: Implement command line parsing

	// Register window class
	if (!RegisterInvisibleClass(hInstance))
	{
		ShowErrorMessageBox(L"The window class couldn't be registered.\nError code: %lu", GetLastError());
		return EXIT_FAILURE;
	}

	// Create window
	if (!CreateWindow(L"MouseDebouncerWndClass", L"MouseFix", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL))
	{
		ShowErrorMessageBox(L"The window couldn't be created. Error code: %lu", GetLastError());
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
			ShowErrorMessageBox(L"An unexpected error has occurred.\nError code: %lu", GetLastError());
			ShutdownApp();
			ReleaseMutex(g_app.mutex);
			CloseHandle(g_app.mutex);
			return GetLastError();
		}
	}

	// Cleanup
	UnregisterClass(L"MouseDebouncerWndClass", hInstance);
	ReleaseMutex(g_app.mutex);
	CloseHandle(g_app.mutex);

	return (int)msg.wParam;
}