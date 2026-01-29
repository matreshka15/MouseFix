#pragma once

#include <stdbool.h>
#include <stdint.h>

// Time manager for high-precision timing
typedef struct
{
	bool initialized;
} TimeManager;

// Initialize time manager
bool time_manager_init(TimeManager *manager);

// Get current time
uint64_t time_manager_get_current_time(const TimeManager *manager);

// Convert milliseconds to time units
uint64_t time_manager_ms_to_time(const TimeManager *manager, uint32_t ms);

// Check if QPC is available
bool time_manager_is_qpc_available(void);

// Get time resolution in nanoseconds
double time_manager_get_resolution_ns(const TimeManager *manager);