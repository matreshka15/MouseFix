#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

// Mouse button types
typedef enum
{
	MOUSE_BUTTON_UNKNOWN = -1,
	MOUSE_BUTTON_LEFT,
	MOUSE_BUTTON_RIGHT,
	MOUSE_BUTTON_MIDDLE,
	MOUSE_BUTTON_X1,
	MOUSE_BUTTON_X2,
	MOUSE_BUTTON_WHEEL,
	MOUSE_BUTTON_COUNT
} MouseButton;

// Mouse event structure
typedef struct
{
	MouseButton button;
	uint64_t timestamp;
	bool is_down;
} MouseEvent;

// Mouse hook callback function type
typedef LRESULT(CALLBACK *MouseHookCallback)(const MouseEvent *event, void *user_data);

// Mouse hook manager
typedef struct
{
	HHOOK hook;
	MouseHookCallback callback;
	void *user_data;
	bool installed;
} MouseHookManager;

// Initialize mouse hook manager
bool mouse_hook_init(MouseHookManager *manager, MouseHookCallback callback, void *user_data);

// Install mouse hook
bool mouse_hook_install(MouseHookManager *manager);

// Uninstall mouse hook
void mouse_hook_uninstall(MouseHookManager *manager);

// Get button from WPARAM and MSLLHOOKSTRUCT
MouseButton mouse_hook_get_button(WPARAM wParam, PMSLLHOOKSTRUCT pdata);

// Check if button is pressed
bool mouse_hook_is_button_down(WPARAM wParam);