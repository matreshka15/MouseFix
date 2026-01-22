#include "mouse_hook.h"
#include <stdint.h>
#include <windows.h>

static MouseHookManager *g_manager = NULL;

// Low-level mouse hook callback
static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	// Read g_manager atomically to avoid race condition
	MouseHookManager *manager = (MouseHookManager *)InterlockedExchangePointer((PVOID *)&g_manager, g_manager);

	if (nCode == HC_ACTION && manager && manager->callback)
	{
		if (wParam != WM_MOUSEMOVE)
		{
			PMSLLHOOKSTRUCT pdata = (PMSLLHOOKSTRUCT)lParam;
			MouseButton button = mouse_hook_get_button(wParam, pdata);

			if (button >= 0)
			{
				MouseEvent event;
				event.button = button;

				// For wheel events, encode scroll direction in upper 32 bits of timestamp
				if (wParam == WM_MOUSEWHEEL)
				{
					int32_t delta = GET_WHEEL_DELTA_WPARAM(pdata->mouseData);
					event.timestamp = ((uint64_t)GetTickCount64() << 32) | (uint32_t)(delta & 0xFFFFFFFF);
				}
				else
				{
					event.timestamp = (uint64_t)pdata->time;
				}

				event.is_down = mouse_hook_is_button_down(wParam);

				LRESULT result = g_manager->callback(&event, g_manager->user_data);
				if (result != 0)
					return result;
			}
		}
	}
	return CallNextHookEx(g_manager ? g_manager->hook : NULL, nCode, wParam, lParam);
}

// Initialize mouse hook manager
bool mouse_hook_init(MouseHookManager *manager, MouseHookCallback callback, void *user_data)
{
	if (!manager || !callback)
		return false;

	manager->hook = NULL;
	manager->callback = callback;
	manager->user_data = user_data;
	manager->installed = false;
	g_manager = manager;

	return true;
}

// Install mouse hook
bool mouse_hook_install(MouseHookManager *manager)
{
	if (!manager)
		return false;

	manager->hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, NULL, 0);
	if (!manager->hook)
		return false;

	manager->installed = true;
	return true;
}

// Uninstall mouse hook
void mouse_hook_uninstall(MouseHookManager *manager)
{
	if (!manager)
		return;

	if (manager->hook)
	{
		UnhookWindowsHookEx(manager->hook);
		manager->hook = NULL;
	}

	manager->installed = false;
	g_manager = NULL;
}

// Get button from WPARAM and MSLLHOOKSTRUCT
MouseButton mouse_hook_get_button(WPARAM wParam, PMSLLHOOKSTRUCT pdata)
{
	if (wParam == WM_LBUTTONDOWN || wParam == WM_LBUTTONUP)
		return MOUSE_BUTTON_LEFT;
	if (wParam == WM_RBUTTONDOWN || wParam == WM_RBUTTONUP)
		return MOUSE_BUTTON_RIGHT;
	if (wParam == WM_MBUTTONDOWN || wParam == WM_MBUTTONUP)
		return MOUSE_BUTTON_MIDDLE;
	if (wParam == WM_XBUTTONDOWN || wParam == WM_XBUTTONUP)
		return HIWORD(pdata->mouseData) == XBUTTON1 ? MOUSE_BUTTON_X1 : MOUSE_BUTTON_X2;
	if (wParam == WM_MOUSEWHEEL)
		return MOUSE_BUTTON_WHEEL;

	return MOUSE_BUTTON_UNKNOWN;
}

// Check if button is pressed
bool mouse_hook_is_button_down(WPARAM wParam)
{
	return (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
			wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN);
}