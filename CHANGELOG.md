# Changelog

## 1.0.9 — 2026-11-21

- Touchpad taps are no longer swallowed. A duplicate button-down arriving before its matching up used to move the button into a blocked state, so the real release was dropped.
- Smart Drag is per button now, left button only by default. Right-button debounce can stay on with right Smart Drag off, which fixes the MouseInc gesture focus problem (#4).
- The old global Smart Drag setting migrates per button: on becomes left-only, off becomes all-off, with the new values written before the old one is deleted.
- Duplicate downs no longer count twice in the blocked counter, and Reset no longer throws away a pending release.
- The release timer is no longer pushed back by every mouse event; the version string lives in `version_defs.h`.

## 1.0.4 — 2026-07-13

Last release with prebuilt binaries attached (x64 and x86). Those still run; they just stop at 1.0.4.

Earlier versions: v1.0.0 to v1.0.3 on the [Releases page](https://github.com/matreshka15/MouseFix/releases).
