# Magda beginner experience: Cursor Auto implementation plan

Prepared for Logan Chambers • 16 September 2026

**Purpose:** implement the beginner experience proposed in `Magda_DAW_Beginner_Feature_Gaps.pdf` while preserving Magda's existing musical tools, project compatibility, and audio behavior.

**Recommended execution:** Cursor Agent with Auto selected, one phase per task. Attach this file and the source PDF. Begin with the kickoff prompt at the end. This is a repository-adaptive implementation specification, not a verified code audit: no repository or build was supplied with the request. All class names, APIs, file locations, DSL syntax, and platform capabilities must be established in Phase 0.

## 1. What we are building

A beginner should be able to open Magda, make a playable idea without a cloud account, change something themselves, turn Session clips into a short arrangement, balance the sound, save, reopen, and export it. AI should explain and expose its edits. Advanced tools should remain accessible.

The source report proposes a finished song of approximately 60–90 seconds as the product outcome. This is a target for the music's duration, **not** a claim that beginners will finish in 60–90 seconds. Usability targets below are new engineering proposals, not measured results or upstream roadmap commitments.

### Source fidelity rules

1. Preserve Session, Arrangement, and Mixer; hybrid tracks; the auditable DSL; view specialists; Drum Grid; Chord Engine; Mix Analyze; and media search.
2. The report identifies First Session as thin. Do not assume the checked-out revision already contains templates or a starter generator. Inspect it.
3. Drum Grid and Chord Engine are existing capabilities to productize. Chord Track is a separate project harmonic guide, not an instrument that makes sound.
4. Stem separation and audio/MIDI take recording and comping are reported existing capabilities. Preserve them; add discoverability later. Do not build replacements as beginner milestones. Verify actual platform support instead of copying the report's platform flags into code.
5. Progressive mixing is a new proposed default. Do not describe hidden sends as an already shipped behavior.
6. Keep the existing specialists, but expose one coherent coach conversation and serialize their project mutations.
7. Local AI should be available without a cloud account or paid key. Missing local models must never block making music. A deterministic starter is not an AI response and must not be labeled one.
8. Adapt general patterns using original UI. Do not copy competitor trademarks, distinctive chrome, or layouts. This plan makes no licensing or legal determination.
9. Do not claim research on AI learning proves a measured Magda learning effect. Test whether users can make an independent edit.

### Scope and traceability

| Report item | Implementation destination | Completion evidence |
|---|---|---|
| M1: guided first session | P2–P3 | Offline starter produces editable, audible Session clips with actual action history |
| M2: auditable agent coach | P1, P7–P8 | Preview, actual DSL/deltas, apply, undo, failure recovery, independent edit |
| M3: Session to Arrangement | P5 | Musical capture/commit, source indicators, Return to Arrangement, reopen |
| M4: beginner instruments | P2, P4 | Working Drum Grid, Chord Engine and Sampler entry points |
| M5: progressive Mix | P6 | Useful basic controls, persistent access to advanced controls and Analyze |
| S1: curated packs and filters | P2, P9 | Offline starter assets, metadata filters, safe audition cleanup |
| S2: scene and section templates | P3, P5 | Beat / Song / Blank; editable sections and duplication |
| S3: simple piano roll | P4 | Draw, snap, scale/key assistance without destructive conversion |
| S4: export, autosave, undo | P1, P10 | Recoverable edits, reopenable projects, verified playable export |
| S5: QWERTY MIDI and simple FX | P4, P6 | Focus-safe keyboard play; one shared spatial return and macro presets |
| S6: graduation coach and conductor | P5, P7–P8 | Source-aware guidance and coherent cross-view context |
| N1–N6, including N5b | Optional backlog | Explicitly deferred until the core release passes P11–P12 |

All Must and Strong items belong in the core plan. A small enabling subset of S1, S4 and S6 is deliberately implemented early because the Must items depend on it. This changes implementation order, not the source report's priorities.

## 2. Rules for Cursor Auto

### Work in small, reviewable changes

- Read applicable `AGENTS.md`, contributor instructions, build documentation and project conventions before editing. Preserve user changes and use an isolated feature branch or worktree when needed.
- Do not migrate frameworks, replace the audio engine, change project formats broadly, or upgrade dependencies as incidental cleanup.
- Use existing command, undo, serialization, settings, transport, device and UI systems. Do not create a parallel beginner project model or a second music engine.
- Implement one phase at a time. For a phase too large for one coherent change, split it into vertical slices with working behavior and evidence. Never mark a whole phase complete for finishing only its UI.
- Inspect the exact interfaces before using them. Conceptual contracts in this file are responsibilities, not instructions to invent similarly named frameworks.
- Keep normal low-risk implementation autonomous. Ask only when a missing product decision materially changes scope or risks existing work. Record ordinary assumptions and proceed.
- No unrelated publishing, purchases, external service setup, merges or destructive repository operations are included in this implementation plan.

### A control is finished only when its behavior works

