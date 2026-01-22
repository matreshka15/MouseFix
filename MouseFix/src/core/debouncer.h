#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include "mouse_hook.h"

// Debounce configuration for a single button
// Optimized field order for cache efficiency and minimal padding
typedef struct
{
	uint64_t previousTime;	// Timestamp of the last processed event (8 bytes)
	uint64_t threshold;		// Debounce threshold in platform-specific time units (8 bytes)
	uint32_t thresholdMs;	// Debounce threshold in milliseconds (4 bytes)
	uint32_t blocks;		// Total number of events blocked for this button (4 bytes)
	int32_t wheelDirection; // Current wheel scroll direction (1=up, -1=down, 0=none) (4 bytes)
	bool isMonitored;		// Whether this button is being monitored for debounce (1 byte)
	bool isBlocked;			// Whether the next event should be blocked (1 byte)
	uint8_t _padding[34];	// Padding to align structure to 64 bytes for cache efficiency (34 bytes)
} __declspec(align(64)) ButtonDebounceData;

// Debounce manager with cache line alignment for performance
typedef struct
{
	ButtonDebounceData buttons[MOUSE_BUTTON_COUNT]; // Array of button debounce data
	bool use_qpc;									// Whether to use QueryPerformanceCounter (1 byte)
	uint8_t _padding1[7];							// Padding to align counts_per_second (7 bytes)
	uint64_t counts_per_second;						// QPC frequency if use_qpc is true (8 bytes)
	CRITICAL_SECTION cs;							// Critical section for thread safety (40 bytes on x64)
	uint8_t _padding2[8];							// Padding to align to 64 bytes (8 bytes)
} __declspec(align(64)) DebounceManager;

// Initialize debounce manager
bool debounce_init(DebounceManager *manager, bool use_qpc);

// Cleanup debounce manager
void debounce_cleanup(DebounceManager *manager);

// Process a mouse event and determine if it should be blocked
bool debounce_process_event(DebounceManager *manager, const MouseEvent *event);

// Set threshold for a specific button
void debounce_set_threshold(DebounceManager *manager, MouseButton button, uint32_t threshold_ms, uint32_t min_threshold_ms, uint32_t max_threshold_ms);

// Enable/disable monitoring for a specific button
void debounce_set_monitored(DebounceManager *manager, MouseButton button, bool monitored);

// Get total blocked events count
uint32_t debounce_get_total_blocks(DebounceManager *manager);

// Get blocked events count for a specific button
uint32_t debounce_get_button_blocks(DebounceManager *manager, MouseButton button);

// Get button name
const char *debounce_get_button_name(MouseButton button);

// Check if any button is being monitored
bool debounce_is_any_monitored(DebounceManager *manager);

// Reset statistics
void debounce_reset_statistics(DebounceManager *manager);