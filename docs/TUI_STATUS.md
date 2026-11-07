# TUI status

Superseded by `docs/TUI.md` (design + usage + limitations).

Done: core (tree, flex layout, damage tracking, diff renderer, events, focus, lifecycle hooks),
widgets (hr/vr/spacer, button, input, checkbox, progress, slider, scroll), term_event adapter,
fuzz-tested core, full port of the player UI (src/ui, see docs/UI.md).

Left: Windows build check; CMake check with
deps available; value-based layout tests (deliberately skipped); the limitations listed in TUI.md.