Every new button must invoke a real domain action or expose an honest unavailable state with a useful next step. Do not ship timers that fake progress, canned agent replies presented as live AI, sample state presented as persisted projects, or an export success toast without a valid output file.

### Keep durable implementation notes

Use the repository's existing planning location, or create `docs/beginner-experience/` if none exists. Maintain:

- `inventory.md`: relevant files/symbols, capability status, build baseline, platform constraints.
- `plan.md`: this plan adapted to actual architecture, dependencies and phase status.
- `progress.md`: completed slices, remaining work, exact validation commands and results.
- `decisions.md`: meaningful tradeoffs and reasons.
- `verification.md`: repeatable end-to-end scenarios, fixtures and manual checks.

Do not generate verbose duplicate documents if equivalent repository records exist. Update these records at each phase boundary. A fresh Cursor chat must be able to continue without guessing from a conversation summary.

### Phase completion report

At each boundary report: user-visible behavior delivered; files/components changed; commands and outcomes; manual audio/UI checks performed; unresolved issues; next eligible phase. Say **not run** for checks that were not run. Distinguish pre-existing failures from new failures using the baseline.

If an unavailable OS, audio device or model prevents verification, finish safe implementation work and label that gate pending. Do not substitute a mock result for real audio or real model validation.

## 3. Architecture and correctness contracts

These apply to all phases. Adapt to the repository's implementation language and architecture.

### One authoritative project and command path

Starter actions, manual actions, and coach actions must operate on the same project entities and existing command infrastructure. Stable track, clip, device and asset identifiers should connect UI, actions and serialized state. View-local state must not become the canonical music data.

Beginner layout preferences generally belong in user settings. Music and musical edits belong in the project. Determine from existing conventions whether key/scale is project-, editor-, or clip-level; never silently make it global. Use additive, versioned serialization changes only when necessary, with fixture coverage for older projects. Do not promise old versions can read new files unless tested.

### Real-time audio safety

Model inference, file access, sample decoding, metadata indexing, DSL parsing, expensive analysis and UI work must stay off the real-time audio callback. Reuse the engine's established queues and synchronization. Do not introduce blocking locks, network calls, heap-heavy work or synchronous logging in the callback.

Engine-affecting changes must follow existing scheduling and parameter-smoothing conventions. A starter must be ready before playback begins. Validate on the host's supported sample rates/buffer settings and compare with the baseline. No unsupported claim of zero glitches across all hardware.

### Transactions, history and retries

Group a user-intended operation such as inserting a starter or applying a coach suggestion into the existing equivalent of one undoable transaction. Prevalidate referenced assets and commands. Failed/canceled operations must not leave half a starter, dangling devices or a broken routing graph.

Use the existing transaction/rollback mechanism; do not assume undo alone safely rolls back every asynchronous side effect. If atomic application does not exist, implement the smallest required staging/compensation mechanism and test failure at intermediate steps. Cleanup may delete only assets created and owned by that failed operation.

Disable duplicate submission while pending and use operation identity where retries can occur. A deliberate second insertion remains possible; retrying the same request must not duplicate it. Do not replay starter commands on project load.

### Source of sound

Viewing Arrangement must not itself change what is playing. Inspect the engine's real Session/Arrangement arbitration rules. The UI must describe them truthfully per track where applicable, including mixed-source states. If no equivalent of returning a track to Arrangement exists, treat that as an explicit engine task before exposing the control.

### AI mutation boundary

The model proposes structured actions; existing validated commands perform changes. Show actual generated/validated DSL and actual applied deltas. Never display an illustrative snippet as the executed command history.

Validate schemas, permitted operations, entity existence, parameter ranges, applicable project revision and asset access before applying. Treat track names, clip names, imported metadata and retrieved text as data, not instructions. Do not widen MCP permissions or execute shell instructions to satisfy a model response.

Keep one active project writer. Existing view specialists may advise or analyze, but must not concurrently mutate the project. Model inference itself must not hold a long-lived lock that prevents manual editing. Check relevant state again at apply time; reject or replan stale proposals without losing intervening user edits.

## 4. Phase P0 — Establish the real codebase

**Goal:** replace assumptions with a concrete implementation map.

1. Read repository instructions; identify language, UI framework, build tools, supported OS targets, package/project formats and test runners.
2. Record current branch/revision and working tree status. Identify existing modifications and avoid overwriting them.
3. Locate the welcome/new-project flow; Session, Arrangement and Mixer; hybrid-track/clip models; transport and source arbitration; command/undo system; project save/load/autosave; export; device factories; media browser; DSL parser/executor; agent routing; local model integration; settings and accessibility conventions.
4. Search for relevant existing implementations before designing replacements. Use repository-wide text search, then inspect callers and tests, not just symbol definitions.
5. Build and run the normal baseline checks. Launch the app if the environment supports it. Record actual commands, results and environment limitations.
6. Classify every M/S capability as **working and reusable**, **present but needs extension**, **missing**, or **unverified**. Attach file/symbol evidence.
7. Inspect representative older project fixtures and media ownership conventions. Identify which changes would affect compatibility.
8. Produce a short phase map listing actual files to change, dependencies and risks. Do not fabricate exact estimates before this inventory.

