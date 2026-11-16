#include "debouncer.h"
#include <string.h>

/*
 * Smart Drag - State machine based drag protection
 *
 * State transitions:
 *   IDLE -> PRESSED -> DRAGGING -> CONFIRMING -> IDLE
 *                |                       ^
 *              BLOCKED -----------------+
 *                  (bounce down cancels confirm)
 */

#define SMART_DRAG_HOLD_THRESHOLD_MS  200
#define SMART_DRAG_DIST_THRESHOLD_SQ  25   /* 5px */
#define SMART_DRAG_CONFIRM_TIMEOUT_MS 150

/* Unit tests link this file without a desktop; they must not inject input. */
#ifndef MOUSEFIX_TESTING
#define MOUSEFIX_TESTING 0
#endif

static const uint32_t default_thresholds[MOUSE_BUTTON_COUNT] = DEBOUNCE_DEFAULT_THRESHOLDS;

const DebouncePreset DEBOUNCE_PRESETS[DEBOUNCE_PRESET_COUNT] = {
    DEBOUNCE_DEFAULT_THRESHOLDS,
    {60, 60, 60, 60, 60, 35},
    {40, 40, 40, 40, 40, 20},
};

/* QPC ticks to microseconds without overflowing on a long uptime. At 10 MHz,
   ticks * 1e6 overflows around day 10; splitting off the seconds keeps it safe. */
static uint64_t qpc_to_us(uint64_t ticks, uint64_t frequency)
{
    if (frequency == 0)
        return 0;
    return (ticks / frequency) * 1000000ULL + (ticks % frequency) * 1000000ULL / frequency;
}

uint64_t debounce_get_timestamp(DebounceManager *manager)
{
    if (manager && manager->qpc_available)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return qpc_to_us((uint64_t)now.QuadPart, (uint64_t)manager->qpc_frequency);
    }
    return GetTickCount64() * 1000;
}

static uint64_t timestamp_to_ms(uint64_t timestamp)
{
    return timestamp / 1000;
}

/* Fill an INPUT for releasing one button. Returns false for wheel. */
static bool get_button_up(MouseButton button, DWORD *flags, DWORD *mouse_data)
{
    *flags = 0;
    *mouse_data = 0;
    switch (button)
    {
    case MOUSE_BUTTON_LEFT:
        *flags = MOUSEEVENTF_LEFTUP;
        return true;
    case MOUSE_BUTTON_RIGHT:
        *flags = MOUSEEVENTF_RIGHTUP;
        return true;
    case MOUSE_BUTTON_MIDDLE:
        *flags = MOUSEEVENTF_MIDDLEUP;
        return true;
    case MOUSE_BUTTON_X1:
        *flags = MOUSEEVENTF_XUP;
        *mouse_data = XBUTTON1;
        return true;
    case MOUSE_BUTTON_X2:
        *flags = MOUSEEVENTF_XUP;
        *mouse_data = XBUTTON2;
        return true;
    default:
        return false;
    }
}

/* Same for pressing one button. */
static bool get_button_down(MouseButton button, DWORD *flags, DWORD *mouse_data)
{
    *flags = 0;
    *mouse_data = 0;
    switch (button)
    {
    case MOUSE_BUTTON_LEFT:
        *flags = MOUSEEVENTF_LEFTDOWN;
        return true;
    case MOUSE_BUTTON_RIGHT:
        *flags = MOUSEEVENTF_RIGHTDOWN;
        return true;
    case MOUSE_BUTTON_MIDDLE:
        *flags = MOUSEEVENTF_MIDDLEDOWN;
        return true;
    case MOUSE_BUTTON_X1:
        *flags = MOUSEEVENTF_XDOWN;
        *mouse_data = XBUTTON1;
        return true;
    case MOUSE_BUTTON_X2:
        *flags = MOUSEEVENTF_XDOWN;
        *mouse_data = XBUTTON2;
        return true;
    default:
        return false;
    }
}

