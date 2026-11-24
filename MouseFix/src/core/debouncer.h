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
    bool smartDragEnabled;
    bool releaseWasDrag;
    uint8_t _padding[1];
} __declspec(align(64)) ButtonDebounceData;

/* Cap on pending injected events. Only a drag release plus a replayed press
   can be in flight at once; the rest of the room is slack. */
#define DEBOUNCE_INJECT_QUEUE_CAP 16

/* One queued injected event plus the button it belongs to, so a failed
   injection can reset exactly that button's state. */
typedef struct
{
    INPUT input;
    MouseButton button;
} DebounceInjectEntry;

/* One button's settings, read under the manager lock. The UI and the
   persistence layer use this instead of touching ButtonDebounceData directly. */
typedef struct
{
    uint32_t threshold_ms;
    bool monitored;
    bool smart_drag_enabled;
} DebounceButtonConfig;

/* Debounce manager */
typedef struct
{
    ButtonDebounceData buttons[MOUSE_BUTTON_COUNT];
    CRITICAL_SECTION cs;
    uint8_t _padding2[16];
    int64_t qpc_frequency;
    bool qpc_available;

    /* Injected input is sent from this thread, never from the hook callback:
       SendInput on the hook thread deadlocks the whole input queue. The
       function pointer is swappable so the unit test can stub SendInput. */
    CRITICAL_SECTION inject_cs;
    HANDLE inject_event;
    HANDLE inject_thread;
    volatile LONG inject_stop;
    int inject_count;
    DebounceInjectEntry inject_queue[DEBOUNCE_INJECT_QUEUE_CAP];
    UINT (WINAPI *send_input)(UINT, LPINPUT, int);

    /* Set once the matching CRITICAL_SECTION is initialized, so cleanup is
       safe to call after a partial init failure. */
    bool cs_ready;
    bool inject_cs_ready;
} __declspec(align(64)) DebounceManager;

bool debounce_init(DebounceManager *manager);
void debounce_cleanup(DebounceManager *manager);
bool debounce_process_event(DebounceManager *manager, const MouseEvent *event);
void debounce_set_smart_drag(DebounceManager *manager, MouseButton button, bool enabled);
DebounceButtonConfig debounce_get_button_config(DebounceManager *manager, MouseButton button);
void debounce_set_threshold(DebounceManager *manager, MouseButton button, uint32_t threshold_ms, uint32_t min_threshold_ms, uint32_t max_threshold_ms);
void debounce_apply_preset(DebounceManager *manager, const DebouncePreset *preset);
bool debounce_matches_preset(DebounceManager *manager, const DebouncePreset *preset);
void debounce_set_monitored(DebounceManager *manager, MouseButton button, bool monitored);
uint32_t debounce_get_total_blocks(DebounceManager *manager);
uint32_t debounce_get_button_blocks(DebounceManager *manager, MouseButton button);
const char *debounce_get_button_name(MouseButton button);
bool debounce_is_any_monitored(DebounceManager *manager);
void debounce_reset_statistics(DebounceManager *manager);
void debounce_check_deferred_releases(DebounceManager *manager);
bool debounce_has_pending_releases(DebounceManager *manager);
void debounce_flush_pending_releases(DebounceManager *manager);
void debounce_inject_events(DebounceManager *manager, const INPUT *inputs, const MouseButton *buttons, int count);
uint64_t debounce_get_timestamp(DebounceManager *manager);
