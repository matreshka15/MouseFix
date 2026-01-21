#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include "mouse_hook.h"

// Debounce configuration for a single button
typedef struct
{
	bool isMonitored;          // Whether this button is being monitored for debounce
	bool isBlocked;            // Whether the next event should be blocked
	uint32_t blocks;           // Total number of events blocked for this button
	uint64_t previousTime;     // Timestamp of the last processed event
	uint64_t threshold;         // Debounce threshold in platform-specific time units
	uint32_t thresholdMs;      // Debounce threshold in milliseconds
	int32_t wheelDirection;     // Current wheel scroll direction (1=up, -1=down, 0=none)
	uint8_t _padding[28];        // Padding to align structure to 64 bytes for cache efficiency
} __declspec(align(64)) ButtonDebounceData;

// Debounce manager
typedef struct
{
	ButtonDebounceData buttons[MOUSE_BUTTON_COUNT];
	bool use_qpc;
	uint64_t counts_per_second;
} DebounceManager;

// Initialize debounce manager
bool debounce_init(DebounceManager* manager, bool use_qpc);

// Process a mouse event and determine if it should be blocked
bool debounce_process_event(DebounceManager* manager, const MouseEvent* event);

// Set threshold for a specific button
void debounce_set_threshold(DebounceManager* manager, MouseButton button, uint32_t threshold_ms, uint32_t min_ms, uint32_t max_ms);

// Enable/disable monitoring for a specific button
void debounce_set_monitored(DebounceManager* manager, MouseButton button, bool monitored);

// Get total blocked events count
uint32_t debounce_get_total_blocks(const DebounceManager* manager);

// Get blocked events count for a specific button
uint32_t debounce_get_button_blocks(const DebounceManager* manager, MouseButton button);

// Get button name
const char* debounce_get_button_name(MouseButton button);

// Check if any button is being monitored
bool debounce_is_any_monitored(const DebounceManager* manager);

// Reset statistics
void debounce_reset_statistics(DebounceManager* manager);