#pragma once

#include <stdbool.h>
#include <stdint.h>

// Time manager for high-precision timing
typedef struct
{
	bool initialized;
	bool qpc_available;
	int64_t qpc_frequency;
} TimeManager;

// Initialize time manager
bool time_manager_init(TimeManager *manager);

// Get current time
uint64_t time_manager_get_current_time(const TimeManager *manager);