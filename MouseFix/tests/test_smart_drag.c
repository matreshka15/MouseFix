#include <stdio.h>
#include <assert.h>
#include "../src/core/debouncer.h"

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST(name) \
    do { printf("\n[TEST] %s\n", name); test_count++; } while(0)

#define CHECK(condition, msg) \
    do { \
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
    down1.timestamp = 1000;
    down1.is_down = true;
    down1.x = 100;
    down1.y = 100;
    CHECK(!debounce_process_event(&manager, &down1), "DOWN passed");

    MouseEvent up1 = {0};
    up1.button = MOUSE_BUTTON_LEFT;
    up1.timestamp = 1060;
    up1.is_down = false;
    up1.x = 100;
    up1.y = 100;
    CHECK(!debounce_process_event(&manager, &up1), "UP passed");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_IDLE, "State back to IDLE");

    /* Test 2: Fast double-click bounce */
    TEST("Fast double-click - bounce should be blocked");
    debounce_reset_statistics(&manager);

    MouseEvent down2a = {0};
    down2a.button = MOUSE_BUTTON_LEFT;
    down2a.timestamp = 2000;
    down2a.is_down = true;
    down2a.x = 100;
    down2a.y = 100;
    debounce_process_event(&manager, &down2a);

    MouseEvent up2a = {0};
    up2a.button = MOUSE_BUTTON_LEFT;
    up2a.timestamp = 2060;
    up2a.is_down = false;
    up2a.x = 100;
    up2a.y = 100;
    debounce_process_event(&manager, &up2a);

    MouseEvent down2b = {0};
    down2b.button = MOUSE_BUTTON_LEFT;
    down2b.timestamp = 2080;
    down2b.is_down = true;
    down2b.x = 100;
    down2b.y = 100;
    CHECK(debounce_process_event(&manager, &down2b), "Bounce DOWN blocked");

    MouseEvent up2b = {0};
    up2b.button = MOUSE_BUTTON_LEFT;
    up2b.timestamp = 2140;
    up2b.is_down = false;
    up2b.x = 100;
    up2b.y = 100;
    CHECK(debounce_process_event(&manager, &up2b), "Bounce UP blocked");

    /* Test 3: Long press - Smart Drag */
    TEST("Long press no move - Smart Drag should trigger");
    debounce_reset_statistics(&manager);

    MouseEvent down3 = {0};
    down3.button = MOUSE_BUTTON_LEFT;
    down3.timestamp = 3000;
    down3.is_down = true;
    down3.x = 100;
    down3.y = 100;
    debounce_process_event(&manager, &down3);

    MouseEvent up3 = {0};
    up3.button = MOUSE_BUTTON_LEFT;
    up3.timestamp = 3300;
    up3.is_down = false;
    up3.x = 100;
    up3.y = 100;
    CHECK(debounce_process_event(&manager, &up3), "UP blocked (holdTime=300)");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "State is CONFIRMING");

    /* Test 4: Move drag - Smart Drag */
    TEST("Press and move - Smart Drag should trigger");
    debounce_reset_statistics(&manager);

    MouseEvent down4 = {0};
    down4.button = MOUSE_BUTTON_LEFT;
    down4.timestamp = 4000;
    down4.is_down = true;
    down4.x = 100;
    down4.y = 100;
    debounce_process_event(&manager, &down4);

    MouseEvent up4 = {0};
    up4.button = MOUSE_BUTTON_LEFT;
    up4.timestamp = 4100;
    up4.is_down = false;
    up4.x = 110;
    up4.y = 100;
    CHECK(debounce_process_event(&manager, &up4), "UP blocked (dist=10px)");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "State is CONFIRMING");

    /* Test 5: Bounce down cancels confirm */
    TEST("Bounce down during confirm - should cancel");
    debounce_reset_statistics(&manager);

    MouseEvent down5 = {0};
    down5.button = MOUSE_BUTTON_LEFT;
    down5.timestamp = 5000;
    down5.is_down = true;
    down5.x = 100;
    down5.y = 100;
    debounce_process_event(&manager, &down5);

    MouseEvent up5 = {0};
    up5.button = MOUSE_BUTTON_LEFT;
    up5.timestamp = 5300;
    up5.is_down = false;
    up5.x = 200;
    up5.y = 200;
    debounce_process_event(&manager, &up5);
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_CONFIRMING, "Entered CONFIRMING");

    MouseEvent down5b = {0};
    down5b.button = MOUSE_BUTTON_LEFT;
    down5b.timestamp = 5350;
    down5b.is_down = true;
    down5b.x = 200;
    down5b.y = 200;
    CHECK(debounce_process_event(&manager, &down5b), "Bounce DOWN blocked");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].state == BTN_STATE_DRAGGING, "Back to DRAGGING");
    CHECK(manager.buttons[MOUSE_BUTTON_LEFT].downTime == 5350, "downTime updated");

    /* Summary */
    printf("\n================================================\n");
    printf("Result: %d/%d passed", pass_count, test_count);
    if (fail_count > 0)
        printf(" (%d failed)", fail_count);
    printf("\n================================================\n");

    debounce_cleanup(&manager);
    return fail_count > 0 ? 1 : 0;
}
