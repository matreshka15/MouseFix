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
			// Update timestamp for next comparison
			data->previousTime = event->timestamp;
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
		manager->buttons[i].wheelDirection = 0;
	}

	LeaveCriticalSection(&manager->cs);
}