/* Injection worker. SendInput is only ever called here, because calling it
   from inside the low-level hook callback blocks the input queue and freezes
   the mouse until the process dies. */
static DWORD WINAPI debounce_injection_thread(LPVOID param)
{
    DebounceManager *manager = (DebounceManager *)param;

    for (;;)
    {
        WaitForSingleObject(manager->inject_event, INFINITE);
        if (manager->inject_stop)
            break;

        INPUT batch[DEBOUNCE_INJECT_QUEUE_CAP];
        int count;

        EnterCriticalSection(&manager->inject_cs);
        count = manager->inject_count;
        if (count > 0)
            memcpy(batch, manager->inject_queue, (size_t)count * sizeof(INPUT));
        manager->inject_count = 0;
        LeaveCriticalSection(&manager->inject_cs);

        if (count > 0)
            SendInput((UINT)count, batch, sizeof(INPUT));
    }

    return 0;
}

/* Queue injected events; one SendInput call per batch keeps their order. */
static void debounce_enqueue_inputs(DebounceManager *manager, const INPUT *inputs, int count)
{
    if (!manager || count <= 0 || !manager->inject_thread)
        return;

    EnterCriticalSection(&manager->inject_cs);
    for (int i = 0; i < count && manager->inject_count < DEBOUNCE_INJECT_QUEUE_CAP; i++)
        manager->inject_queue[manager->inject_count++] = inputs[i];
    LeaveCriticalSection(&manager->inject_cs);

    SetEvent(manager->inject_event);
}

bool debounce_init(DebounceManager *manager)
{
    if (!manager)
        return false;

    memset(manager, 0, sizeof(DebounceManager));
    manager->use_hybrid_heuristic = true;

    /* Fresh-install default state: every button monitored, default thresholds. */
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        manager->buttons[i].thresholdMs = default_thresholds[i];
        manager->buttons[i].isMonitored = true;
    }

    LARGE_INTEGER freq;
    manager->qpc_available = QueryPerformanceFrequency(&freq) != 0;
    manager->qpc_frequency = manager->qpc_available ? freq.QuadPart : 0;

    InitializeCriticalSection(&manager->cs);
    manager->cs_ready = true;

    if (!MOUSEFIX_TESTING)
    {
        InitializeCriticalSection(&manager->inject_cs);
        manager->inject_cs_ready = true;

        manager->inject_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!manager->inject_event)
        {
            debounce_cleanup(manager);
            return false;
        }

        manager->inject_thread = CreateThread(NULL, 0, debounce_injection_thread, manager, 0, NULL);
        if (!manager->inject_thread)
        {
            debounce_cleanup(manager);
            return false;
        }
    }

    return true;
}

void debounce_cleanup(DebounceManager *manager)
{
    if (!manager)
        return;

    if (manager->inject_thread)
    {
        InterlockedExchange(&manager->inject_stop, 1);
        SetEvent(manager->inject_event);
        WaitForSingleObject(manager->inject_thread, 2000);
        CloseHandle(manager->inject_thread);
        manager->inject_thread = NULL;
    }
    if (manager->inject_event)
    {
        CloseHandle(manager->inject_event);
        manager->inject_event = NULL;
    }
    if (manager->inject_cs_ready)
    {
        DeleteCriticalSection(&manager->inject_cs);
        manager->inject_cs_ready = false;
    }
    if (manager->cs_ready)
    {
        DeleteCriticalSection(&manager->cs);
        manager->cs_ready = false;
    }
}