**Done when:** the plan points to real code, baseline results are recorded, and the first vertical slice has a justified implementation path. If the report conflicts with the checkout, preserve its product intent, document the discrepancy, and adapt.

**Boundary:** no broad feature coding during discovery. Repair only a narrowly necessary baseline blocker and clearly separate that change.

## 5. Phase P1 — Establish safe edits and persistence

**Covers:** foundations for M1/M2 and the undo/autosave part of S4.

1. Trace one existing manual clip edit through command execution, undo/redo, dirty state, save/load and UI updates.
2. Reuse or extend this path for grouped starter insertion and grouped agent edits. Verify that a failed operation leaves the original project intact.
3. Inspect autosave/recovery behavior for named and unsaved projects. Extend only missing behavior required for the new flow.
4. Keep manual saves distinct from recovery snapshots. Recover into an explicitly identified recovery session/copy rather than silently overwriting a user's last good save.
5. Make writes atomic using existing platform/file abstractions where supported. Retain the previous valid file when a new save fails. Never clear the dirty indicator before a successful write.
6. Ensure assets referenced by a newly saved starter resolve after restart. Honor project-relative/managed asset conventions.
7. Expose actionable errors for unwritable destinations, missing assets, interrupted writes and corrupt recovery data. Do not suppress them behind a generic success message.

**Acceptance:** a grouped edit undoes/redoes coherently; failed application does not alter prior music; save/reopen retains clips/devices/settings; an interrupted recovery write preserves a prior valid recovery candidate. Recovery can be exercised in a controlled subprocess or existing harness without risking real projects.

**Suggested slice:** one small, deterministic clip insertion through the complete history/save/reopen path before adding onboarding UI.

## 6. Phase P2 — Starter content and recipe pipeline

**Covers:** enabling M1/M4, minimum S1/S2.

Start with a small offline recipe: drums, bass and chords over an eight-bar loop, or a simpler beat if the existing instruments require that first slice. Grow to the full three-part starter before declaring the phase complete. Keep each musical part editable.

### Implementation tasks

1. Inventory usable bundled instruments and assets. Select a small curated set through existing device factories. Do not add a plugin marketplace or mandatory third-party VST dependency.
2. Define the minimum recipe metadata needed: recipe ID/version, title, musical intent, tempo, meter, key/scale when meaningful, duration in bars, track/device/preset references, editable clip content and asset IDs. Reuse existing preset/template formats where possible.
3. Use existing musical timing units and conversion utilities. Do not confuse bars, beats, ticks, seconds and audio samples. Keep meter and tempo explicit.
4. Compile/load recipes through real project actions. If the DSL cannot express a required action, expose that gap and add a narrow real DSL capability or truthful audit representation. Do not invent DSL syntax in UI copy.
5. Preflight all devices and assets before mutating. Load expensive resources away from the audio thread; surface cancelable progress when needed.
6. Produce a human-readable result such as “Added drums, bass and chords in A minor at 100 BPM” using actual committed state. Retain exact commands in action details.
7. Store content provenance, attribution and redistribution evidence in the repository's asset manifest. Use existing cleared assets or original material whose distribution rights are established. Do not source starter packs from competitor installations.
8. Set musically useful conservative levels and check representative sums for clipping. Do not silently insert a limiter or normalize every track to hide poor gain choices.

**Acceptance:** insertion works offline without a model; clips remain editable; playback is non-silent and uses actual devices; one undo removes the inserted group; redo restores it; repeated clicks during loading do not double-insert; a missing asset yields a useful error with no partial project change; save/reopen retains the result.

**Test fixtures:** a deterministic seed/recipe, a missing asset, an invalid device reference and an intentionally failed mid-operation command. Use stable musical structure assertions; avoid bit-for-bit audio assertions for nondeterministic instruments.

## 7. Phase P3 — Guided first session

**Covers:** M1 and the welcome entry portion of S2.

### Proposed first-run flow

Open welcome → choose **Beat**, **Song**, or **Blank** → select a curated sound/recipe with sensible defaults → explicit **Create and Play** → Session appears with playable clips → show one suggested manual edit and a compact “What was added” summary.

- Beat is a short loop focused on rhythm and optional bass/chords.
- Song uses the same music objects and leads into editable sections in P5.
- Blank preserves the normal empty-project workflow.

Keep genre/key/tempo optional at first; use defaults without a mandatory setup questionnaire. Clearly label any preset genre/style examples as this app's own content.

### Implementation tasks

