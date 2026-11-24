#include <stdio.h>
#include <assert.h>
#include "../src/core/debouncer.h"
#include "../src/core/settings_migration.h"
#include "../src/utils/review_prompt.h"
#include "../src/utils/store_license.h"

static int test_count = 0;
static int check_count = 0;
static int pass_count = 0;
static int fail_count = 0;

/* SendInput stubs so the tests never inject real input. */
static UINT stub_send_count = 0;
static UINT WINAPI stub_send(UINT c, LPINPUT p, int s) { (void)p; (void)s; stub_send_count = c; return c; }
static UINT WINAPI stub_send_fail(UINT c, LPINPUT p, int s) { (void)c; (void)p; (void)s; return 0; }

/* Captures a whole batch so the test can check what is queued for injection. */
static INPUT captured_inputs[DEBOUNCE_INJECT_QUEUE_CAP];
static UINT captured_count = 0;
static UINT WINAPI stub_send_capture(UINT c, LPINPUT p, int s)
{
    (void)s;
    captured_count = c;
    for (UINT i = 0; i < c && i < DEBOUNCE_INJECT_QUEUE_CAP; i++)
        captured_inputs[i] = p[i];
    return c;
}

/* Unit tests have no injection thread; the queued batch is read directly. */
static void clear_queue(DebounceManager *manager)
{
    manager->inject_count = 0;
}

#define TEST(name) \
    do { printf("\n[TEST] %s\n", name); test_count++; } while(0)

#define CHECK(condition, msg) \
    do { \
        check_count++; \
        if (condition) { \
            printf("  PASS: %s\n", msg); \
            pass_count++; \
        } else { \
            printf("  FAIL: %s\n", msg); \
            fail_count++; \
        } \
    } while(0)