bool debounce_process_event(DebounceManager *manager, const MouseEvent *event)
{
    if (!manager || !event || event->button < 0 || event->button >= MOUSE_BUTTON_COUNT)
        return false;

    EnterCriticalSection(&manager->cs);

    ButtonDebounceData *data = &manager->buttons[event->button];

    if (event->is_injected)
    {
        LeaveCriticalSection(&manager->cs);
        return false;
    }

    if (!data->isMonitored)
    {
        LeaveCriticalSection(&manager->cs);
        return false;
    }

    bool should_block = false;
    bool replay_press = false;

    /* Wheel handling */
    if (event->button == MOUSE_BUTTON_WHEEL)
    {
        int32_t wheel_delta = event->data;
        int32_t direction_sign = (wheel_delta > 0) ? 1 : (wheel_delta < 0) ? -1 : 0;

        if (direction_sign == 0)
        {
            LeaveCriticalSection(&manager->cs);
            return false;
        }

        if (data->wheelDirection != 0 && data->wheelDirection != direction_sign)
        {
            uint64_t elapsed_time = event->timestamp - data->previousTime;
            if (timestamp_to_ms(elapsed_time) <= data->thresholdMs)
            {
                data->blocks++;
                should_block = true;
            }
        }

        data->wheelDirection = direction_sign;
        data->previousTime = event->timestamp;
    }
    /* Button handling - state machine */
    else
    {
        uint64_t now = event->timestamp;
        uint64_t elapsed = now - data->previousTime;

        if (event->is_down)
        {
            switch (data->state)
            {
            case BTN_STATE_IDLE:
            case BTN_STATE_PRESSED:
            case BTN_STATE_DRAGGING:
                if (timestamp_to_ms(elapsed) <= data->thresholdMs)
                {
                    data->state = BTN_STATE_BLOCKED;
                    data->blocks++;
                    should_block = true;
                }
                else
                {
                    data->state = BTN_STATE_PRESSED;
                    data->downTime = now;
                    data->downPoint.x = event->x;
                    data->downPoint.y = event->y;
                }
                break;

            case BTN_STATE_CONFIRMING:
                /* A new press arrived while the previous drag release is
                   still held. Swallow this physical press and replay it as
                   release + press, so the target sees a complete click and
                   the drag ends in the same instant. Letting the physical
                   press through instead would race the held release. */
                data->state = BTN_STATE_PRESSED;
                data->downTime = now;
                data->downPoint.x = event->x;
                data->downPoint.y = event->y;
                should_block = true;
                replay_press = true;
                break;

            case BTN_STATE_BLOCKED:
                data->blocks++;
                should_block = true;
                break;
            }
        }
        else
        {
            switch (data->state)
            {
            case BTN_STATE_IDLE:
                break;

            case BTN_STATE_BLOCKED:
                /* The matching press was already counted; blocking the
                   release keeps the pair consistent without counting twice. */
                data->state = BTN_STATE_IDLE;
                should_block = true;
                break;

            case BTN_STATE_PRESSED:
                if (manager->use_hybrid_heuristic)
                {
                    uint64_t holdTime = now - data->downTime;
                    long dx = event->x - data->downPoint.x;
                    long dy = event->y - data->downPoint.y;
                    long distSq = dx * dx + dy * dy;

                    if (timestamp_to_ms(holdTime) > SMART_DRAG_HOLD_THRESHOLD_MS || distSq > SMART_DRAG_DIST_THRESHOLD_SQ)
                    {
                        data->state = BTN_STATE_CONFIRMING;
                        data->confirmStartTime = now;
                        should_block = true;
                    }
                    else
                    {
                        data->state = BTN_STATE_IDLE;
                    }
                }
                else
                {
                    data->state = BTN_STATE_IDLE;
                }
                break;

            case BTN_STATE_DRAGGING:
                if (manager->use_hybrid_heuristic)
                {
                    data->state = BTN_STATE_CONFIRMING;
                    data->confirmStartTime = now;
                    should_block = true;
                }
                else
                {
                    data->state = BTN_STATE_IDLE;
                }
                break;

            case BTN_STATE_CONFIRMING:
                /* Held for the confirm window and replayed, not dropped. */
                should_block = true;
                break;
            }
        }

        data->previousTime = now;
    }

    LeaveCriticalSection(&manager->cs);

    /* Queue the held-back release followed by the replayed press. The two go
       in one SendInput batch so their order is guaranteed. */
    if (replay_press)
    {
        DWORD up_flags, up_data, down_flags, down_data;
        if (get_button_up(event->button, &up_flags, &up_data) &&
            get_button_down(event->button, &down_flags, &down_data))
        {
            INPUT inputs[2] = {0};
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dwFlags = up_flags;
            inputs[0].mi.mouseData = up_data;
            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dwFlags = down_flags;
            inputs[1].mi.mouseData = down_data;
            debounce_enqueue_inputs(manager, inputs, 2);
        }
    }

    return should_block;
}