1. Reuse welcome/navigation components and settings. Show the guide on appropriate new/first-run paths, with skip and a discoverable way to reopen it. Existing projects should retain their normal reopening behavior.
2. After successful creation, route new guided projects to Session. Do not reset experienced users' saved layout preferences or force every opened project into Session.
3. Make Create and Play a single explicit action. Handle audio-device initialization separately from content creation. A failed output device must preserve the created music and offer retry/device setup.
4. Show real readiness, progress and errors. Prevent duplicate creation. Canceling should restore the pre-operation state.
5. Present one next action, such as changing a drum step, with a direct route to the relevant existing editor. Do not add a blocking tour as a dependency.
6. Keep Session, Arrangement, Mix and action/DSL details directly reachable. Friendly copy may explain DSL as “the commands behind this change,” while preserving access to its real representation.
7. Do not auto-open a cloud sign-in flow, require microphone permissions for MIDI starters, or demand model installation before playback.

**Acceptance:** clean local profile can reach audible editable music with no network or account; skipping works; restarting does not replay onboarding unexpectedly; returning users can reopen it; failed audio initialization is recoverable; keyboard navigation and visible focus reach all actions.

**Measure:** log or manually record steps and elapsed time to first sound, separating app cold start, device setup and recipe loading. Establish a target after measuring the baseline rather than inventing a performance guarantee.

## 8. Phase P4 — Beginner instruments, note editing and keyboard play

**Covers:** M4, S3 and QWERTY portion of S5.

### Drum Grid / Chord Engine / Sampler

1. Add obvious entry points such as **Make a beat**, **Add chords**, and **Play a sound**, routed to existing devices.
2. Surface a focused set of controls appropriate to each device. Preserve per-pad chains and advanced device editing behind an explicit expansion, using existing parameter hiding/macros where available.
3. Show selected kit/sound and meaningful defaults. Audition pads through the real device path.
4. Present Chord Engine as the sound/pattern-producing device flow and Chord Track as harmonic guidance. Verify where an instrument must receive generated MIDI; never imply the guide itself produces audio.
5. Do not rewrite arpeggiator, step sequencer or prompt-to-pattern engines that already work.

### Simple piano roll

1. Add or adapt a reversible editor presentation with draw, erase, selection, movement, note length, velocity and rhythmic snap readily available.
2. Use the existing note model; advanced and simple views edit the same clip.
3. Make selected key/scale visible. Choose and document a predictable rule for new-note snapping; test ties and chromatic boundaries.
4. Existing out-of-scale notes must remain intact and visible. Changing key/scale or enabling scale lock must not silently rewrite existing music. Offer a separate explicit, undoable transform only if in scope.
5. Distinguish rhythmic snap from scale lock. Do not apply pitched scale restrictions to drum lanes.
6. Keep expressive note data that the simple UI does not display. Toggling views must not strip it on save.

### QWERTY MIDI

1. Expose the existing computer-keyboard input mode, key map, octave and velocity controls where supported.
2. Resolve shortcut conflicts using existing command routing. Text fields, naming dialogs, search and the coach composer must not trigger notes.
3. Send appropriate note-off/all-notes-off on focus loss, mode disable, modal activation or track/device changes according to the engine's conventions.

**Acceptance:** edit the starter beat; play and record notes with the keyboard if recording is already supported; text entry makes no sound; lost focus produces no stuck notes; scale lock constrains new pitched notes without altering existing ones; switching simple/advanced views and save/reopen preserves musical data.

## 9. Phase P5 — Session to Arrangement and section templates

**Covers:** M3, remaining S2 and graduation portion of S6.

### Establish the engine behavior first

Document clip launch quantization, track playback ownership, transport/loop behavior, capture support, MIDI/audio representation, tempo automation and routing. Do not build a source badge from whichever tab is open.

### Implementation tasks

1. Reuse existing capture/record-to-arrangement capability when present. A **Capture Jam** action must capture actual performance timing; a **Place Scene in Arrangement** action may insert a deterministic scene block. Use distinct labels if both exist.
2. If capture is missing, implement and test its domain behavior before exposing Capture Jam. Merely copying currently visible clips is not performance capture.
3. Add a scoped commit destination/range using existing selection conventions. Default away from destructive overwrite; surface conflicts before replacing occupied regions. Commit remains undoable.
4. Add visible actual playback-source indicators, including mixed tracks. Make **Return to Arrangement** available when Session overrides apply. Give concise explanations, e.g. “This track is playing a Session clip; return it to the song timeline to hear its arrangement.”
5. Opening Arrangement alone should not change playback. A user invoking Return to Arrangement should get a real source change at the engine's documented scheduling boundary.
6. Support thin section templates using existing scenes/markers/regions: intro, main section, variation and ending. Prefer duplication/repetition of editable clips over rendered stereo content.
7. Derive duration from musical timing. Example only: in 4/4 at constant 100 BPM, 32 bars are 76.8 seconds before tails. Use the actual tempo map when it changes; do not hardcode this math into all songs.
8. Preserve required MIDI/audio edits and device/routing behavior. Do not discard automation silently; if the current capture path cannot preserve a feature, expose and resolve that limitation before claiming full capture support.
9. Add short contextual guidance for launch → capture/commit → return → edit a section, not a permanent overlay.

**Acceptance:** launch multiple clips, capture a short performance, return tracks to Arrangement and hear the expected song; different launches retain their timing; arrangement data remains editable; mixed-source state is accurate; undo removes only the new commit; save/reopen preserves the arrangement and the app's established playback-state semantics.

