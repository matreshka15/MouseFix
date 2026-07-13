#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include "mouse_hook.h"

/* Button state for Smart Drag state machine */
typedef enum
{
    BTN_STATE_IDLE = 0,
    BTN_STATE_PRESSED,
    BTN_STATE_DRAGGING,
    BTN_STATE_CONFIRMING,
    BTN_STATE_BLOCKED
} ButtonState;

/* Per-button debounce data, aligned to cache line */
typedef struct
{
    uint64_t previousTime;
    uint64_t downTime;
    uint64_t confirmStartTime;
    POINT downPoint;
    uint32_t thresholdMs;
    uint32_t blocks;
    int32_t wheelDirection;
    ButtonState state;
    bool isMonitored;
    uint8_t _padding[3];
} __declspec(align(64)) ButtonDebounceData;

/* Debounce manager */
typedef struct
{
    ButtonDebounceData buttons[MOUSE_BUTTON_COUNT];
    bool use_hybrid_heuristic;
    uint8_t _padding1[7];
    CRITICAL_SECTION cs;
    uint8_t _padding2[16];
    int64_t qpc_frequency;
    bool qpc_available;
} __declspec(align(64)) DebounceManager;

bool debounce_init(DebounceManager *manager);
void debounce_cleanup(DebounceManager *manager);
bool debounce_process_event(DebounceManager *manager, const MouseEvent *event);
void debounce_set_threshold(DebounceManager *manager, MouseButton button, uint32_t threshold_ms, uint32_t min_threshold_ms, uint32_t max_threshold_ms);
void debounce_set_monitored(DebounceManager *manager, MouseButton button, bool monitored);
uint32_t debounce_get_total_blocks(DebounceManager *manager);
uint32_t debounce_get_button_blocks(DebounceManager *manager, MouseButton button);
const char *debounce_get_button_name(MouseButton button);
bool debounce_is_any_monitored(DebounceManager *manager);
void debounce_reset_statistics(DebounceManager *manager);
void debounce_set_hybrid_heuristic(DebounceManager *manager, bool use_hybrid);
void debounce_check_deferred_releases(DebounceManager *manager);
uint64_t debounce_get_timestamp(DebounceManager *manager);