void debounce_check_deferred_releases(DebounceManager *manager)
{
    if (!manager)
        return;

    INPUT inputs[MOUSE_BUTTON_COUNT] = {0};
    int count = 0;

    EnterCriticalSection(&manager->cs);
    uint64_t now = debounce_get_timestamp(manager);

    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        ButtonDebounceData *data = &manager->buttons[i];
        if (data->state == BTN_STATE_CONFIRMING)
        {
            uint64_t elapsed_ms = timestamp_to_ms(now - data->confirmStartTime);
            if (elapsed_ms >= SMART_DRAG_CONFIRM_TIMEOUT_MS)
            {
                DWORD flags, mouse_data;
                data->state = BTN_STATE_IDLE;
                if (get_button_up((MouseButton)i, &flags, &mouse_data))
                {
                    inputs[count].type = INPUT_MOUSE;
                    inputs[count].mi.dwFlags = flags;
                    inputs[count].mi.mouseData = mouse_data;
                    count++;
                }
            }
        }
    }
    LeaveCriticalSection(&manager->cs);

    debounce_enqueue_inputs(manager, inputs, count);
}

void debounce_set_threshold(DebounceManager *manager, MouseButton button, uint32_t threshold_ms, uint32_t min_threshold_ms, uint32_t max_threshold_ms)
{
    if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)
        return;
    if (threshold_ms < min_threshold_ms || threshold_ms > max_threshold_ms)
        return;

    EnterCriticalSection(&manager->cs);
    manager->buttons[button].thresholdMs = threshold_ms;
    LeaveCriticalSection(&manager->cs);
}

void debounce_apply_preset(DebounceManager *manager, const DebouncePreset *preset)
{
    if (!manager || !preset)
        return;

    EnterCriticalSection(&manager->cs);
    manager->buttons[MOUSE_BUTTON_LEFT].thresholdMs = preset->left;
    manager->buttons[MOUSE_BUTTON_RIGHT].thresholdMs = preset->right;
    manager->buttons[MOUSE_BUTTON_MIDDLE].thresholdMs = preset->middle;
    manager->buttons[MOUSE_BUTTON_X1].thresholdMs = preset->x1;
    manager->buttons[MOUSE_BUTTON_X2].thresholdMs = preset->x2;
    manager->buttons[MOUSE_BUTTON_WHEEL].thresholdMs = preset->wheel;
    LeaveCriticalSection(&manager->cs);
}

bool debounce_matches_preset(const DebounceManager *manager, const DebouncePreset *preset)
{
    if (!manager || !preset)
        return false;

    return manager->buttons[MOUSE_BUTTON_LEFT].thresholdMs == preset->left &&
           manager->buttons[MOUSE_BUTTON_RIGHT].thresholdMs == preset->right &&
           manager->buttons[MOUSE_BUTTON_MIDDLE].thresholdMs == preset->middle &&
           manager->buttons[MOUSE_BUTTON_X1].thresholdMs == preset->x1 &&
           manager->buttons[MOUSE_BUTTON_X2].thresholdMs == preset->x2 &&
           manager->buttons[MOUSE_BUTTON_WHEEL].thresholdMs == preset->wheel;
}

