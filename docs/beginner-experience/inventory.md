# Beginner experience — repository inventory (P0)

**Repo:** SUNROOM fork of MAGDA (`logan123385/magda-core`, branch `codex/sunroom`)  
**Baseline revision inspected:** `92edd84` (+ uncommitted beginner first-session edits in `magda/daw/sunroom/*`)  
**Date:** 2026-09-17  
**Method:** static path/symbol inspection + headless baseline commands. GUI app launch **not run** (Xcode license / sandbox).

Source scope: `Magda_DAW_Beginner_Feature_Gaps.pdf` (M1–M5, S1–S6). Execution: `Magda_Cursor_Auto_Implementation_Plan.md`, `Magda_Cursor_Auto_Phase_Prompts.md`.

---

## Capability status (M/S map)

| ID | Requirement | Status | Evidence / notes |
|---|---|---|---|
| M1 | Guided first session | **needs extension** | `SunroomStudio` is default guided overlay (`MainWindow` `guidedStudio_ = true`). Feeling → Build → Play path exists; Beat/Song/Blank welcome picker and Fixture A drum/bass/chords starter **missing**. |
| M2 | Auditable agent coach | **present, needs extension** | DSL + specialists + SUNROOM MLX/Luna coach. Preview/apply/undo for coach mutations and before/after A/B need productization (P7–P8). |
| M3 | Session → Arrangement | **working/reusable** | `SessionView`, `MainView` (arrange), `SessionClipScheduler`, `TrackPlaybackMode`, `SessionRecorder`. Beginner Capture Jam / source badges need wiring (P5). |
| M4 | Beginner instruments | **working/reusable** | Guided Make a Beat / Add Chords / Play a Sound open Fixture A Drum Grid / Chords / Bass (P4). Chord Track ≠ Chord Engine. |
| M5 | Progressive Mix | **needs extension** | `MixerView` exists; guided collapsed presentation + shared spatial return macros not productized (P6). |
| S1 | Curated packs / filters | **needs extension** | Media DB + SUNROOM Sounds exist; curated packs + key/BPM filter UX incomplete (P9). |
| S2 | Scene / section templates | **needs extension** | Session scenes exist; Beat/Song/Blank landed (P3); intro/main/variation/ending templates incomplete (P5). |
| S3 | Simple piano roll | **working/reusable** | Scale lock + fold-as-simple for guided projects (P4); shares clip data with advanced view. |
| S4 | Export, autosave, undo | **working/reusable** | `UndoManager`, `ProjectManager` autosave (`.autosave`), export/`OfflineRenderHelper`. Recovery UX and Export Song prominence need verification/extension (P1/P10). |
| S5 | QWERTY + simple FX | **needs extension** | QWERTY text-focus skip + focus-loss flush (P4); shared spatial return macros still P6. |
| S6 | Graduation coach / conductor | **needs extension** | Specialists + Sunroom coach exist; one coherent conductor conversation (P7–P8). |

Nice items N1–N6: **deferred**.

---

## System map (real symbols)

### Guided / first-run
- `magda/daw/sunroom/SunroomStudio.{hpp,cpp}` — Create / Note garden / Sound shelf / AI companion
- `magda/daw/sunroom/SunroomActions.{hpp,cpp}` — `CreateJourneyCommand`, `AddMelodyCommand`, …
- `magda/daw/sunroom/MusicTheory.hpp` — moods, compose, Options (default 8 bars)
- `magda/daw/ui/dialogs/SplashScreen.*`, `ProjectManager::newProject()`

### Session / Arrangement / Mix
- `magda/daw/ui/views/SessionView.*`
- `magda/daw/ui/views/MainView.*` (arrangement; `ArrangementView` is not the live UI)
- `magda/daw/ui/views/MixerView.*`
- `magda/daw/core/ViewModeController.hpp` — `ViewMode::{Live,Arrange,Mix,Master}`

### Tracks / clips / transport arbitration
- `magda/daw/core/TrackManager.*`, `TrackInfo.hpp` (`TrackPlaybackMode::{Arrangement,Session}`)
- `magda/daw/core/ClipManager.*`, `ClipInfo.hpp`, `ClipTypes.hpp`
- `magda/daw/audio/session/SessionClipScheduler.*` — sync modes / revert to arrangement
- `magda/daw/audio/session/SessionRecorder.*` — session→arrangement capture

### Commands / persistence
- `magda/daw/core/UndoManager.*` — `UndoableCommand`, `CompoundCommand`
- `magda/daw/project/ProjectManager.*` — save/load/autosave/recovery prompt
- `magda/daw/project/serialization/ProjectSerializer.*`

### Devices / music tools
- Drum Grid: `magda/daw/audio/plugins/DrumGridPlugin.*`, `DrumGridUI`
- Chord Engine: `magda/daw/music/ChordEngine.*`, `MidiChordEnginePlugin`
- Chord Track: `TrackType::Chord`, `TrackManager::ensureChordTrack()`

### Export / media / agents / models
- Export: `MainWindowExport.cpp`, `OfflineRenderHelper.*`, CLI render
- Media: `MediaExplorerContent`, `MediaDbBrowserContent`, `paths::dataDir()`
- DSL: `magda/agents/dsl_interpreter.*`, `ConsoleAgentOrchestrator`
- Models: `llama_model_manager`, `sunroom_mlx_client` (MLX / LAN / Luna)

---

## Baseline commands (this environment)

| Check | Result |
|---|---|
| `./build-sunroom/tests/sunroom_theory_test` | **PASS** — 144 arrangements, 43728 notes |
| `python3 tests/sunroom/test_mlx_boundary.py` | **PASS** — 7 tests |
| `python3 scripts/verify_sunroom_native.py --cli …/magda_cli` | **PASS** — 7 tracks, 14 clips, 97 notes; peak −12.6 dBFS |
| Launch `SUNROOM.app` | **not run** — Xcode license / host GUI gate |
| `magda_tests` / `magda_juce_tests` full suite | **not run** — long rebuild; license risk |
| Live Luna / mini-PC | **not run** — needs owner key/server |

Build tree: `build-sunroom` (Ninja Release). Targets: `magda_daw_app`, `magda_cli`.

---

## Smallest P1 slice (justified)

Prove **one grouped undoable clip/journey insertion → save → reopen → undo** through existing infrastructure, using Fixture A timing (or current SUNROOM journey as stand-in until Fixture A lands in P2).

**Status (2026-09-17):** complete for headless path via `scripts/verify_beginner_p1.py` (14 clips; undo→0; redo/reopen/recover fingerprints match). GUI autosave prompt still pending.

**Touchpoints:**
1. `UndoManager::executeCommand` + `CompoundCommand` (or existing `CreateJourneyCommand` group)
2. `ProjectManager::saveProject` / load + dirty tracking
3. Failure seam: mid-command abort leaves prior project intact
4. Verification: `scripts/verify_beginner_p1.py` (dump-json musical fingerprint)

Do **not** build Beat/Song/Blank welcome UI in P1.