**Edge cases:** empty scenes, quantized launch pending at capture start/stop, stopping before a bar ends, loop wraparound, an occupied destination, changing tempo, muted/solo tracks, undo during or after capture. Use existing engine behavior as the contract and explain unsupported combinations honestly.

## 10. Phase P6 — Progressive Mix and simple effects

**Covers:** M5 and effects portion of S5.

1. Add a beginner mixer presentation with track labels, meters, faders, pan, mute and solo. Use existing controls and values.
2. Collapse sends, spectrum and advanced routing by default in the guided experience. Preserve existing advanced users' chosen layout and per-view preferences.
3. Keep **Analyze** visible and usable; keep advanced mixing one explicit action away. Changing presentation must never disconnect, reset or delete hidden processing.
4. Reuse measurement-grounded Mix Analyze and show its actual status. Do not call an unmeasured heuristic an audio analysis. Route proposed mixing actions through P7's validated application path when available.
5. Offer a small set of simple effect/macros using existing processors, sensible ranges and smoothing. Display the actual amount and allow reset/undo.
6. Add one optional shared spatial return through the existing routing graph. Reuse a suitable existing return instead of adding duplicates. One beginner control may drive the selected track's send, with the return processing configured appropriately for that architecture.
7. Keep shared FX optional and initially unobtrusive. Prevent feedback/self-routing; preserve manual routing edits; show the full routing through advanced view.
8. Explain whether a one-knob control changes wet/dry or send level. Do not falsely label multiple hidden parameter changes as a single measured quantity.

**Acceptance:** balance starter tracks and hear actual changes; hidden sends continue to work; switching presentations preserves all values; Analyze is reachable; turning on simple spatial FX creates/reuses one valid return; repeated use does not duplicate buses; undo and reopening preserve the routing.

**Check:** representative summed levels, mute/solo behavior, effects tails, CPU behavior against baseline and parameter changes during playback. Keep objective clipping/duration tests separate from subjective “sounds good” checks.

## 11. Phase P7 — Conductor and auditable action pipeline

**Covers:** enabling M2 and conductor portion of S6.

### One conversation, existing specialists

1. Inspect current agent/session context management and tool routing. Keep existing specialists; introduce the smallest coordination layer needed for one user-facing plan and coherent project context.
2. Capture project identity, relevant revision/state, selection, active view, pending request and recent applied actions. Follow existing privacy/persistence conventions; do not persist secrets in project files or action logs.
3. Switching views may change tools and selection context but must not erase the user's request, duplicate the conversation or spawn competing writers.
4. Add lifecycle states appropriate to existing infrastructure: idle, preparing/proposing, ready, applying, completed, canceled and failed. Ensure every pending state has a cancellation/error exit.
5. Validate actions against command schemas, permitted targets, capabilities and applicable parameter constraints. Reject invalid or unrelated actions before mutation.
6. At apply time, check whether the relevant project state still matches. Recompute or ask the user to review a refreshed proposal when stale. Never overwrite intervening manual edits to force the old plan through.
7. Group a successful apply into a coherent undo unit with actual before/after deltas and command details. Distinguish generated, rejected and executed commands.
8. Prevent duplicate apply and late-arriving responses from canceled requests. A response for a closed/switched project must never modify the new project.
9. Preserve read-only-until-grant MCP behavior. Tool grants belong to the existing permissions boundary, not to agent-generated text.

**Acceptance:** a mocked valid specialist response exercises the real validator/executor; invalid DSL causes no change; changing selection/project while thinking does not misdirect edits; cancellation/retry does not duplicate actions; one completed suggestion undoes cleanly; switching views retains conversation context.

Mocks validate the integration boundary here. They do not prove that a real model works; P8 must include that evidence separately.

## 12. Phase P8 — Beginner coach and local AI experience

**Covers:** M2 and completion of S6.

### Product behavior

Provide constrained suggestions whose current scope is obvious: “Make this drum pattern simpler,” “Add a small variation to this clip,” or “Explain what this mix measurement means.” Enable only suggestions supported by real existing specialist capabilities. The separate N6 expansion to new sound-design domains remains optional.

Each response should expose: what the coach intends to change; the affected musical objects; a plain-language explanation; real DSL/action details; and Apply/Cancel or the existing equivalent. Default project-changing suggestions to reviewable application for the beginner flow.

### Implementation tasks