void debounce_set_hybrid_heuristic(DebounceManager *manager, bool use_hybrid)
{
    if (!manager)
        return;

    /* Turning Smart Drag off must not strand a release that was already held
       back: the physical up was swallowed, so send it before dropping the
       CONFIRMING state or the OS keeps seeing the button as pressed. */
    INPUT releases[MOUSE_BUTTON_COUNT] = {0};
    int release_count = 0;

    EnterCriticalSection(&manager->cs);
    manager->use_hybrid_heuristic = use_hybrid;

    if (!use_hybrid)
    {
        for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
        {
            if (manager->buttons[i].state == BTN_STATE_CONFIRMING)
            {
                DWORD flags, mouse_data;
                manager->buttons[i].state = BTN_STATE_IDLE;
                if (get_button_up((MouseButton)i, &flags, &mouse_data))
                {
                    releases[release_count].type = INPUT_MOUSE;
                    releases[release_count].mi.dwFlags = flags;
                    releases[release_count].mi.mouseData = mouse_data;
                    release_count++;
                }
            }
        }
    }
    LeaveCriticalSection(&manager->cs);

    debounce_enqueue_inputs(manager, releases, release_count);
}

void debounce_set_monitored(DebounceManager *manager, MouseButton button, bool monitored)
{
    if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)
        return;

    EnterCriticalSection(&manager->cs);
    manager->buttons[button].isMonitored = monitored;
    LeaveCriticalSection(&manager->cs);
}

uint32_t debounce_get_total_blocks(DebounceManager *manager)
{
    if (!manager)
        return 0;

    EnterCriticalSection(&manager->cs);
    uint32_t total = 0;
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
        total += manager->buttons[i].blocks;
    LeaveCriticalSection(&manager->cs);
    return total;
}

uint32_t debounce_get_button_blocks(DebounceManager *manager, MouseButton button)
{
    if (!manager || button < 0 || button >= MOUSE_BUTTON_COUNT)
        return 0;

    EnterCriticalSection(&manager->cs);
    uint32_t blocks = manager->buttons[button].blocks;
    LeaveCriticalSection(&manager->cs);
    return blocks;
}

const char *debounce_get_button_name(MouseButton button)
{
    switch (button)
    {
    case MOUSE_BUTTON_LEFT:   return "Left";
    case MOUSE_BUTTON_RIGHT:  return "Right";
    case MOUSE_BUTTON_MIDDLE: return "Middle";
    case MOUSE_BUTTON_X1:     return "4th";
    case MOUSE_BUTTON_X2:     return "5th";
    case MOUSE_BUTTON_WHEEL:  return "Wheel";
    default:                  return "Unknown";
    }
}

bool debounce_is_any_monitored(DebounceManager *manager)
{
    if (!manager)
        return false;

    EnterCriticalSection(&manager->cs);
    bool any = false;
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        if (manager->buttons[i].isMonitored)
        {
            any = true;
            break;
        }
    }
    LeaveCriticalSection(&manager->cs);
    return any;
}

void debounce_reset_statistics(DebounceManager *manager)
{
    if (!manager)
        return;

    /* A button in the confirm window has a held-back release. Dropping the
       state without sending it leaves the OS thinking the button is down. */
    INPUT releases[MOUSE_BUTTON_COUNT] = {0};
    int release_count = 0;

    EnterCriticalSection(&manager->cs);
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        if (manager->buttons[i].state == BTN_STATE_CONFIRMING)
        {
            DWORD flags, mouse_data;
            if (get_button_up((MouseButton)i, &flags, &mouse_data))
            {
                releases[release_count].type = INPUT_MOUSE;
                releases[release_count].mi.dwFlags = flags;
                releases[release_count].mi.mouseData = mouse_data;
                release_count++;
            }
        }

        manager->buttons[i].blocks = 0;
        manager->buttons[i].state = BTN_STATE_IDLE;
        manager->buttons[i].wheelDirection = 0;
    }
    LeaveCriticalSection(&manager->cs);

    debounce_enqueue_inputs(manager, releases, release_count);
}
