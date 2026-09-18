# Beginner experience — adapted phase plan

Adapted from `Magda_Cursor_Auto_Implementation_Plan.md` to this SUNROOM/MAGDA checkout.

## Current phase status

| Phase | Status | Notes |
|---|---|---|
| P0 Inspect & map | **complete** (2026-09-17) | See `inventory.md`, `progress.md` |
| P1 Edits, undo, recovery | **complete** (2026-09-17) | `verify_beginner_p1.py` PASS; GUI recovery prompt still pending |
| P2 Offline starter (Fixture A) | **complete** (2026-09-17) | `fixture-a` + `verify_beginner_p2.py` PASS |
| P3 First-session flow | **complete** (2026-09-17) | Beat/Song/Blank + Create and Play; GUI audible pending |
| P4 Instruments / piano roll / QWERTY | **complete** (2026-09-17) | Guided entry + scale-lock + QWERTY focus; GUI audible pending |
| P5 Arrangement ownership | **complete** (2026-09-17) | Fixture B/C + Place Scene conflict refuse; Capture Jam/Return/badges code paths; GUI audible pending |
| P6 Progressive Mix | **complete** (2026-09-17) | Open Mix + Shared Space return reuse; Analyze unchanged; GUI audible pending |
| P7 Conductor / safe apply | **complete** (2026-09-17) | Staged DSL proposal; mocks through real interpreter; live model is P8 |
| P8 Local AI coach | **complete** (2026-09-17) | Absent model labeled honestly; SUNROOM_DSL stages through P7; live weights not loaded |
| P9 Curated media | **complete** (2026-09-18) | Offline starter catalog filters + import; unknown BPM/key not guessed; GUI audition not run |
| P10 Export Song | **next** | |
| P11 E2E verification | pending | |
| P12 Release handoff | pending | |

## Architecture reuse (non-negotiable)

- Same project entities for guided + Full studio (`TrackManager`, `ClipManager`, `UndoManager`).
- No parallel beginner project model; no audio-callback I/O or inference.
- Chord Track ≠ Chord Engine; Drum Grid is a device, not a new sequencer rewrite.
- AI mutations must go through validated DSL/command path (P7 before claiming coach apply).

## SUNROOM overlap

Prior SUNROOM work already delivers a guided Create path, 8-bar default, coach, and offline journey generation. P2–P3 must **reconcile** Magda Fixture A (drums/bass/chords @ 100 BPM) with SUNROOM moods without inventing a fourth composition model. Prefer additive entry (Beat/Song/Blank) that can call either recipe pipeline once both exist.
