#include "time_manager.h"
#include <windows.h>

/* QPC ticks to microseconds without overflowing on a long uptime. At 10 MHz,
   ticks * 1e6 overflows around day 10; splitting off the seconds keeps it safe. */
static uint64_t qpc_to_us(uint64_t ticks, uint64_t frequency)
{
	if (frequency == 0)
		return 0;
	return (ticks / frequency) * 1000000ULL + (ticks % frequency) * 1000000ULL / frequency;
}

// Initialize time manager
bool time_manager_init(TimeManager *manager)
{
	if (!manager)
		return false;

	LARGE_INTEGER freq;
	manager->qpc_available = QueryPerformanceFrequency(&freq) != 0;
	manager->qpc_frequency = manager->qpc_available ? freq.QuadPart : 0;
	manager->initialized = true;
	return true;
}

// Get current time in microseconds
uint64_t time_manager_get_current_time(const TimeManager *manager)
{
	if (!manager || !manager->initialized)
		return 0;

	if (manager->qpc_available)
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return qpc_to_us((uint64_t)now.QuadPart, (uint64_t)manager->qpc_frequency);
	}

	return GetTickCount64() * 1000;
}