#include <stdio.h>
#include <assert.h>
#include "../src/core/debouncer.h"

static int test_count = 0;
static int check_count = 0;
static int pass_count = 0;
static int fail_count = 0;

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
    debounce_set_hybrid_heuristic(&manager, true);

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

    /* Test 3: Long press no move - Smart Drag (200ms hold) */
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

    /* Test 4: Move drag - Smart Drag */
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
    up4.timestamp = 4100000;      /* +100ms, moved 10px */
    up4.is_down = false;
    up4.x = 110;
    up4.y = 100;
    CHECK(debounce_process_event(&manager, &up4), "UP blocked (dist=10px)");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "State is CONFIRMING");

    /* Test 5: New click right after a drag release must not be swallowed.
       The physical press is blocked and replayed as release + press. */
    TEST("New press during confirm - press replayed as release + press");
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
    debounce_process_event(&manager, &up5);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "Entered CONFIRMING");

    MouseEvent down5b = {0};
    down5b.button = MOUSE_BUTTON_LEFT;
    down5b.timestamp = 5350000;
    down5b.is_down = true;
    down5b.x = 200;
    down5b.y = 200;
    CHECK(debounce_process_event(&manager, &down5b), "Physical DOWN blocked for replay");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_PRESSED, "State is PRESSED");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == 5350000, "downTime updated");

    MouseEvent up5b = {0};
    up5b.button = MOUSE_BUTTON_LEFT;
    up5b.timestamp = 5400000;
    up5b.is_down = false;
    up5b.x = 200;
    up5b.y = 200;
    CHECK(!debounce_process_event(&manager, &up5b), "Short click UP passes");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "State back to IDLE");

    /* Test 6: Confirm window - releases after 150ms, not before */
    TEST("Confirm window - release on time");
    debounce_reset_statistics(&manager);

    /* Past the 150ms window: should release */
    uint64_t now_us = debounce_get_timestamp(&manager);
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_LEFT].confirmStartTime = now_us - 160000; /* 160ms ago in us */
    debounce_check_deferred_releases(&manager);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "Released after 150ms window");

    /* Inside the 150ms window: should hold */
    now_us = debounce_get_timestamp(&manager);
    manager.buttons[MOUSE_BUTTON_LEFT].state = BTN_STATE_CONFIRMING;
    manager.buttons[MOUSE_BUTTON_LEFT].confirmStartTime = now_us - 100000; /* 100ms ago in us */
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

    /* Summary */
    printf("\n================================================\n");
    printf("Result: %d/%d checks passed in %d tests", pass_count, check_count, test_count);
    if (fail_count > 0)
        printf(" (%d failed)", fail_count);
    printf("\n================================================\n");

    debounce_cleanup(&manager);
    return fail_count > 0 ? 1 : 0;
}
