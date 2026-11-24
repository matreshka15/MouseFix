#pragma once

#include <stdbool.h>

/* Resolve one button's Smart Drag value while loading settings.
 *
 * 1.0.9 replaced the single global "HybridHeuristic" switch with a
 * "Btn%d_SmartDrag" value per button. The old build only ever turned Smart
 * Drag on for the left button, so a legacy 1 migrates to left on and every
 * other button off. A per-button value always wins, which keeps the migration
 * idempotent: once 1.0.9 has saved, the legacy value is ignored even if it is
 * still in the registry.
 *
 * Kept as a pure function so the combinations can be unit tested without
 * touching the registry. */
static inline bool settings_resolve_smart_drag(bool has_per_button,
                                               bool per_button_value,
                                               bool legacy_present,
                                               bool legacy_enabled,
                                               bool is_left_button)
{
    if (has_per_button)
        return per_button_value;
    if (legacy_present)
        return legacy_enabled && is_left_button;
    return is_left_button; /* fresh install: left on, every other button off */
}
