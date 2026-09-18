# Beginner experience — decisions

| Date | Decision | Reason |
|---|---|---|
| 2026-09-17 | Store Magda Auto plan/prompts/PDF under `docs/beginner-experience/` | Plan requires durable docs in-repo; matches existing `docs/` layout |
| 2026-09-17 | Treat `SunroomStudio` as existing guided first-run surface, not a blank slate | Already default `guidedStudio_`; P3 extends rather than replaces |
| 2026-09-17 | P2 Fixture A remains drums/bass/chords @ 100 BPM A minor | Source Magda beginner plan; SUNROOM journey is parallel offline recipe, not a substitute for Fixture A acceptance |
| 2026-09-17 | Arrangement UI is `MainView`, not `ArrangementView` | `ArrangementView` has no live `.cpp` implementation in this checkout |
| 2026-09-17 | P1 before welcome UI | Plan requires proving undo/save/reopen before onboarding screens |
| 2026-09-17 | GUI launch marked not run | Xcode license / sandbox block; headless CLI evidence retained |
| 2026-09-17 | P1 reopen identity via dump-json fingerprint, not .mgd bytes | Gzip/project-name rewrite makes byte digests unstable; musical tracks/clips/notes are the acceptance signal |
| 2026-09-17 | Refuse unwritable save targets before TemporaryFile replace | Atomic rename can replace a mode-0444 file when the directory is writable |
| 2026-09-17 | Headless CLI `_Exit` after successful save | Avoids known JUCE/Tracktion destructor SIGSEGV after durable write; tracked as open teardown gate |
| 2026-09-17 | Fixture A is a new `CreateFixtureACommand`, not an extension of SUNROOM journey | Journey moods ≠ Magda drums/bass/chords acceptance; keep both offline recipes |
| 2026-09-17 | Drum Grid pad notes 24/25/26 for kick/snare/hat | `DrumGridPlugin::baseNote` + pad index; not General MIDI |
| 2026-09-17 | Beat/Song both call Fixture A in P3; Song sections deferred to P5 | Same music objects now; arrangement templates are P5 scope |
| 2026-09-17 | Create and Play is idempotent when Fixture A tracks exist | Prevents double-insert on rapid clicks / Play again |
| 2026-09-17 | Scale lock snaps new pitched notes only; ties prefer upward | Deterministic rule for P4; existing notes unchanged |
| 2026-09-17 | Fixture A sets sunroomMood=1 (natural minor) with keyRoot A | Aligns guide colours / scale lock with Fixture A harmony |
| 2026-09-17 | Make a Beat / Add Chords / Play a Sound open Fixture A clips | Reuse Drum Grid + piano roll + polysynth; no new sequencer |
| 2026-09-17 | Place Scene refuses occupied Arrangement ranges; no silent overwrite | Magda P5 ownership; PreserveExisting would shift — explicit fail instead |
| 2026-09-17 | Copy `failureReason` before `UndoManager::clearHistory` | clearHistory destroys the command; reading raw after is UAF (SIGSEGV) |
| 2026-09-17 | ASCII-only juce::String literals from `const char*` in fail paths | JUCE_LOG_ASSERTIONS + CharPointer_ASCII rejects em-dash bytes |
| 2026-09-17 | Capture Jam / Return reuse SessionRecorder + deactivateAllSessionClips | No parallel capture stack for beginners |
| 2026-09-17 | Shared Space is one Aux+magda_reverb; beginner control is send level | Distinct from journey insert Space; reuse by device identity |
| 2026-09-17 | Open Mix leaves guided overlay; does not rewrite mixerShow* Config | Advanced users keep rail preferences; defaults already collapsed |
| 2026-09-17 | P7 stages DSL then applies through Interpreter in one undo compound | Console still applies immediately; live model is P8; model text is not an MCP grant |
| 2026-09-17 | Coach project changes require a SUNROOM_DSL line and Apply | Prose and missing model do not mutate; Hear before/after is undo/redo of that one step only |
| 2026-09-18 | Starter shelf reads manifest.json only; unknown BPM/key are counted, not guessed | Drums stay unpitched; Media Library semantic search is not replaced; import undo does not delete WAVs |