1. Connect UI to P7 and existing local GGUF/llama.cpp support if verified in P0. Keep inference off the audio thread.
2. Detect readiness honestly: ready, no compatible model, loading, insufficient resources, failed or canceled. Provide a clear local setup path without a cloud account requirement.
3. Do not bundle a large model or introduce download/update infrastructure without first checking existing distribution strategy, compatibility, artifact verification, disk usage and cancellation support. Reuse existing facilities. Document required model/runtime setup in development instructions.
4. Without a ready model, retain offline starters and manual/static tips labeled as such. State that AI is unavailable until setup completes. This fallback keeps first sound usable; it does not satisfy the real-AI acceptance gate.
5. Add a real before/after audition using existing preview/snapshot/render infrastructure if available. Otherwise implement a scoped reversible audition that cannot overwrite concurrent edits, respects transport state, and keeps parameter smoothing. Do not use a decorative A/B toggle.
6. After applying an action, suggest one independent manual change. Acknowledge that an edit occurred; do not claim to have measured learning or musical quality.
7. Ground mix coaching in actual available measurements. Label subjective suggestions as suggestions and avoid promises that AI produces a hit song.

**Acceptance:** on a supported machine with a configured local model, a real suggestion passes validation, displays actual actions, applies audibly and undoes; a real failure has a useful message; no cloud account/key is needed; missing-model mode remains usable without pretending to be AI; before/after playback corresponds to real states; a user can then make a manual edit.

**Verification boundary:** test command semantics independently of nondeterministic model wording. Record model/runtime identity and test environment for the real integration check. If unavailable, mark the live-AI gate pending rather than complete.

## 13. Phase P9 — Curated media discovery

**Covers:** remaining S1.

1. Extend the existing Media Library; preserve semantic/similarity search. Add curated collection entry points and useful type/instrument, key and tempo filters.
2. Distinguish declared metadata, analyzed estimates and unknown values. Unknown key/BPM should not be guessed or silently excluded without visible filter behavior.
3. Use existing supported time stretching/transposition. Do not promise every loop matches any tempo/key. Show original metadata and any applied transformation; keep drum/unpitched assets out of inappropriate pitch rules.
4. Audition through the existing preview path. Import/drag into Session creates real editable clips through the normal command and asset-management path.
5. Keep temporary audition/cache assets distinct from imported project media. Cleanup only owned, unreferenced temporary data, accounting for undo/recovery references. Never delete a user's source file because its preview ended.
6. Keep the starter pack available offline. Larger catalogs remain optional; do not add accounts or subscriptions just because a competitor uses them.

**Acceptance:** combine filters with semantic search; handle empty results and unknown metadata; audition then import; save/reopen without broken media; cleanup removes temporary previews but preserves referenced, undoable and user-owned files; canceling an import has no partial result.

## 14. Phase P10 — Simple export and recovery completion

**Covers:** remaining S4.

1. Build on the existing renderer/export API. Make **Export Song** a prominent action that selects a clear arrangement range and opens the platform's required destination flow. “One-button” means a clear entry point, not bypassing destination consent or file errors.
2. Use a supported broadly playable default, such as stereo WAV if the renderer supports it. Verify supported encoders before offering compressed formats. Do not add codec dependencies solely for feature parity.
3. Show duration/range, destination and relevant output defaults. Preserve access to advanced export options. Do not silently alter the project's sample rate or rendering quality.
4. Define exports against the actual Arrangement, including the engine's established effect-tail policy. If Session overrides are active, explain the export source and explicitly return/resolve it or use a renderer that targets Arrangement directly. Never export a different source without telling the user.
5. Handle an empty arrangement with a useful route to P5. Distinguish empty content from intentionally silent passages; avoid declaring an entire project invalid based on one silent region.
6. Use real progress where supported, support cancellation, write through the existing safe output mechanism and report success only after finalization. Preserve existing files on failure; require explicit overwrite handling.
7. Provide a real Reveal/Open/Share action supported by the host. If OS sharing is unavailable, provide export/reveal without a fake Share button.
8. Complete any autosave/recovery gaps found in P1/P11. Ensure exporting does not silently mark unsaved project edits as saved.

**Acceptance:** exported file exists, decodes/plays, has expected channel count/rate/duration within justified tail tolerance and contains expected music; cancellation leaves no misleading final file; missing/unwritable destinations produce an error; user can find the result; save/reopen and recovery remain valid.

## 15. Phase P11 — End-to-end verification and focused polish

Run the complete beginner journey using actual UI and audio on an available supported host. Automated coverage should target behavior and failure modes, not snapshots of every label.

| Scenario | Steps | Required result |
|---|---|---|
| Offline first sound | Clean profile, disconnect network, create starter, play | Audible editable clips; no account/model gate |
| Make it your own | Edit drums, draw a note, change chord/sound | Real musical change; one coherent history per action |
| Keyboard focus | Play via QWERTY, focus search/coach, switch track | No notes from typing; no stuck notes |
| Coach help | Local model ready, request scoped change, inspect/apply/A-B/undo | Real validated commands, audible changes and reliable undo |
| AI unavailable | Remove configured model or simulate load failure | Honest status, useful setup, manual music still works |
| Stale/canceled AI | Edit while model responds; cancel; switch projects | No stale overwrite or cross-project mutation |
| Build song | Launch scenes, capture/commit, return to Arrangement, edit sections | Timing and playback-source labels match engine behavior |
| Mix | Adjust levels/pan; apply simple spatial FX; Analyze; expand controls | Real routing and measurement; hidden settings retained |
| Persist | Save, close, reopen; exercise recovery separately | Music, assets and relevant preferences restored |
| Export | Export arrangement, open in independent decoder/player | Expected source, duration and audible result |
| Compatibility | Load representative older projects and advanced devices | No unrequested musical transformation or missing data |
| Reachability | Skip onboarding, use advanced Mix, DSL and Session | Beginner presentation does not trap experienced users |

