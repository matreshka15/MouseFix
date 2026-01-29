#include "debouncer.h"
#include "time_manager.h"
#include <string.h>

// Initialize debounce manager with specified time source
// Parameters:
//   manager - Pointer to DebounceManager structure to initialize
//   use_qpc - If true, use QueryPerformanceCounter for high-precision timing
// Returns:
//   true on success, false on failure
bool debounce_init(DebounceManager *manager, bool use_qpc)
{
	if (!manager)
		return false;

	memset(manager, 0, sizeof(DebounceManager));
	manager->use_qpc = use_qpc;
	manager->use_hybrid_heuristic = true; // Default to enabled

	if (use_qpc)
	{
		if (!QueryPerformanceFrequency((LARGE_INTEGER *)&manager->counts_per_second))
		{
			manager->use_qpc = false;
		}
	}

	// Initialize critical section for thread safety
	InitializeCriticalSection(&manager->cs);

	return true;
}

// Cleanup and release debounce manager resources
// Parameters:
//   manager - Pointer to DebounceManager structure to cleanup
void debounce_cleanup(DebounceManager *manager)
{
	if (!manager)
		return;

	DeleteCriticalSection(&manager->cs);
}

// Process a mouse event and determine if it should be blocked
// Parameters:
//   manager - Pointer to DebounceManager structure
//   event - Pointer to MouseEvent structure containing event data
// Returns:
//   true if event should be blocked, false if it should pass through
bool debounce_process_event(DebounceManager *manager, const MouseEvent *event)
{
	// Early return for invalid inputs
	if (!manager || !event)
		return false;

	EnterCriticalSection(&manager->cs);

	ButtonDebounceData *data = &manager->buttons[event->button];

	if (event->is_injected)
	{
		LeaveCriticalSection(&manager->cs);
		return false;
	}

	// Early return if button is not monitored (most common case)
	if (!data->isMonitored)
	{
		LeaveCriticalSection(&manager->cs);
		return false;
	}

	bool should_block = false;

	// Handle wheel events specially
	if (event->button == MOUSE_BUTTON_WHEEL)
	{
		// For wheel events, we encode the scroll direction in the timestamp field:
		// - Upper 32 bits: Time of the event (GetTickCount64)
		// - Lower 32 bits: Wheel delta (positive for scroll up, negative for scroll down)
		// This allows us to pass both direction and time through the same parameter
		int32_t wheel_direction = (int32_t)(event->timestamp & 0xFFFFFFFF);
		int32_t direction_sign = (wheel_direction > 0) ? 1 : (wheel_direction < 0) ? -1
																				   : 0;

		if (direction_sign == 0)
		{
			LeaveCriticalSection(&manager->cs);
			return false; // Ignore zero delta
		}

		// Check if direction changed within the debounce threshold
		// If the user tries to scroll in the opposite direction too quickly, block it
		if (data->wheelDirection != 0 && data->wheelDirection != direction_sign)
		{
			uint64_t elapsed_time = (uint64_t)(event->timestamp >> 32) - data->previousTime;
			if (elapsed_time <= data->threshold)
			{
				// Block this reverse scroll
				data->blocks++;
				should_block = true;
			}
		}

		// Update direction and timestamp for next comparison
		data->wheelDirection = direction_sign;
		data->previousTime = (uint64_t)(event->timestamp >> 32);
	}
	else
	{
		// Optimize branch prediction by checking is_down first
		if (event->is_down)
		{
			// Check if we are deferring a release (Hybrid Heuristic)
			if (manager->use_hybrid_heuristic && data->isDeferringRelease)
			{
				data->isDeferringRelease = false;
				data->blocks++;
				should_block = true;
				data->previousTime = event->timestamp;
			}
			else
			{
				// Check if event should be blocked
				if (!data->isBlocked)
				{
					uint64_t elapsed_time = event->timestamp - data->previousTime;
					if (elapsed_time <= data->threshold)
					{
						// Block this event
						data->isBlocked = true;
						data->blocks++;
						should_block = true;
					}
				}
				data->previousTime = event->timestamp;
				data->downTime = event->timestamp;
				data->downPoint.x = event->x;
				data->downPoint.y = event->y;
			}
		}
		else
		{
			// Button up event
			if (data->isBlocked)
			{
				// Unblock and block this event
				data->isBlocked = false;
				should_block = true;
			}

			data->previousTime = event->timestamp;

			// Hybrid Heuristic
			if (!should_block && manager->use_hybrid_heuristic)
			{
				uint64_t holdTime = event->timestamp - data->downTime;
				long dx = event->x - data->downPoint.x;
				long dy = event->y - data->downPoint.y;
				long distSq = dx * dx + dy * dy;

				if (holdTime > 200 || distSq > 25)
				{
					data->isDeferringRelease = true;
					data->deferStartTime = event->timestamp;
					data->deferUpPoint.x = event->x;
					data->deferUpPoint.y = event->y;
					should_block = true;
				}
			}
		}
	}

	LeaveCriticalSection(&manager->cs);
	return should_block;
}

