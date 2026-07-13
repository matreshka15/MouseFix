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

uint64_t debounce_get_timestamp(DebounceManager *manager)
{
    if (manager && manager->qpc_available)
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (uint64_t)(now.QuadPart * 1000000 / manager->qpc_frequency);
    }
    return GetTickCount64() * 1000;
}

static uint64_t timestamp_to_ms(uint64_t timestamp, bool qpc_available)
{
    return timestamp / 1000;
}

bool debounce_init(DebounceManager *manager)
{
    if (!manager)
        return false;

    memset(manager, 0, sizeof(DebounceManager));
    manager->use_hybrid_heuristic = true;

    LARGE_INTEGER freq;
    manager->qpc_available = QueryPerformanceFrequency(&freq) != 0;
    manager->qpc_frequency = manager->qpc_available ? freq.QuadPart : 0;

    InitializeCriticalSection(&manager->cs);
    return true;
}

void debounce_cleanup(DebounceManager *manager)
{
    if (!manager)
        return;
    DeleteCriticalSection(&manager->cs);
}

bool debounce_process_event(DebounceManager *manager, const MouseEvent *event)
{
    if (!manager || !event)
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
            if (elapsed_time <= data->thresholdMs)
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
                if (elapsed <= data->thresholdMs)
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
                /* Bounce down during confirm: cancel, back to DRAGGING */
                data->state = BTN_STATE_DRAGGING;
                data->downTime = now;
                data->downPoint.x = event->x;
                data->downPoint.y = event->y;
                data->blocks++;
                should_block = true;
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
                data->state = BTN_STATE_IDLE;
                data->blocks++;
                should_block = true;
                break;

            case BTN_STATE_PRESSED:
                if (manager->use_hybrid_heuristic)
                {
                    uint64_t holdTime = now - data->downTime;
                    long dx = event->x - data->downPoint.x;
                    long dy = event->y - data->downPoint.y;
                    long distSq = dx * dx + dy * dy;

                    if (holdTime > SMART_DRAG_HOLD_THRESHOLD_MS || distSq > SMART_DRAG_DIST_THRESHOLD_SQ)
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
                data->blocks++;
                should_block = true;
                break;
            }
        }

        data->previousTime = now;
    }

    LeaveCriticalSection(&manager->cs);
    return should_block;
}

void debounce_check_deferred_releases(DebounceManager *manager)
{
    if (!manager)
        return;

    struct
    {
        bool inject;
        DWORD flags;
        DWORD mouseData;
    } actions[MOUSE_BUTTON_COUNT] = {0};

    EnterCriticalSection(&manager->cs);
    uint64_t now = debounce_get_timestamp(manager);

    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        ButtonDebounceData *data = &manager->buttons[i];
        if (data->state == BTN_STATE_CONFIRMING)
        {
            uint64_t elapsed_ms = timestamp_to_ms(now - data->confirmStartTime, manager->qpc_available);
            if (elapsed_ms >= SMART_DRAG_CONFIRM_TIMEOUT_MS)
            {
                data->state = BTN_STATE_IDLE;
                actions[i].inject = true;

                switch (i)
                {
                case MOUSE_BUTTON_LEFT:
                    actions[i].flags = MOUSEEVENTF_LEFTUP;
                    break;
                case MOUSE_BUTTON_RIGHT:
                    actions[i].flags = MOUSEEVENTF_RIGHTUP;
                    break;
                case MOUSE_BUTTON_MIDDLE:
                    actions[i].flags = MOUSEEVENTF_MIDDLEUP;
                    break;
                case MOUSE_BUTTON_X1:
                    actions[i].flags = MOUSEEVENTF_XUP;
                    actions[i].mouseData = XBUTTON1;
                    break;
                case MOUSE_BUTTON_X2:
                    actions[i].flags = MOUSEEVENTF_XUP;
                    actions[i].mouseData = XBUTTON2;
                    break;
                }
            }
        }
    }
    LeaveCriticalSection(&manager->cs);

    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        if (actions[i].inject && actions[i].flags)
        {
            INPUT input = {0};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = actions[i].flags;
            input.mi.mouseData = actions[i].mouseData;
            SendInput(1, &input, sizeof(INPUT));
        }
    }
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

void debounce_set_hybrid_heuristic(DebounceManager *manager, bool use_hybrid)
{
    if (!manager)
        return;

    EnterCriticalSection(&manager->cs);
    manager->use_hybrid_heuristic = use_hybrid;

    if (!use_hybrid)
    {
        for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
        {
            if (manager->buttons[i].state == BTN_STATE_CONFIRMING)
                manager->buttons[i].state = BTN_STATE_IDLE;
        }
    }
    LeaveCriticalSection(&manager->cs);
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

    EnterCriticalSection(&manager->cs);
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++)
    {
        manager->buttons[i].blocks = 0;
        manager->buttons[i].state = BTN_STATE_IDLE;
        manager->buttons[i].wheelDirection = 0;
    }
    LeaveCriticalSection(&manager->cs);
}