### Additional checks

- Keyboard-only operation, visible focus, accessible labels, non-color-only source/status cues, normal text scaling and the repository's minimum supported window size.
- Guidance never permanently covers transport, notes, error messages or export controls.
- Repeated open/close, starter creation, preview cancellation and view switching do not leak resources or leave workers running after project close.
- Compare audio CPU/dropouts and UI responsiveness against P0 on representative supported configurations. Investigate new regressions; do not invent universal latency thresholds.
- For GUI/audio unavailable in CI, provide a precise manual script and record results separately. Passing headless unit tests is insufficient proof of playable audio.

### Small beginner trial

After technical checks, ask a few actual beginners to use the app without coaching from the developer. Measure first sound, independent edit, correct identification of what is playing, arrangement completion, save and export. Record where they get stuck. This is a qualitative pilot, not statistical validation. Any external analytics should follow existing product consent practices; local/manual observations are sufficient for this phase.

**Done when:** Must/Strong paths work end to end, remaining failures have concrete evidence, and critical data-loss, wrong-source playback/export, crash, stuck-note and inaccessible-navigation regressions are resolved. Fix observed friction; do not expand the roadmap during polish.

## 16. Phase P12 — Release handoff

1. Update user-facing guidance for the new first-session flow, Session/Arrangement source behavior, simple Mix, local model setup and export.
2. Update screenshots using the actual final build; remove stale placeholders and abandoned experimental controls.
3. Finalize M1–M5 and S1–S6 traceability with links to code, validation and any remaining restrictions. Mark partially delivered requirements as partial.
4. Record compatibility boundaries, host/model coverage and remaining manual gates. Do not describe untested platforms as verified.
5. Use existing feature-flag/release mechanisms if available. Prefer disabling a new presentation while preserving project data over inventing a broad rollback framework. Document how to return to the previous release and the limits of project backward compatibility.
6. Prepare a concise review/PR description: beginner problem, new behavior, reused architecture, evidence and material limitations. Leave unrelated merge/release/publication steps outside this task unless separately authorized.

**Core definition of done:** a beginner can make editable music offline, make an unaided edit, receive real local AI help when configured, inspect and undo its actions, arrange a short song with clear playback ownership, mix it, save/recover/reopen it, and produce a verified export while advanced workflows still work.

## 17. Optional backlog — implement only after the core is accepted

These are the report's Nice items. Do not let them block first sound or silently increase the core scope.

| Item | Bounded implementation | Dependencies and acceptance |
|---|---|---|
| N1: packed starter grids | Add a few curated multi-scene packs using P2's format | P2/P5/P9; every scene editable, assets cleared, pack size measured, levels checked |
| N2: interactive tutorial | Add a resumable optional tour tied to real actions | P3–P11; detects actual completion, skips safely, does not trap focus or block playback |
| N3: constrained instruments | Add focused musical control surfaces over existing devices | P4/P6; same parameter model, advanced access and expressive data preserved |
| N4: collaboration/cloud share | First produce a separate architecture/product proposal | P10/P12; resolve identity, ownership, permissions, storage, conflict behavior and offline operation before implementation; not a surprise backend project |
| N5: dice/randomize | Seeded randomization using existing compatible media search | P2/P9; explicit locked parts respected, preview/cancel, one undo, reproducible recipe |
| N5b: layered loop matching | Genre/key/BPM constraints choose editable Session layers with real action history | P7/P9/N5; no opaque stereo-song output; preserve rights/metadata and unsupported-transform handling; competitor membership does not require this app to add a paywall |
| N6: expanded sound-design coach | Extend existing sound-design agent into supported Sampler, Drum Grid and FX actions | P4/P6/P7/P8; bounded commands, real param deltas, audition and undo; no unrestricted graph rewriting |

Additional preservation polish: make existing Chord Track, stems and take/comp workflows easier to find, with verified per-platform capability states. Keep them out of the required first-session hero path. Palette-style loop matching is distinct from opaque full-song generation; the product helps users shape music and does not promise hit songs.

## 18. Explicit exclusions

- A fourth Pattern/Playlist composition model alongside Session and Arrangement.
- FL Studio suite parity or a plugin-count competition.
- An unbounded day-one VST marketplace.
- Advanced send/bus/sidechain routing as mandatory onboarding.
- Opaque full-song generation as the default or a required milestone.
- Multiple uncoordinated agents writing the project.
- Cloud accounts, social walls or paid keys before first sound/first local AI use.
- A permanent simple mode that hides Session, DSL, Analyze or advanced editing.
- Copying competitor distinctive UI, trademarks or content.
- Timeline-only redesigns, mandatory Faust/DSP instruction, or rebuilding existing stems/comping/Chord Track capabilities.

