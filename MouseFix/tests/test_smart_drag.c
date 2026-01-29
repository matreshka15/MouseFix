#include <stdio.h>
#include <assert.h>
#include "../src/core/debouncer.h"
#include "../src/core/time_manager.h"

// Helper to print results
void print_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

int main() {
    DebounceManager manager;
    // Enable monitoring for Left Button
    
    if (!debounce_init(&manager)) {
        printf("Failed to init manager\n");
        return 1;
    }
    
    // Simulate setting up the manager
    debounce_set_monitored(&manager, MOUSE_BUTTON_LEFT, true);
    debounce_set_threshold(&manager, MOUSE_BUTTON_LEFT, 50, 1, 200);
    debounce_set_hybrid_heuristic(&manager, true);

    printf("Starting Smart Drag Tests...\n");

    // ==========================================
    // Test 1: Normal Click (No Drag)
    // ==========================================
    // Down
    MouseEvent down1 = {0};
    down1.button = MOUSE_BUTTON_LEFT;
    down1.timestamp = 1000;
    down1.is_down = true;
    down1.x = 100;
    down1.y = 100;
    
    debounce_process_event(&manager, &down1);
    
    // Up (Short time 50ms, no move)
    // Note: If timestamp is exactly 50ms diff, it might be debounced if threshold is 50. 
    // Let's use 60ms to be safe from debounce logic, focusing on Smart Drag logic.
    MouseEvent up1 = {0};
    up1.button = MOUSE_BUTTON_LEFT;
    up1.timestamp = 1060; 
    up1.is_down = false;
    up1.x = 100;
    up1.y = 100;

    bool block_up1 = debounce_process_event(&manager, &up1);
    bool defer1 = manager.buttons[MOUSE_BUTTON_LEFT].isDeferringRelease;
    
    // Should NOT block (pass through) and NOT defer
    print_result("Normal Click - Should Not Block", !block_up1);
    print_result("Normal Click - Should Not Defer", !defer1);

    // ==========================================
    // Test 2: Long Hold Drag (> 200ms)
    // ==========================================
    // Down
    MouseEvent down2 = {0};
    down2.button = MOUSE_BUTTON_LEFT;
    down2.timestamp = 2000;
    down2.is_down = true;
    down2.x = 100;
    down2.y = 100;
    
    debounce_process_event(&manager, &down2);

    // Up (Long time 300ms > 200ms)
    MouseEvent up2 = {0};
    up2.button = MOUSE_BUTTON_LEFT;
    up2.timestamp = 2300; 
    up2.is_down = false;
    up2.x = 100;
    up2.y = 100;

    bool block_up2 = debounce_process_event(&manager, &up2);
    bool defer2 = manager.buttons[MOUSE_BUTTON_LEFT].isDeferringRelease;

    // Should Block (swallow original up) and Defer (inject later)
    print_result("Long Hold Drag - Should Block", block_up2);
    print_result("Long Hold Drag - Should Defer", defer2);

    // Manually clear state for next test
    manager.buttons[MOUSE_BUTTON_LEFT].isDeferringRelease = false;

    // ==========================================
    // Test 3: Move Drag (> 5px distance)
    // ==========================================
    // Down
    MouseEvent down3 = {0};
    down3.button = MOUSE_BUTTON_LEFT;
    down3.timestamp = 3000;
    down3.is_down = true;
    down3.x = 100;
    down3.y = 100;
    
    debounce_process_event(&manager, &down3);

    // Up (Short time, but moved 10px)
    MouseEvent up3 = {0};
    up3.button = MOUSE_BUTTON_LEFT;
    up3.timestamp = 3060; 
    up3.is_down = false;
    up3.x = 110; // moved 10px (sq dist = 100 > 25)
    up3.y = 100;

    bool block_up3 = debounce_process_event(&manager, &up3);
    bool defer3 = manager.buttons[MOUSE_BUTTON_LEFT].isDeferringRelease;

    // Should Block and Defer
    print_result("Move Drag - Should Block", block_up3);
    print_result("Move Drag - Should Defer", defer3);

    debounce_cleanup(&manager);
    return 0;
}