// Set debounce threshold for a specific button
// Parameters:
//   manager - Pointer to DebounceManager structure
//   button - Mouse button to set threshold for
//   threshold_ms - Threshold value in milliseconds
//   min_threshold_ms - Minimum allowed threshold value
//   max_threshold_ms - Maximum allowed threshold value
void debounce_set_threshold(DebounceManager *manager, MouseButton button, uint32_t threshold_ms, uint32_t min_threshold_ms, uint32_t max_threshold_ms)
{
	if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	if (threshold_ms < min_threshold_ms || threshold_ms > max_threshold_ms)
		return;

	EnterCriticalSection(&manager->cs);

	manager->buttons[button].thresholdMs = threshold_ms;

	if (manager->use_qpc)
	{
		manager->buttons[button].threshold = (uint64_t)threshold_ms * manager->counts_per_second / 1000;
	}
	else
	{
		manager->buttons[button].threshold = threshold_ms;
	}

	LeaveCriticalSection(&manager->cs);
}

// Set whether to use Hybrid Heuristic Scheme
// Parameters:
//   manager - Pointer to DebounceManager structure
//   use_hybrid - true to enable, false to disable
void debounce_set_hybrid_heuristic(DebounceManager *manager, bool use_hybrid)
{
	if (!manager)
		return;

	EnterCriticalSection(&manager->cs);
	manager->use_hybrid_heuristic = use_hybrid;

	// If disabling, clear any pending deferred releases
	if (!use_hybrid)
	{
		for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
		{
			if (manager->buttons[i].isDeferringRelease)
			{
				manager->buttons[i].isDeferringRelease = false;
				// Since we are disabling, we can't reliably inject now,
				// but since the original event was swallowed, the user might be stuck in "drag".
				// However, if we do nothing, the next click will clear it or the user clicks again.
				// A cleaner way is to let the user click again.
			}
		}
	}
	LeaveCriticalSection(&manager->cs);
}

// Enable or disable monitoring for a specific button
// Parameters:
//   manager - Pointer to DebounceManager structure
//   button - Mouse button to enable/disable monitoring for
//   monitored - true to enable monitoring, false to disable
void debounce_set_monitored(DebounceManager *manager, MouseButton button, bool monitored)
{
	if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	EnterCriticalSection(&manager->cs);
	manager->buttons[button].isMonitored = monitored;
	LeaveCriticalSection(&manager->cs);
}

// Get total number of blocked events across all buttons
// Parameters:
//   manager - Pointer to DebounceManager structure
// Returns:
//   Total number of blocked events
uint32_t debounce_get_total_blocks(DebounceManager *manager)

{

	if (!manager)

		return 0;

	EnterCriticalSection(&manager->cs);

	uint32_t total = 0;

	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)

	{

		total += manager->buttons[i].blocks;
	}

	LeaveCriticalSection(&manager->cs);

	return total;
}

// Get number of blocked events for a specific button

// Parameters:

//   manager - Pointer to DebounceManager structure

//   button - Mouse button to get blocked count for

// Returns:

//   Number of blocked events for the specified button

uint32_t debounce_get_button_blocks(DebounceManager *manager, MouseButton button)

{

	if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)

		return 0;

	EnterCriticalSection(&manager->cs);

	uint32_t blocks = manager->buttons[button].blocks;

	LeaveCriticalSection(&manager->cs);

	return blocks;
}

// Get human-readable name for a mouse button

// Parameters:

//   button - Mouse button to get name for

// Returns:

//   String representation of button name

const char *debounce_get_button_name(MouseButton button)
{
	switch (button)
	{
	case MOUSE_BUTTON_LEFT:
		return "Left";
	case MOUSE_BUTTON_RIGHT:
		return "Right";
	case MOUSE_BUTTON_MIDDLE:
		return "Middle";
	case MOUSE_BUTTON_X1:
		return "4th";
	case MOUSE_BUTTON_X2:
		return "5th";
	case MOUSE_BUTTON_WHEEL:
		return "Wheel";
	default:
		return "Unknown";
	}
}

// Check if any button is currently being monitored
// Parameters:
//   manager - Pointer to DebounceManager structure
// Returns:
//   true if at least one button is monitored, false otherwise
bool debounce_is_any_monitored(DebounceManager *manager)
{
	if (!manager)
		return false;

	EnterCriticalSection(&manager->cs);

	bool any_monitored = false;
	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		if (manager->buttons[i].isMonitored)
		{
			any_monitored = true;
			break;
		}
	}

	LeaveCriticalSection(&manager->cs);
	return any_monitored;
}

// Reset all statistics (blocked counts, blocked states, wheel directions)
// Parameters:
//   manager - Pointer to DebounceManager structure
void debounce_reset_statistics(DebounceManager *manager)
{
	if (!manager)
		return;

	EnterCriticalSection(&manager->cs);

	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		manager->buttons[i].blocks = 0;
		manager->buttons[i].isBlocked = false;
		manager->buttons[i].isDeferringRelease = false;
		manager->buttons[i].wheelDirection = 0;
	}

	LeaveCriticalSection(&manager->cs);
}

// Check for deferred releases and inject events if necessary
void debounce_check_deferred_releases(DebounceManager *manager)
{
	if (!manager)
		return;

	// Use a temporary list to store actions to perform outside the critical section
	struct
	{
		bool inject;
		DWORD flags;
		DWORD mouseData;
	} actions[MOUSE_BUTTON_COUNT] = {0};

	EnterCriticalSection(&manager->cs);
	// Use GetTickCount64 to be consistent with event timestamps (mostly)
	uint64_t now = GetTickCount64();

	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		ButtonDebounceData *data = &manager->buttons[i];
		if (data->isDeferringRelease)
		{
			// 50ms delay
			if (now - data->deferStartTime > 50)
			{
				data->isDeferringRelease = false;
				actions[i].inject = true;

				switch (i)
				{
				case MOUSE_BUTTON_LEFT:
					actions[i].flags = MOUSEEVENTF_LEFTUP;
					break;
				case MOUSE_BUTTON_RIGHT:
					actions[i].flags = MOUSEEVENTF_RIGHTUP;
					break;
				case MOUSE_BUTTON_MIDDLE:
					actions[i].flags = MOUSEEVENTF_MIDDLEUP;
					break;
				case MOUSE_BUTTON_X1:
					actions[i].flags = MOUSEEVENTF_XUP;
					actions[i].mouseData = XBUTTON1;
					break;
				case MOUSE_BUTTON_X2:
					actions[i].flags = MOUSEEVENTF_XUP;
					actions[i].mouseData = XBUTTON2;
					break;
				}
			}
		}
	}
	LeaveCriticalSection(&manager->cs);

	// Perform injections
	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		if (actions[i].inject && actions[i].flags)
		{
			INPUT input = {0};
			input.type = INPUT_MOUSE;
			input.mi.dwFlags = actions[i].flags;
			input.mi.mouseData = actions[i].mouseData;
			SendInput(1, &input, sizeof(INPUT));
		}
	}
}