## 19. Copy-ready Cursor kickoff prompt

Paste the following into Cursor with this file and the source PDF attached. Use it in the actual Magda-based repository.

```text
Implement the beginner experience specified in the attached Magda_Cursor_Auto_Implementation_Plan.md. The attached Magda_DAW_Beginner_Feature_Gaps.pdf is the source for product scope and priorities. Use Cursor Auto for this task.

Start with Phase P0 only: inspect the actual repository, read applicable AGENTS.md/contributor instructions, establish the build/test baseline, locate the existing implementations, and create the durable planning/progress records described in the plan. Adapt proposed responsibilities to real file paths and symbols. Do not invent APIs, DSL syntax, supported platforms or verification results. Do not start broad feature coding during discovery.

Preserve Session + Arrangement + Mixer, hybrid tracks, the existing audio engine, device systems, command/undo/serialization path, auditable DSL, agents and media search. Productize existing Drum Grid/Chord Engine tools; Chord Track is a separate existing harmonic guide. Preserve existing stems and comping. No framework rewrite, duplicate project model, dependency-upgrade sweep or competitor UI clone.

The core implementation includes M1–M5 and S1–S6. N items remain a documented optional backlog. Work in dependency order and one reviewable phase at a time. Each phase must deliver real behavior with persistence, undo, meaningful errors and appropriate verification. A working-looking screen does not count as an implemented feature.

Keep model inference and I/O off the audio thread. All AI project mutations must pass the existing validated command path, preserve intervening user edits, expose actual actions, and remain undoable. Offline starters must work without a model or account. Do not label deterministic fallback behavior as AI.

Preserve unrelated working-tree changes. Use a feature branch/worktree as appropriate. Do not merge, publish, buy services or perform destructive changes as part of this task.

Complete P0, update the plan/progress records, and return the repository capability inventory, actual baseline results, concrete next files/symbols for P1, and any genuine blockers. Resolve ordinary implementation choices yourself. Mark unavailable tests as not run; never fake completion. End at the P0 checkpoint so I can review the discovered architecture before the implementation phases begin.
```

## 20. Copy-ready next-phase prompt

Use this after P0 and after each subsequent phase. The agent should read its saved records, not rely on an old chat remembering everything.

```text
Continue implementing Magda_Cursor_Auto_Implementation_Plan.md.

Read applicable repository instructions and the saved inventory, plan, decisions, progress and verification records. Inspect the working tree and the actual code before editing. Identify the next incomplete phase whose dependencies are satisfied, and implement that phase only. Split it into smaller working slices if necessary, but do not skip its remaining acceptance criteria or expand into Nice backlog items.

Reuse the existing architecture and complete the real user flow: domain behavior, UI connection, undo/redo, persistence, errors and focused verification. Run the repository-required checks and the phase's meaningful tests. Perform real UI/audio/model checks where the environment supports them; explicitly label the rest pending. Do not count mocks as live integration proof.

Resolve ordinary implementation details without asking me. If a genuine blocker appears, complete other safe work inside this phase, record the exact blocker and the smallest next action. Do not silently weaken a requirement to mark the phase done.

Update progress and verification records. Report the behavior delivered, changed files, tests with actual results, unresolved gates, and the next phase. End at this phase boundary; do not merge or publish.
```

## 21. Copy-ready final review prompt

```text
Review the implemented Magda beginner experience against Magda_Cursor_Auto_Implementation_Plan.md and the source PDF. Inspect code and run the appropriate checks; do not rely solely on prior completion summaries.

Trace every M1–M5 and S1–S6 item to real implementation and evidence. Look for fake or unconnected controls, duplicated existing systems, unsafe audio-thread work, stale/concurrent AI writes, invalid DSL handling, broken undo, missing media after reopen, destructive recovery, stuck QWERTY notes, scale-lock data loss, incorrect Session/Arrangement source labels, dropped capture data, duplicate/feedback FX routing, and exports of the wrong source or invalid files.

Run the complete journey: offline starter -> independent edit -> real configured local AI suggestion with action details and undo -> Session capture/commit -> Return to Arrangement -> simple Mix and Analyze -> save/reopen/recover -> playable export. Verify older-project and advanced-view behavior. Mark tests unavailable in this environment as pending, never passed.

Fix concrete issues within approved scope and add focused regression coverage where warranted. Keep optional items deferred. Update the traceability/progress records and give me a concise readiness report with remaining blockers and the evidence required to close them. Do not merge or publish.
```

## Source note

This plan derives its scope from the supplied `Magda_DAW_Beginner_Feature_Gaps.pdf`, especially sections 2–4 (north star, baseline and locked shortlist), 5–6 (opportunities and exclusions), and 7 (uncertainty). Its internal research claim IDs are not independently reopened citations here. Phase order, technical contracts, acceptance tests and execution prompts are new implementation proposals. Repository inspection must establish the actual code and current capabilities before implementation.
