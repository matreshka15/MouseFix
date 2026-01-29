#include "time_manager.h"
#include <windows.h>

// Initialize time manager
bool time_manager_init(TimeManager *manager)
{
	if (!manager)
		return false;

	manager->initialized = true;
	return true;
}

// Get current time
uint64_t time_manager_get_current_time(const TimeManager *manager)
{
	if (!manager || !manager->initialized)
		return 0;

	return GetTickCount64();
}

// Convert milliseconds to time units
uint64_t time_manager_ms_to_time(const TimeManager *manager, uint32_t ms)
{
	if (!manager || !manager->initialized)
		return 0;

	return (uint64_t)ms;
}

// Check if QPC is available
bool time_manager_is_qpc_available(void)
{
	LARGE_INTEGER freq;
	return QueryPerformanceFrequency(&freq) != 0;
}

// Get time resolution in nanoseconds
double time_manager_get_resolution_ns(const TimeManager *manager)
{
	if (!manager || !manager->initialized)
		return 0.0;

	return 15.6e6; // ~15.6ms for GetTickCount
}