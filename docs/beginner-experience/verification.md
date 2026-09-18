# Beginner experience — verification

## P9 curated media gate (ran 2026-09-18)

Command:

```bash
python3 scripts/verify_beginner_p9.py
```

| Scenario | Status | Evidence |
|---|---|---|
| Kind filter | pass | kick returns 8 Heartbeat files, declared-no, bpm unknown |
| Combined filters / empty text | pass | kick+horizon and text zzz return 0 with zero unknown counts |
| Key filter | pass | unpitched skipped 40; pitched bells unknown-key 8; no guessed key |
| BPM filter | pass | unknown-bpm 48; no match invented |
| Missing / escaped import | pass | refused; nothing added |
| Import, reopen, undo, redo | pass | Heartbeat 01 track; source WAV still on disk |
| Audition / GUI | **not run** | shelf click still imports; no separate preview player |
| Time-stretch | not applied | query and import say so |

Evidence: `artifacts/beginner-p9/result.json`

## P8 coach gate (ran 2026-09-17)

Command:

```bash
python3 scripts/verify_beginner_p8.py
```

| Scenario | Status | Evidence |
|---|---|---|
| Missing local model | pass | "not installed" and "not an AI answer"; no API key text |
| Prose without SUNROOM_DSL | pass | refused; no save |
| SUNROOM_DSL then apply-proposal | pass | real interpreter; Applied |
| Live model / weights | **not run** | status check does not start the worker |
| GUI audible before/after | **not run** | Hear before/after is undo/redo of "Apply suggestion" only |

Evidence: `artifacts/beginner-p8/result.json`

## P7 staged DSL gate (ran 2026-09-17)

Command:

```bash
python3 scripts/verify_beginner_p7.py
```

| Scenario | Status | Evidence |
|---|---|---|
| Mock DSL groups tracks via real interpreter | pass | Applied; dump has All Tracks |
| One undo removes that group only | pass | same process; Drums remain |
| Invalid DSL | pass | Refused; no save |
| Stale revision | pass | bump-revision then refuse; no rollback |
| Selection change | pass | not retargeted |
| Cancel / second apply | pass | no duplicate save |
| Live model | **not run** | P8 |
| Console auto-apply | **unchanged** | still immediate |

Evidence: `artifacts/beginner-p7/result.json`

## Fixtures

- **Fixture A:** 8 bars @ 100 BPM 4/4, A natural minor; drums / bass / chords.
- **Fixture B:** 32-bar intro/main/variation/ending from A.
- **Fixture C:** distinct Session vs Arrangement playback sources.

## P6 Progressive Mix gate (ran 2026-09-17)

Command:

```bash
python3 scripts/verify_beginner_p6.py
```

| Scenario | Status | Evidence |
|---|---|---|
| Shared Space creates one Aux + magda_reverb | pass | dump devices on Shared Space |
| Starter tracks send at requested amount | pass | ≥3 sends @ 0.4 |
| Second apply reuses same Aux | pass | still one aux track |
| Undo removes created return | pass | space-return then undo in one exec |
| Open Mix | **code path** | `onShowMix` → `ViewMode::Mix` |
| Analyze reachable | **code path** | existing MixerToggleRail |
| GUI audible level/pan/FX | **not run** | Xcode license / app relink |

Evidence: `artifacts/beginner-p6/result.json`

## P5 Session→Arrangement gate (ran 2026-09-17)

Command:

```bash
python3 scripts/verify_beginner_p5.py
```

| Scenario | Status | Evidence |
|---|---|---|
| Fixture B 32-bar loop + section markers | pass | loopEndBeats ≥ 128; markers in .mgd |
| Place Scene empty range / occupied refuse | pass | Deterministic place; overwrite message |
| Fixture C dual sources | pass | Pulse + Pad |
| Capture Jam / Return / badges | **code path** | SessionRecorder / deactivateAllSessionClips / TrackPlaybackMode |
| GUI audible mixed Session+Arrangement | **not run** | Xcode license / app relink |

Evidence: `artifacts/beginner-p5/result.json`

## Earlier phases

P0–P4 evidence under `artifacts/beginner-p{0..4}/` and prior `verification.md` history.

## Manual GUI script (when unblocked)

1. Create and Play → Open Mix → faders / mute / solo; Analyze on mixer rail.
2. Shared Space with Space slider; confirm one Aux; undo.
3. Expand sends on mixer rail; confirm Shared Space send; collapse again.
