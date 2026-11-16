#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include "mouse_hook.h"

/* Default thresholds in ms, ordered like MouseButton (Left/Right/Middle/X1/X2/Wheel).
   This is the single source of truth for the fresh-install defaults: debounce_init
   applies them and DEBOUNCE_PRESETS[DEBOUNCE_PRESET_DEFAULT] expands the same
   initializer. */
#define DEBOUNCE_DEFAULT_THRESHOLDS {50, 50, 50, 50, 50, 30}

/* Threshold sets offered by the tray menu. The default expands the same
   initializer as DEBOUNCE_DEFAULT_THRESHOLDS, so the fresh-install values
   exist in exactly one place. */
typedef struct
{
    uint32_t left;
    uint32_t right;
    uint32_t middle;
    uint32_t x1;
    uint32_t x2;
    uint32_t wheel;
} DebouncePreset;

typedef enum
{
    DEBOUNCE_PRESET_DEFAULT = 0,
    DEBOUNCE_PRESET_OFFICE,
    DEBOUNCE_PRESET_STRICT,
    DEBOUNCE_PRESET_COUNT
} DebouncePresetId;

extern const DebouncePreset DEBOUNCE_PRESETS[DEBOUNCE_PRESET_COUNT];

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

/* Cap on pending injected events. Only a drag release plus a replayed press
   can be in flight at once; the rest of the room is slack. */
#define DEBOUNCE_INJECT_QUEUE_CAP 16

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

    /* Injected input is sent from this thread, never from the hook callback:
       SendInput on the hook thread deadlocks the whole input queue. */
    CRITICAL_SECTION inject_cs;
    HANDLE inject_event;
    HANDLE inject_thread;
    volatile LONG inject_stop;
    int inject_count;
    INPUT inject_queue[DEBOUNCE_INJECT_QUEUE_CAP];

    /* Set once the matching CRITICAL_SECTION is initialized, so cleanup is
       safe to call after a partial init failure. */
    bool cs_ready;
    bool inject_cs_ready;
} __declspec(align(64)) DebounceManager;

bool debounce_init(DebounceManager *manager);
void debounce_cleanup(DebounceManager *manager);
bool debounce_process_event(DebounceManager *manager, const MouseEvent *event);
void debounce_set_threshold(DebounceManager *manager, MouseButton button, uint32_t threshold_ms, uint32_t min_threshold_ms, uint32_t max_threshold_ms);
void debounce_apply_preset(DebounceManager *manager, const DebouncePreset *preset);
bool debounce_matches_preset(const DebounceManager *manager, const DebouncePreset *preset);
void debounce_set_monitored(DebounceManager *manager, MouseButton button, bool monitored);
uint32_t debounce_get_total_blocks(DebounceManager *manager);
uint32_t debounce_get_button_blocks(DebounceManager *manager, MouseButton button);
const char *debounce_get_button_name(MouseButton button);
bool debounce_is_any_monitored(DebounceManager *manager);
void debounce_reset_statistics(DebounceManager *manager);
void debounce_set_hybrid_heuristic(DebounceManager *manager, bool use_hybrid);
void debounce_check_deferred_releases(DebounceManager *manager);
uint64_t debounce_get_timestamp(DebounceManager *manager);