int main(void)
{
    printf("================================================\n");
    printf("Smart Drag Tests\n");
    printf("================================================\n");

    DebounceManager manager;
    if (!debounce_init(&manager))
    {
        printf("Init failed\n");
        return 1;
    }

    debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);

    /* Test 1: Normal click */
    TEST("Normal click - should not block");
    debounce_reset_statistics(&manager);

    MouseEvent down1 = {0};
    down1.button = MOUSE_BUTTON_LEFT;
    down1.timestamp = 1000000;   /* 1.000s in us */
    down1.is_down = true;
    down1.x = 100;
    down1.y = 100;
    CHECK(!debounce_process_event(&manager, &down1), "DOWN passed");

    MouseEvent up1 = {0};
    up1.button = MOUSE_BUTTON_LEFT;
    up1.timestamp = 1060000;     /* +60ms */
    up1.is_down = false;
    up1.x = 100;
    up1.y = 100;
    CHECK(!debounce_process_event(&manager, &up1), "UP passed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "State back to IDLE");

    /* Test 2: Fast double-click bounce (second down within 50ms) */
    TEST("Fast double-click - bounce should be blocked");
    debounce_reset_statistics(&manager);

    MouseEvent down2a = {0};
    down2a.button = MOUSE_BUTTON_LEFT;
    down2a.timestamp = 2000000;
    down2a.is_down = true;
    down2a.x = 100;
    down2a.y = 100;
    debounce_process_event(&manager, &down2a);

    MouseEvent up2a = {0};
    up2a.button = MOUSE_BUTTON_LEFT;
    up2a.timestamp = 2060000;
    up2a.is_down = false;
    up2a.x = 100;
    up2a.y = 100;
    debounce_process_event(&manager, &up2a);

    MouseEvent down2b = {0};
    down2b.button = MOUSE_BUTTON_LEFT;
    down2b.timestamp = 2080000;   /* +20ms since up */
    down2b.is_down = true;
    down2b.x = 100;
    down2b.y = 100;
    CHECK(debounce_process_event(&manager, &down2b), "Bounce DOWN blocked");

    MouseEvent up2b = {0};
    up2b.button = MOUSE_BUTTON_LEFT;
    up2b.timestamp = 2140000;
    up2b.is_down = false;
    up2b.x = 100;
    up2b.y = 100;
    CHECK(debounce_process_event(&manager, &up2b), "Bounce UP blocked");

    /* Test 3: Long press no move - Smart Drag (200ms hold), slow click. */
    TEST("Long press no move - Smart Drag should trigger");
    debounce_reset_statistics(&manager);

    MouseEvent down3 = {0};
    down3.button = MOUSE_BUTTON_LEFT;
    down3.timestamp = 3000000;
    down3.is_down = true;
    down3.x = 100;
    down3.y = 100;
    debounce_process_event(&manager, &down3);

    MouseEvent up3 = {0};
    up3.button = MOUSE_BUTTON_LEFT;
    up3.timestamp = 3300000;      /* +300ms hold */
    up3.is_down = false;
    up3.x = 100;
    up3.y = 100;
    CHECK(debounce_process_event(&manager, &up3), "UP blocked (holdTime=300ms)");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "State is CONFIRMING");
    CHECK(!manager.buttons[MOUSE_BUTTON_LEFT].releaseWasDrag, "releaseWasDrag false for a slow click");

    /* A press during a slow-click confirm is replayed: the press time resets. */
    MouseEvent down3b = {0};
    down3b.button = MOUSE_BUTTON_LEFT;
    down3b.timestamp = 3330000;
    down3b.is_down = true;
    down3b.x = 100;
    down3b.y = 100;
    CHECK(debounce_process_event(&manager, &down3b), "Slow-click press swallowed for replay");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED, "State is PRESSED");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == 3330000, "downTime reset for slow-click replay");

    /* Test 4: Move drag - Smart Drag (distance past 5px threshold) */
    TEST("Press and move - Smart Drag should trigger");
    debounce_reset_statistics(&manager);

    MouseEvent down4 = {0};
    down4.button = MOUSE_BUTTON_LEFT;
    down4.timestamp = 4000000;
    down4.is_down = true;
    down4.x = 100;
    down4.y = 100;
    debounce_process_event(&manager, &down4);

    MouseEvent up4 = {0};
    up4.button = MOUSE_BUTTON_LEFT;
    up4.timestamp = 4100000;      /* +100ms, moved 30px */
    up4.is_down = false;
    up4.x = 130;
    up4.y = 100;
    CHECK(debounce_process_event(&manager, &up4), "UP blocked (dist=30px)");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "State is CONFIRMING");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].releaseWasDrag, "releaseWasDrag true for a moved drag");

    /* Test 5: Jitter during a drag must not break the drag. */
    TEST("Jitter during a drag does not break it");
    debounce_reset_statistics(&manager);

    MouseEvent down5 = {0};
    down5.button = MOUSE_BUTTON_LEFT;
    down5.timestamp = 5000000;
    down5.is_down = true;
    down5.x = 100;
    down5.y = 100;
    debounce_process_event(&manager, &down5);

    MouseEvent up5 = {0};
    up5.button = MOUSE_BUTTON_LEFT;
    up5.timestamp = 5300000;
    up5.is_down = false;
    up5.x = 200;
    up5.y = 200;
    CHECK(debounce_process_event(&manager, &up5), "Drag release swallowed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "Entered CONFIRMING");

    MouseEvent down5b = {0};
    down5b.button = MOUSE_BUTTON_LEFT;
    down5b.timestamp = 5330000;   /* +30ms jitter */
    down5b.is_down = true;
    down5b.x = 200;
    down5b.y = 200;
    CHECK(debounce_process_event(&manager, &down5b), "Jitter press swallowed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED, "State back to PRESSED");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == 5000000, "downTime unchanged");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downPoint.x == 100 &&
          manager.buttons[MOUSE_BUTTON_LEFT].downPoint.y == 100, "downPoint unchanged");

    /* Test 6: Confirm window - releases at 150ms, not before */
    TEST("Confirm window - release on time");
    debounce_reset_statistics(&manager);

    /* Past the 150ms window: should release */
    uint64_t now_us = debounce_get_timestamp(&manager);
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_LEFT].confirmStartTime = now_us - 151000; /* 151ms ago in us */
    debounce_check_deferred_releases(&manager);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "Released after 150ms window");

    /* Inside the 150ms window: should hold */
    now_us = debounce_get_timestamp(&manager);
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_LEFT].confirmStartTime = now_us - 149000; /* 149ms ago in us */
    debounce_check_deferred_releases(&manager);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "Still CONFIRMING before 150ms");

    /* Test 7: Presets set all six thresholds and match back */
    TEST("Presets apply and match");
    for (int i = 0; i < DEBOUNCE_PRESET_COUNT; i++)
    {
        debounce_apply_preset(&manager, &DEBOUNCE_PRESETS[i]);
        CHECK(debounce_matches_preset(&manager, &DEBOUNCE_PRESETS[i]), "Preset matches after apply");
    }

    debounce_apply_preset(&manager, &DEBOUNCE_PRESETS[DEBOUNCE_PRESET_DEFAULT]);
    debounce_set_threshold(&manager, MOUSE_BUTTON_RIGHT, 77, 1, 200);
    CHECK(!debounce_matches_preset(&manager, &DEBOUNCE_PRESETS[DEBOUNCE_PRESET_DEFAULT]),
          "Edited threshold breaks the match");

    /* Test 8: A filtered press counts once, not once per blocked event */
    TEST("Statistics count one block per filtered press");
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_IDLE;
    debounce_reset_statistics(&manager);

    MouseEvent down7 = {0};
    down7.button = MOUSE_BUTTON_LEFT;
    down7.timestamp = 7000000;
    down7.is_down = true;
    down7.x = 100;
    down7.y = 100;
    debounce_process_event(&manager, &down7);

    MouseEvent up7 = {0};
    up7.button = MOUSE_BUTTON_LEFT;
    up7.timestamp = 7060000;
    up7.is_down = false;
    up7.x = 100;
    up7.y = 100;
    debounce_process_event(&manager, &up7);

    MouseEvent down7b = {0};
    down7b.button = MOUSE_BUTTON_LEFT;
    down7b.timestamp = 7080000;   /* +20ms since up */
    down7b.is_down = true;
    down7b.x = 100;
    down7b.y = 100;
    CHECK(debounce_process_event(&manager, &down7b), "Bounce DOWN blocked");

    MouseEvent up7b = {0};
    up7b.button = MOUSE_BUTTON_LEFT;
    up7b.timestamp = 7140000;
    up7b.is_down = false;
    up7b.x = 100;
    up7b.y = 100;
    CHECK(debounce_process_event(&manager, &up7b), "Bounce UP blocked");
    CHECK(debounce_get_total_blocks(&manager) == 1, "One block counted for the pair");
    CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 1, "Per-button count is one");

    /* Test 9: Out-of-range button ids must not touch memory */
    TEST("Out-of-range button is ignored");
    MouseEvent bad_negative = {0};
    bad_negative.button = MOUSE_BUTTON_UNKNOWN;
    bad_negative.timestamp = 9000000;
    bad_negative.is_down = true;
    CHECK(!debounce_process_event(&manager, &bad_negative), "Negative button passes through");

    MouseEvent bad_high = {0};
    bad_high.button = MOUSE_BUTTON_COUNT;
    bad_high.timestamp = 9000000;
    bad_high.is_down = true;
    CHECK(!debounce_process_event(&manager, &bad_high), "Button == COUNT passes through");

    /* Test 10: Reset must not leave a held-back release behind */
    TEST("Reset statistics clears a pending confirm");
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
    debounce_reset_statistics(&manager);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "CONFIRMING cleared by reset");
    CHECK(debounce_get_total_blocks(&manager) == 0, "Counts cleared");

    /* Test 11: Two consecutive jitters, drag survives both */
    TEST("Two consecutive jitters, drag survives");
    debounce_reset_statistics(&manager);

    MouseEvent down11 = {0};
    down11.button = MOUSE_BUTTON_LEFT;
    down11.timestamp = 11000000;
    down11.is_down = true;
    down11.x = 100;
    down11.y = 100;
    debounce_process_event(&manager, &down11);

    MouseEvent up11 = {0};
    up11.button = MOUSE_BUTTON_LEFT;
    up11.timestamp = 11300000;
    up11.is_down = false;
    up11.x = 300;
    up11.y = 300;
    debounce_process_event(&manager, &up11);

    MouseEvent down11b = {0};
    down11b.button = MOUSE_BUTTON_LEFT;
    down11b.timestamp = 11330000;
    down11b.is_down = true;
    down11b.x = 300;
    down11b.y = 300;
    debounce_process_event(&manager, &down11b);

    MouseEvent up11b = {0};
    up11b.button = MOUSE_BUTTON_LEFT;
    up11b.timestamp = 11360000;
    up11b.is_down = false;
    up11b.x = 301;
    up11b.y = 300;
    CHECK(debounce_process_event(&manager, &up11b), "First jitter release swallowed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "Still dragging after first jitter");

    MouseEvent down11c = {0};
    down11c.button = MOUSE_BUTTON_LEFT;
    down11c.timestamp = 11390000;
    down11c.is_down = true;
    down11c.x = 301;
    down11c.y = 301;
    CHECK(debounce_process_event(&manager, &down11c), "Second jitter press swallowed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED, "State is PRESSED");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == 11000000, "downTime still original after two jitters");

    /* Test 12: Right button has drag protection off by default */
    TEST("Right button: drag protection off by default");
    debounce_reset_statistics(&manager);
    manager.buttons[MOUSE_BUTTON_RIGHT].state = BTN_STATE_IDLE;

    MouseEvent down12 = {0};
    down12.button = MOUSE_BUTTON_RIGHT;
    down12.timestamp = 12000000;
    down12.is_down = true;
    down12.x = 100;
    down12.y = 100;
    debounce_process_event(&manager, &down12);

    MouseEvent up12 = {0};
    up12.button = MOUSE_BUTTON_RIGHT;
    up12.timestamp = 12300000;
    up12.is_down = false;
    up12.x = 300;
    up12.y = 300;
    CHECK(!debounce_process_event(&manager, &up12), "Right-button move release passes through");
    CHECK(manager.buttons[MOUSE_BUTTON_RIGHT].state == BTN_STATE_IDLE, "Right button not CONFIRMING");

    /* Test 13: Exit flush sends a held-back release */
    TEST("Exit flush sends a held-back release");
    debounce_reset_statistics(&manager);
    manager.send_input = stub_send;
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_LEFT].releaseWasDrag = true;
    stub_send_count = 0;
    debounce_flush_pending_releases(&manager);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "CONFIRMING flushed to IDLE");
    CHECK(stub_send_count == 1, "One release sent on exit flush");
    manager.send_input = NULL;

    /* Test 14: Disabling a button clears its state */
    TEST("Disabling a button clears its state");
    debounce_reset_statistics(&manager);
    manager.buttons[MOUSE_BUTTON_RIGHT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_RIGHT].releaseWasDrag = true;
    debounce_set_monitored(&manager, MOUSE_BUTTON_RIGHT, false);
    CHECK(manager.buttons[MOUSE_BUTTON_RIGHT].state == BTN_STATE_IDLE, "State cleared to IDLE");
    CHECK(!manager.buttons[MOUSE_BUTTON_RIGHT].isMonitored, "Button disabled");
    debounce_set_monitored(&manager, MOUSE_BUTTON_RIGHT, true);

    /* Test 15: SendInput failure resets the button state */
    TEST("SendInput failure resets the button state");
    debounce_reset_statistics(&manager);
    manager.send_input = stub_send_fail;
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_PRESSED;
    INPUT in = {0};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    MouseButton btn = MOUSE_BUTTON_LEFT;
    debounce_inject_events(&manager, &in, &btn, 1);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "Failed injection resets button to IDLE");
    manager.send_input = NULL;

    /* Test 16: cleanup then flush/has-pending must not crash. This is the
       shutdown-after-failed-init path: InitializeApp failed, debounce_cleanup
       already deleted the critical section, and ShutdownApp still calls flush. */
    TEST("Cleanup then flush/has-pending is safe");
    {
        DebounceManager m;
        debounce_init(&m);
        debounce_cleanup(&m);
        debounce_flush_pending_releases(&m);
        CHECK(!debounce_has_pending_releases(&m), "has_pending false after cleanup");
    }

    /* Test 17: Store license mapping - trial / full / expired / unknown. */
    TEST("Store license state mapping");
    CHECK(store_license_resolve(true, true, true) == STORE_LICENSE_TRIAL, "Active trial maps to TRIAL");
    CHECK(store_license_resolve(true, true, false) == STORE_LICENSE_FULL, "Purchased (active, not trial) maps to FULL");
    CHECK(store_license_resolve(true, false, false) == STORE_LICENSE_EXPIRED, "Trial over (inactive) maps to EXPIRED");
    CHECK(store_license_resolve(true, false, true) == STORE_LICENSE_EXPIRED, "Inactive trial still EXPIRED");
    CHECK(store_license_resolve(false, true, false) == STORE_LICENSE_UNKNOWN, "Failed query maps to UNKNOWN");

    /* Test 18: Review trigger - block count threshold. */
    TEST("Review trigger: 199 blocks no, 200 blocks yes");
    {
        ReviewDecisionInput in = {0};
        in.first_run_unix_seconds = 1000;
        in.now_unix_seconds = 1000 + REVIEW_SECONDS_PER_DAY; /* 1 day old */

        in.cumulative_blocks = 199;
        CHECK(!review_should_prompt(&in), "199 blocks does not prompt");
        in.cumulative_blocks = 200;
        CHECK(review_should_prompt(&in), "200 blocks prompts");
    }

    /* Test 19: Review trigger - already prompted never fires again. */
    TEST("Review trigger: already prompted is silent");
    {
        ReviewDecisionInput in = {0};
        in.cumulative_blocks = 500;
        in.already_prompted = true;
        CHECK(!review_should_prompt(&in), "Already prompted does not prompt again");
    }

    /* Test 20: Review trigger - first-run age threshold. */
    TEST("Review trigger: 7-day first run");
    {
        ReviewDecisionInput in = {0};
        in.cumulative_blocks = 0;
        in.first_run_unix_seconds = 1000;
        in.now_unix_seconds = 1000 + REVIEW_SECONDS_PER_DAY * 6;   /* 6 days */
        CHECK(!review_should_prompt(&in), "6 days old does not prompt");

        in.now_unix_seconds = 1000 + REVIEW_SECONDS_PER_DAY * 7;   /* 7 days */
        CHECK(review_should_prompt(&in), "7 days old prompts");
    }

    /* Test 21: Review trigger - no first-run date and no blocks stays quiet. */
    TEST("Review trigger: no signal is silent");
    {
        ReviewDecisionInput in = {0};
        in.cumulative_blocks = 0;
        in.first_run_unix_seconds = 0;
        in.now_unix_seconds = 999999999;
        CHECK(!review_should_prompt(&in), "No blocks and no first-run date stays quiet");
    }

    /* Test 22: Precision touchpad duplicate down, Smart Drag off.
       The duplicate is swallowed, but the real release must still go out. */
    TEST("Touchpad duplicate down: duplicate swallowed, release passes");
    debounce_reset_statistics(&manager);
    debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, false);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 1, 1, 200);

    MouseEvent down22 = {0};
    down22.button = MOUSE_BUTTON_LEFT;
    down22.timestamp = 20000000;
    down22.is_down = true;
    down22.x = 100;
    down22.y = 100;
    CHECK(!debounce_process_event(&manager, &down22), "First DOWN passes");

    MouseEvent dup22 = down22;
    dup22.timestamp += 200;  /* 200us later, inside the 1ms window */
    CHECK(debounce_process_event(&manager, &dup22), "Duplicate DOWN swallowed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED,
          "Still PRESSED after duplicate");
    CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 1,
          "Duplicate counted once");

    MouseEvent up22 = down22;
    up22.is_down = false;
    up22.timestamp = 20050000;
    CHECK(!debounce_process_event(&manager, &up22), "Release passes");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "Back to IDLE");

    /* Test 23: The same duplicate with Smart Drag on must not lose the press
       time; the eventual release still enters the confirm window. */
    TEST("Touchpad duplicate down: Smart Drag still sees the original press");
    debounce_reset_statistics(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);

    MouseEvent down23 = {0};
    down23.button = MOUSE_BUTTON_LEFT;
    down23.timestamp = 21000000;
    down23.is_down = true;
    down23.x = 100;
    down23.y = 100;
    CHECK(!debounce_process_event(&manager, &down23), "First DOWN passes");

    MouseEvent dup23 = down23;
    dup23.timestamp += 200;
    CHECK(debounce_process_event(&manager, &dup23), "Duplicate DOWN swallowed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == down23.timestamp,
          "downTime preserved");

    MouseEvent up23 = down23;
    up23.is_down = false;
    up23.timestamp = down23.timestamp + 300000;  /* 300ms hold */
    CHECK(debounce_process_event(&manager, &up23), "Release held by Smart Drag");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING,
          "State is CONFIRMING");
    CHECK(!manager.buttons[MOUSE_BUTTON_LEFT].releaseWasDrag,
          "Slow click, not a drag");
    debounce_reset_statistics(&manager);

    /* Test 24: Smart Drag is per button; turning it off flushes a held release. */
    TEST("Per-button Smart Drag: right button enable and disable");
    debounce_reset_statistics(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_RIGHT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_RIGHT, 50, 1, 200);

    MouseEvent down24 = {0};
    down24.button = MOUSE_BUTTON_RIGHT;
    down24.timestamp = 22000000;
    down24.is_down = true;
    down24.x = 100;
    down24.y = 100;
    CHECK(!debounce_process_event(&manager, &down24), "Right DOWN passes");

    MouseEvent up24 = down24;
    up24.is_down = false;
    up24.timestamp = down24.timestamp + 300000;
    CHECK(debounce_process_event(&manager, &up24), "Right release held");
    CHECK(manager.buttons[MOUSE_BUTTON_RIGHT].state == BTN_STATE_CONFIRMING,
          "Right is CONFIRMING");

    debounce_set_smart_drag(&manager, MOUSE_BUTTON_RIGHT, false);
    CHECK(manager.buttons[MOUSE_BUTTON_RIGHT].state == BTN_STATE_IDLE,
          "Disabling Smart Drag clears CONFIRMING");
    CHECK(!debounce_has_pending_releases(&manager), "No pending release");
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_RIGHT, false);

    /* Test 25: The UI snapshot returns all three settings under one lock. */
    TEST("Button config snapshot");
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 33, 1, 200);
    debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, false);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, false);

    DebounceButtonConfig config = debounce_get_button_config(&manager, MOUSE_BUTTON_LEFT);
    CHECK(config.threshold_ms == 33, "Snapshot threshold");
    CHECK(!config.monitored, "Snapshot monitored");
    CHECK(!config.smart_drag_enabled, "Snapshot Smart Drag");

    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);

    /* Test 26: down / duplicate down / up with Smart Drag off. The duplicate
       is swallowed, the press survives and the real up passes. */
    TEST("Duplicate down, Smart Drag off: down passes, dup swallowed, up passes");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, false);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 100000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        CHECK(!debounce_process_event(&manager, &down), "First DOWN passes");

        MouseEvent dup = down;
        dup.timestamp = t + 200; /* 0.2ms later */
        CHECK(debounce_process_event(&manager, &dup), "Duplicate DOWN blocked");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED,
              "State stays PRESSED");
        CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 1,
              "Duplicate counted once");

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 50000;
        CHECK(!debounce_process_event(&manager, &up), "Real UP passes");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State IDLE after UP");
        CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 1,
              "UP adds no block");
    }

    /* Test 27: the same sequence with Smart Drag on. A short press stays a
       click; the duplicate must not turn it into a drag. */
    TEST("Duplicate down, Smart Drag on: short click still passes");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 101000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        CHECK(!debounce_process_event(&manager, &down), "First DOWN passes");

        MouseEvent dup = down;
        dup.timestamp = t + 200;
        CHECK(debounce_process_event(&manager, &dup), "Duplicate DOWN blocked");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == t,
              "downTime preserved");

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 50000; /* 50ms hold, no move */
        CHECK(!debounce_process_event(&manager, &up), "UP passes, not a drag");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State IDLE");
    }

    /* Test 28: three downs then one up. Only the first passes, the two
       duplicates are swallowed separately and the up still passes. */
    TEST("down/down/down/up: two duplicates swallowed, up passes");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, false);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 102000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        CHECK(!debounce_process_event(&manager, &down), "First DOWN passes");

        MouseEvent dup1 = down;
        dup1.timestamp = t + 100;
        MouseEvent dup2 = down;
        dup2.timestamp = t + 200;
        CHECK(debounce_process_event(&manager, &dup1), "Second DOWN blocked");
        CHECK(debounce_process_event(&manager, &dup2), "Third DOWN blocked");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED,
              "Still PRESSED");
        CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 2,
              "Each duplicate counted once");

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 60000;
        CHECK(!debounce_process_event(&manager, &up), "UP passes");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State IDLE");
    }

    /* Test 29: a second up with no press in between must pass through and not
       count as anything. */
    TEST("Repeated UP in IDLE passes through");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 103000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        debounce_process_event(&manager, &down);

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 60000;
        CHECK(!debounce_process_event(&manager, &up), "First UP passes");

        MouseEvent up2 = up;
        up2.timestamp = t + 90000;
        CHECK(!debounce_process_event(&manager, &up2), "Second UP passes");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State IDLE");
        CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 0,
              "No block counted");
    }

    /* Test 30: bounce after a completed click. The press and its duplicate
       are one blocked press; the up closes it. */
    TEST("Bounce after up: pair blocked, duplicate not counted twice");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, false);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 104000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        debounce_process_event(&manager, &down);

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 60000;
        CHECK(!debounce_process_event(&manager, &up), "Click UP passes");

        MouseEvent bounce = down;
        bounce.timestamp = t + 80000; /* 20ms after the up */
        CHECK(debounce_process_event(&manager, &bounce), "Bounce DOWN blocked");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_BLOCKED,
              "State BLOCKED");

        MouseEvent bounce_dup = down;
        bounce_dup.timestamp = t + 80200;
        CHECK(debounce_process_event(&manager, &bounce_dup),
              "Duplicate bounce DOWN swallowed");
        CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 1,
              "Blocked press counted once, not per duplicate");

        MouseEvent bounce_up = down;
        bounce_up.is_down = false;
        bounce_up.timestamp = t + 140000;
        CHECK(debounce_process_event(&manager, &bounce_up), "Bounce UP blocked");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State IDLE after the bounce pair");
        CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 1,
              "Total still one block");
    }

    /* Test 31: duplicate down inside CONFIRMING after a real drag. The drag
       continues: no replay is queued and the original press is kept. */
    TEST("CONFIRMING (drag) + duplicate down: drag continues, nothing replayed");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 105000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        debounce_process_event(&manager, &down);

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 100000; /* 100ms, moved 30px */
        up.x = 40;
        CHECK(debounce_process_event(&manager, &up), "Drag UP held");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING,
              "State CONFIRMING");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].releaseWasDrag,
              "releaseWasDrag true");

        clear_queue(&manager);
        MouseEvent dup = down;
        dup.timestamp = t + 130000;
        dup.x = 40;
        dup.y = 40;
        CHECK(debounce_process_event(&manager, &dup),
              "Duplicate DOWN during drag swallowed");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED,
              "Back to PRESSED");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == t,
              "Original downTime kept");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downPoint.x == 10 &&
              manager.buttons[MOUSE_BUTTON_LEFT].downPoint.y == 10,
              "Original downPoint kept");
        CHECK(manager.inject_count == 0, "No replay queued for a drag");
    }

    /* Test 32: duplicate down inside CONFIRMING after a slow click. The held
       release is replayed first, then the new press, in one batch. */
    TEST("CONFIRMING (slow click) + duplicate down: release+press replayed");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 106000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        debounce_process_event(&manager, &down);

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 300000; /* 300ms, no move */
        CHECK(debounce_process_event(&manager, &up), "Slow-click UP held");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING,
              "State CONFIRMING");
        CHECK(!manager.buttons[MOUSE_BUTTON_LEFT].releaseWasDrag,
              "releaseWasDrag false");

        clear_queue(&manager);
        MouseEvent next_down = down;
        next_down.timestamp = t + 330000;
        CHECK(debounce_process_event(&manager, &next_down),
              "Press during the confirm window swallowed");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED,
              "State PRESSED for the replayed press");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == t + 330000,
              "downTime reset for the replayed press");
        CHECK(manager.inject_count == 2, "UP and DOWN queued together");
        if (manager.inject_count == 2)
        {
            CHECK(manager.inject_queue[0].input.mi.dwFlags == MOUSEEVENTF_LEFTUP,
                  "First queued event is LEFTUP");
            CHECK(manager.inject_queue[1].input.mi.dwFlags == MOUSEEVENTF_LEFTDOWN,
                  "Second queued event is LEFTDOWN");
            CHECK(manager.inject_queue[0].button == MOUSE_BUTTON_LEFT &&
                  manager.inject_queue[1].button == MOUSE_BUTTON_LEFT,
                  "Both events belong to the left button");
        }

        MouseEvent final_up = down;
        final_up.is_down = false;
        final_up.timestamp = t + 430000; /* 100ms after the replay press */
        CHECK(!debounce_process_event(&manager, &final_up),
              "Following UP passes");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State IDLE");
    }

    /* Test 33: disabling a button inside the confirm window flushes the held
       release instead of stranding it. */
    TEST("set_monitored(false) during CONFIRMING flushes the release");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 107000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        debounce_process_event(&manager, &down);

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 300000;
        debounce_process_event(&manager, &up);
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING,
              "Held release pending");

        clear_queue(&manager);
        debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, false);
        CHECK(!debounce_get_button_config(&manager, MOUSE_BUTTON_LEFT).monitored,
              "Button disabled");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State cleared");
        CHECK(manager.inject_count == 1, "One release queued");
        if (manager.inject_count == 1)
            CHECK(manager.inject_queue[0].input.mi.dwFlags == MOUSEEVENTF_LEFTUP,
                  "Queued event is LEFTUP");

        debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, true);
    }

    /* Test 34: turning Smart Drag off inside the confirm window does the same. */
    TEST("set_smart_drag(false) during CONFIRMING flushes the release");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 108000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        debounce_process_event(&manager, &down);

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 300000;
        debounce_process_event(&manager, &up);
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING,
              "Held release pending");

        clear_queue(&manager);
        debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, false);
        CHECK(!debounce_get_button_config(&manager, MOUSE_BUTTON_LEFT).smart_drag_enabled,
              "Smart Drag disabled");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State cleared");
        CHECK(manager.inject_count == 1, "One release queued");
        if (manager.inject_count == 1)
            CHECK(manager.inject_queue[0].input.mi.dwFlags == MOUSEEVENTF_LEFTUP,
                  "Queued event is LEFTUP");

        debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    }

    /* Test 35: Reset Statistics over several held releases sends one release
       per button, in button order, and clears every counter. */
    TEST("Reset statistics flushes every held release");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_LEFT].releaseWasDrag = true;
    manager.buttons[MOUSE_BUTTON_RIGHT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_RIGHT].releaseWasDrag = false;
    manager.buttons[MOUSE_BUTTON_LEFT].blocks = 5;
    manager.buttons[MOUSE_BUTTON_RIGHT].blocks = 7;

    debounce_reset_statistics(&manager);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE &&
          manager.buttons[MOUSE_BUTTON_RIGHT].state == BTN_STATE_IDLE,
          "Both buttons back to IDLE");
    CHECK(debounce_get_total_blocks(&manager) == 0, "Counters cleared");
    CHECK(manager.inject_count == 2, "Two releases queued");
    if (manager.inject_count == 2)
    {
        CHECK(manager.inject_queue[0].input.mi.dwFlags == MOUSEEVENTF_LEFTUP &&
              manager.inject_queue[1].input.mi.dwFlags == MOUSEEVENTF_RIGHTUP,
              "LEFTUP then RIGHTUP");
    }

    /* Test 36: the shutdown flush sends every pending release in one batch. */
    TEST("Shutdown flush sends releases for several buttons");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_RIGHT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_MIDDLE].state = BTN_STATE_CONFIRMING;

    manager.send_input = stub_send_capture;
    captured_count = 0;
    debounce_flush_pending_releases(&manager);
    CHECK(captured_count == 3, "One batch of three releases");
    CHECK(captured_inputs[0].mi.dwFlags == MOUSEEVENTF_LEFTUP &&
          captured_inputs[1].mi.dwFlags == MOUSEEVENTF_RIGHTUP &&
          captured_inputs[2].mi.dwFlags == MOUSEEVENTF_MIDDLEUP,
          "Batch is LEFTUP, RIGHTUP, MIDDLEUP");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE &&
          manager.buttons[MOUSE_BUTTON_RIGHT].state == BTN_STATE_IDLE &&
          manager.buttons[MOUSE_BUTTON_MIDDLE].state == BTN_STATE_IDLE,
          "All buttons IDLE");
    manager.send_input = NULL;

    /* Test 37: invalid button ids are rejected by every accessor. */
    TEST("Invalid button ids are ignored");
    {
        DebounceButtonConfig bad = debounce_get_button_config(&manager, (MouseButton)MOUSE_BUTTON_COUNT);
        CHECK(bad.threshold_ms == 0 && !bad.monitored && !bad.smart_drag_enabled,
              "Config for button == COUNT is empty");

        bad = debounce_get_button_config(&manager, MOUSE_BUTTON_UNKNOWN);
        CHECK(bad.threshold_ms == 0 && !bad.monitored && !bad.smart_drag_enabled,
              "Config for UNKNOWN is empty");

        debounce_set_smart_drag(&manager, (MouseButton)99, true);
        debounce_set_smart_drag(&manager, MOUSE_BUTTON_UNKNOWN, true);
        debounce_set_monitored(&manager, (MouseButton)-2, true);
        debounce_set_threshold(&manager, (MouseButton)MOUSE_BUTTON_COUNT, 999, 1, 200);
        CHECK(debounce_get_button_blocks(&manager, (MouseButton)99) == 0,
              "Block count for an invalid button is 0");
        CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) != 999,
              "Invalid writes did not land on a real button");

        /* Out-of-range threshold values are rejected too. */
        uint32_t before = debounce_get_button_config(&manager, MOUSE_BUTTON_LEFT).threshold_ms;
        debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 0, 1, 200);
        debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 201, 1, 200);
        CHECK(debounce_get_button_config(&manager, MOUSE_BUTTON_LEFT).threshold_ms == before,
              "Out-of-range thresholds rejected");
    }

    /* Test 38: Reset Statistics during a blocked bounce must keep swallowing
       the pending up, otherwise the target sees a release for a press it was
       never given. */
    TEST("Reset during a blocked press keeps the up swallowed");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, false);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    {
        uint64_t t = 109000000ULL;
        MouseEvent down = {0};
        down.button = MOUSE_BUTTON_LEFT;
        down.timestamp = t;
        down.is_down = true;
        down.x = 10;
        down.y = 10;
        debounce_process_event(&manager, &down);

        MouseEvent up = down;
        up.is_down = false;
        up.timestamp = t + 60000;
        debounce_process_event(&manager, &up);

        MouseEvent bounce = down;
        bounce.timestamp = t + 80000;
        CHECK(debounce_process_event(&manager, &bounce), "Bounce DOWN blocked");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_BLOCKED,
              "State BLOCKED before reset");

        debounce_reset_statistics(&manager);
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_BLOCKED,
              "Reset keeps BLOCKED until the up arrives");
        CHECK(debounce_get_button_blocks(&manager, MOUSE_BUTTON_LEFT) == 0,
              "Counters still cleared");

        MouseEvent bounce_up = down;
        bounce_up.is_down = false;
        bounce_up.timestamp = t + 140000;
        CHECK(debounce_process_event(&manager, &bounce_up),
              "Pending UP still swallowed");
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "State IDLE after the up");
    }

    /* Test 39: the deferred-release timer flushes every expired button in one
       batch and leaves one still inside the window alone. */
    TEST("Deferred releases flush several expired buttons in one batch");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    {
        uint64_t now = debounce_get_timestamp(&manager);
        manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
        manager.buttons[MOUSE_BUTTON_LEFT].confirmStartTime = now - 151000;
        manager.buttons[MOUSE_BUTTON_RIGHT].state = BTN_STATE_CONFIRMING;
        manager.buttons[MOUSE_BUTTON_RIGHT].confirmStartTime = now - 160000;
        manager.buttons[MOUSE_BUTTON_MIDDLE].state = BTN_STATE_CONFIRMING;
        manager.buttons[MOUSE_BUTTON_MIDDLE].confirmStartTime = now;

        debounce_check_deferred_releases(&manager);
        CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE,
              "Expired left released");
        CHECK(manager.buttons[MOUSE_BUTTON_RIGHT].state == BTN_STATE_IDLE,
              "Expired right released");
        CHECK(manager.buttons[MOUSE_BUTTON_MIDDLE].state == BTN_STATE_CONFIRMING,
              "Unexpired middle still held");
        CHECK(manager.inject_count == 2, "Two releases queued");
        if (manager.inject_count == 2)
        {
            CHECK(manager.inject_queue[0].input.mi.dwFlags == MOUSEEVENTF_LEFTUP &&
                  manager.inject_queue[1].input.mi.dwFlags == MOUSEEVENTF_RIGHTUP,
                  "LEFTUP then RIGHTUP");
        }
    }

    /* Test 40: the Smart Drag migration decision. */
    TEST("Smart Drag migration combinations");
    CHECK(settings_resolve_smart_drag(true, true, true, false, false),
          "Per-button 1 wins over a legacy 0");
    CHECK(!settings_resolve_smart_drag(true, false, true, true, true),
          "Per-button 0 wins over a legacy 1");
    CHECK(settings_resolve_smart_drag(false, false, true, true, true),
          "Legacy 1 turns the left button on");
    CHECK(!settings_resolve_smart_drag(false, false, true, true, false),
          "Legacy 1 leaves the other buttons off");
    CHECK(!settings_resolve_smart_drag(false, false, true, false, true),
          "Legacy 0 turns the left button off");
    CHECK(!settings_resolve_smart_drag(false, false, true, false, false),
          "Legacy 0 leaves the other buttons off");
    CHECK(settings_resolve_smart_drag(false, false, false, false, true),
          "Fresh install keeps the left button on");
    CHECK(!settings_resolve_smart_drag(false, false, false, false, false),
          "Fresh install keeps the other buttons off");

    /* Test 40: the 5px drag threshold is a boundary, not an approximate hint. */
    TEST("Drag distance boundary: 4px passes, 6px triggers");
    debounce_reset_statistics(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);

    MouseEvent down40a = {0};
    down40a.button = MOUSE_BUTTON_LEFT;
    down40a.timestamp = 24000000;
    down40a.is_down = true;
    down40a.x = 100;
    down40a.y = 100;
    CHECK(!debounce_process_event(&manager, &down40a), "First DOWN passes");

    MouseEvent up40a = down40a;
    up40a.is_down = false;
    up40a.timestamp = 24050000;
    up40a.x = 104;                 /* 4px: below the 5px boundary */
    CHECK(!debounce_process_event(&manager, &up40a), "4px release passes");

    MouseEvent down40b = down40a;
    down40b.timestamp = 24200000;  /* 150ms after the first release */
    CHECK(!debounce_process_event(&manager, &down40b), "Second DOWN passes");

    MouseEvent up40b = down40b;
    up40b.is_down = false;
    up40b.timestamp = 24150000;
    up40b.x = 106;                 /* 6px: above the 5px boundary */
    CHECK(debounce_process_event(&manager, &up40b), "6px release held");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING,
          "6px enters CONFIRMING");
    debounce_reset_statistics(&manager);

    /* Test 41: a bounce 60ms after the release must still be inside the
       150ms window. This is the regression that 50ms caused in 1.0.4. */
    TEST("60ms bounce stays inside the 150ms confirm window");
    debounce_reset_statistics(&manager);
    clear_queue(&manager);
    debounce_set_smart_drag(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);

    MouseEvent down41 = {0};
    down41.button = MOUSE_BUTTON_LEFT;
    down41.timestamp = 25000000;
    down41.is_down = true;
    down41.x = 100;
    down41.y = 100;
    CHECK(!debounce_process_event(&manager, &down41), "Drag DOWN passes");

    MouseEvent up41 = down41;
    up41.is_down = false;
    up41.timestamp = 25300000;    /* 300ms hold */
    up41.x = 120;                 /* 20px move */
    CHECK(debounce_process_event(&manager, &up41), "Drag release held");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING,
          "State CONFIRMING");

    uint64_t now41 = debounce_get_timestamp(&manager);
    manager.buttons[MOUSE_BUTTON_LEFT].confirmStartTime = now41 - 60000; /* 60ms ago */
    debounce_check_deferred_releases(&manager);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING,
          "Still held at 60ms");
    CHECK(manager.inject_count == 0, "No release queued at 60ms");

    MouseEvent bounce41 = {0};
    bounce41.button = MOUSE_BUTTON_LEFT;
    bounce41.timestamp = now41;
    bounce41.is_down = true;
    bounce41.x = 120;
    bounce41.y = 100;
    CHECK(debounce_process_event(&manager, &bounce41), "Bounce DOWN swallowed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED,
          "Back to PRESSED, drag kept");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == down41.timestamp,
          "Original downTime kept");
    debounce_reset_statistics(&manager);

    /* Summary */
    printf("\n================================================\n");
    printf("Result: %d/%d checks passed in %d tests", pass_count, check_count, test_count);
    if (fail_count > 0)
        printf(" (%d failed)", fail_count);
    printf("\n================================================\n");

    debounce_cleanup(&manager);
    return fail_count > 0 ? 1 : 0;
}
