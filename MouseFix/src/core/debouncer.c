#include "debouncer.h"
#include "time_manager.h"
#include <string.h>

// Initialize debounce manager
bool debounce_init(DebounceManager* manager, bool use_qpc)
{
	if (!manager)
		return false;

	memset(manager, 0, sizeof(DebounceManager));
	manager->use_qpc = use_qpc;

	if (use_qpc)
	{
		if (!QueryPerformanceFrequency((LARGE_INTEGER*)&manager->counts_per_second))
		{
			manager->use_qpc = false;
		}
	}

	return true;
}

// Process a mouse event and determine if it should be blocked
bool debounce_process_event(DebounceManager* manager, const MouseEvent* event)
{
	// Early return for invalid inputs
	if (!manager || !event)
		return false;

	ButtonDebounceData* data = &manager->buttons[event->button];

	// Early return if button is not monitored (most common case)
	if (!data->isMonitored)
		return false;

	// Handle wheel events specially
	if (event->button == MOUSE_BUTTON_WHEEL)
	{
		// For wheel events, we encode the scroll direction in the timestamp field:
		// - Upper 32 bits: Time of the event (GetTickCount)
		// - Lower 32 bits: Wheel delta (positive for scroll up, negative for scroll down)
		// This allows us to pass both direction and time through the same parameter
		int32_t wheel_direction = (int32_t)(event->timestamp & 0xFFFFFFFF);
		int32_t direction_sign = (wheel_direction > 0) ? 1 : (wheel_direction < 0) ? -1 : 0;

		if (direction_sign == 0)
			return false;  // Ignore zero delta

		// Check if direction changed within the debounce threshold
		// If the user tries to scroll in the opposite direction too quickly, block it
		if (data->wheelDirection != 0 && data->wheelDirection != direction_sign)
		{
			uint64_t elapsed_time = (uint64_t)(event->timestamp >> 32) - data->previousTime;
			if (elapsed_time <= data->threshold)
			{
				// Block this reverse scroll
				data->blocks++;
				return true;
			}
		}

		// Update direction and timestamp for next comparison
		data->wheelDirection = direction_sign;
		data->previousTime = (uint64_t)(event->timestamp >> 32);
		return false;
	}

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
				return true;
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
			return true;
		}
		// Update timestamp for next comparison
		data->previousTime = event->timestamp;
	}

	return false;
}

// Set threshold for a specific button
void debounce_set_threshold(DebounceManager* manager, MouseButton button, uint32_t threshold_ms, uint32_t min_ms, uint32_t max_ms)
{
	if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	if (threshold_ms < min_ms || threshold_ms > max_ms)
		return;

	manager->buttons[button].thresholdMs = threshold_ms;

	if (manager->use_qpc)
	{
		manager->buttons[button].threshold = (uint64_t)threshold_ms * manager->counts_per_second / 1000;
	}
	else
	{
		manager->buttons[button].threshold = threshold_ms;
	}
}

// Enable/disable monitoring for a specific button
void debounce_set_monitored(DebounceManager* manager, MouseButton button, bool monitored)
{
	if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)
		return;

	manager->buttons[button].isMonitored = monitored;
}

// Get total blocked events count
uint32_t debounce_get_total_blocks(const DebounceManager* manager)
{
	if (!manager)
		return 0;

	uint32_t total = 0;
	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		total += manager->buttons[i].blocks;
	}
	return total;
}

// Get blocked events count for a specific button
uint32_t debounce_get_button_blocks(const DebounceManager* manager, MouseButton button)
{
	if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)
		return 0;

	return manager->buttons[button].blocks;
}

// Get button name
const char* debounce_get_button_name(MouseButton button)
{
	switch (button)
	{
		case MOUSE_BUTTON_LEFT: return "Left";
		case MOUSE_BUTTON_RIGHT: return "Right";
		case MOUSE_BUTTON_MIDDLE: return "Middle";
		case MOUSE_BUTTON_X1: return "4th";
		case MOUSE_BUTTON_X2: return "5th";
		case MOUSE_BUTTON_WHEEL: return "Wheel";
		default: return "Unknown";
	}
}

// Check if any button is being monitored
bool debounce_is_any_monitored(const DebounceManager* manager)
{
	if (!manager)
		return false;

	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		if (manager->buttons[i].isMonitored)
			return true;
	}

	return false;
}

// Reset statistics
void debounce_reset_statistics(DebounceManager* manager)
{
	if (!manager)
		return;

	for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
	{
		manager->buttons[i].blocks = 0;
		manager->buttons[i].isBlocked = false;
		manager->buttons[i].wheelDirection = 0;
	}
}