# Magda: phase-by-phase Cursor Auto prompts

Companion to Magda_Cursor_Auto_Implementation_Plan.md • Prepared for Logan Chambers • 16 September 2026

This file turns the implementation plan into individual execution prompts. It adds a concrete musical fixture, observable completion checks, and recovery prompts for interrupted work. The source scope remains M1–M5 and S1–S6 from Magda_DAW_Beginner_Feature_Gaps.pdf; Nice items remain optional.

No Magda repository has been inspected while preparing this handoff. Proposed labels and fixture data below are design inputs, not claims about existing APIs, DSL syntax, preset names or file formats.

## How to use this

1. Open your actual Magda-based repository in Cursor and select Auto for the task.
2. Give the agent this file, the original implementation plan and the source PDF. Keep the Markdown files available in the project, using its established documentation location.
3. Begin with P0. If P0 or later work already exists, have Cursor verify and reuse it instead of starting over.
4. Paste one phase prompt at a time. Each prompt points to the full specification and ends at a useful checkpoint.
5. If you want Cursor to work through successive phases without you pasting each one, use the sequential-work prompt near the end. It changes the stopping behavior; it does not change the scope or acceptance criteria.

The prompts are deliberately specific about outcomes and failure cases while leaving file names, interfaces and implementation techniques to repository inspection. A native desktop app must be verified with its actual runtime and tooling; do not introduce web frameworks or browser-only tests unless that is the architecture found in the repository.

## Working agreement for every prompt

The detailed architecture contracts in the original plan apply throughout. In particular:

- Read repository instructions; preserve unrelated changes; work through existing project, audio, device, command, history and serialization systems.
- A beginner screen must edit the same musical objects used by advanced tools. Hiding a control must not reset its data.
- Keep model inference, disk access, decoding and expensive analysis off the audio callback.
- Every meaningful musical edit must have an appropriate undo path, persistence behavior, failure behavior and real UI connection.
- Preserve existing Session/Arrangement/Mix, agents/DSL, Drum Grid, Chord Engine, Chord Track, comping, stems and media capabilities. Verify availability in the checkout and supported host.
- Inspect and adapt existing tests. Add focused tests for real behavioral risk; do not create dozens of tests asserting that labels or internal helper calls match the implementation.
- Never weaken assertions, remove required checks or invent test outcomes to get a green status.
- Do not expand into optional features, purchases, cloud provisioning, publishing, merging or unrelated refactors.
- Resolve normal implementation details autonomously. Record consequential assumptions and report actual blockers with the smallest next action.

Maintain the inventory, plan, decisions, progress and verification records described in the original plan. Every completion report must identify the phase, delivered behavior, validation evidence, unresolved gates and exact next step.

## A repeatable musical fixture

Use this proposal as a small reference project for starter creation, editing, save/reopen, arrangement and export. Translate it into existing project/recipe formats; do not introduce a fixture-only production data model. Curated shipping starters may sound different, but their tests should be equally reproducible.

### Fixture A: eight-bar idea

| Property | Value |
|---|---|
| Descriptive ID | beginner-fixture-a, version 1; adapt to existing ID conventions |
| Tempo | Constant 100 BPM |
| Meter | 4/4 |
| Loop | Eight bars, beginning at the project origin |
| Key/scale guidance | A natural minor; do not impose this on drum lanes |
| Tracks | Drums, Bass, Chords using existing hybrid-track infrastructure |
| Dependencies | Verified bundled devices/assets; no external plugin or model required |
| Expected musical duration | 19.2 seconds before release/effect tails |

Use the repository's musical time conversion utilities. For human descriptions below, bars and beats start at 1. Convert to the engine's actual indexing and tick units exactly once. In the no-tail timing fixture, define clips as the half-open musical interval from bar 1 start through bar 9 start; do not accidentally add a ninth bar.

**Drums:** every bar has kick on beats 1 and 3, snare on 2 and 4, and closed hi-hat on eighth notes. Map these roles through the actual kit; do not assume General MIDI note assignments match the bundled device. Use consistent, documented velocities with hats lower than the main hits. Trigger durations and choke behavior must follow the device's semantics.

**Harmony and bass:** each row covers two bars. Use sustained chord notes or the existing equivalent that yields reproducible editable note content. Bass plays the root once per beat, with a small gap before the next note to make timing clear.

| Bars | Chord | Chord MIDI note numbers | Bass MIDI root |
|---|---|---|---|
| 1–2 | A minor | 57, 60, 64 | 45 |
| 3–4 | F major | 53, 57, 60 | 41 |
| 5–6 | C major | 55, 60, 64 | 48 |
| 7–8 | G major | 55, 59, 62 | 43 |

MIDI numbers are used because displayed octave names can differ between applications. Adapt voicings if a verified bundled instrument has a restricted range, and record the resulting fixture. Keep Chord Engine in the actual beginner flow; the Chord Track remains harmonic guidance, not the sound source. Prefer the existing engine's editable pattern output where available.

Use modest initial levels and listen to the summed result. These musical notes do not establish asset redistribution rights; use the source/asset manifest required by P2.

### Fixture B: short arrangement

Make a 32-bar arrangement using editable clips derived from Fixture A:

| Bars | Section | Content |
|---|---|---|
| 1–8 | Intro | Chords and a lighter rhythm |
| 9–16 | Main | All three parts |
| 17–24 | Variation | An explicitly saved small drum or bass change |
| 25–32 | Ending | Thinner texture and a clear ending |

At the fixture's constant tempo and meter, its musical range is 76.8 seconds before tails. For other projects, derive time from the actual tempo map. Build sections with existing clip/scene/marker behavior, not a fourth composition model.

For deterministic timing checks, use controlled supported instruments and a documented tail policy. For musical quality and device integration, listen to the actual shipping instruments. Nonzero audio alone is not proof of a correct or pleasant result.

### Fixture C: clearly different playback sources

Use one track with a sparse arrangement rhythm and a recognizably different Session rhythm. Add a second track that stays in Arrangement. This makes a mixed-source state audible and visible. Use it in P5 and P10 to detect badges derived from the active tab and exports that render the wrong source.

Do not reuse the same audio content for both sources in this test: a wrong-source export could otherwise pass unnoticed.

## P0 prompt — inspect and map the repository

~~~text
Execute P0 of Magda_Cursor_Auto_Implementation_Plan.md. Use Magda_Cursor_Auto_Phase_Prompts.md as the execution companion and the attached feature-gap PDF for scope. Apply the working agreement in the companion.

This phase is discovery with a concrete handoff. Read applicable repository instructions. Inspect branch/revision, working-tree changes, language, native/UI framework, supported hosts, build setup and test commands. Reuse an existing inventory if present, but check that its evidence matches the current revision.

Locate actual files and symbols for new-project/welcome, Session, Arrangement, Mixer, tracks/clips, transport/source arbitration, device creation, command/undo, project save/load/recovery, asset ownership, media search, export, DSL validation/execution, specialist agents and local model support. Trace callers and representative tests; a name match is not proof that a feature works.

For each M1–M5 and S1–S6 requirement, record working/reusable, needs extension, missing or unverified, with code evidence and host restrictions. Distinguish Chord Track from Chord Engine. Inspect existing project fixtures and advanced data that beginner views must preserve.

Run the normal available baseline checks and launch the actual app if supported. Record commands, results and environment limitations. Do not install a different app architecture or repair unrelated failures.

Produce the durable inventory and phase plan using actual files/symbols. Identify the smallest P1 slice that proves grouped editing, save/reopen and undo through existing infrastructure. Include the concrete verification command or manual procedure for that slice.

End at the P0 checkpoint with evidence and the proposed P1 touchpoints. Do not implement broad features during discovery or mark unavailable GUI/audio tests passed.
~~~

## P1 prompt — edits, undo and recovery

~~~text
Implement P1 from Magda_Cursor_Auto_Implementation_Plan.md, applying the companion's working agreement. Read the current inventory, progress and relevant code first. Finish missing P1 behavior without duplicating existing history or persistence systems.

Trace a real clip insertion/edit through the command path, engine/model update, dirty state, undo/redo, save and reopen. Use a small deterministic project fixture. Add the minimum support necessary for future starter and agent actions to behave as one coherent undoable operation.

Prevalidate resources and stage asynchronous work safely. Inject a failure after an intermediate step using the existing test harness or a test seam; do not ship a user-facing failure switch. Verify the original project remains intact and cleanup affects only newly owned resources. A double-click/retry for one operation must not duplicate its edits.

Inspect named-project and unsaved-project recovery. Preserve the prior valid file when a write fails. Recover into an identified recovery session/copy. Resolve dirty-state races: if another edit happens while an older revision is being saved, finishing that write must not mark the newer revision saved. Capture a consistent project snapshot using existing synchronization, off the real-time callback.

Verify grouped undo/redo, failure rollback, missing media, save/reopen and controlled interrupted recovery. Preserve the original saved project and source assets throughout. If crash recovery requires a host unavailable here, implement the safe path and document the exact pending procedure rather than claiming it passed.

Update progress with code pointers, actual commands/results and unresolved gates. End at P1. Do not build the welcome UI or introduce a replacement serialization framework.
~~~

## P2 prompt — editable offline starter

~~~text
Implement P2 from the master plan and use Fixture A in Magda_Cursor_Auto_Phase_Prompts.md as the reference musical recipe. Read the existing device, preset, asset and command systems before editing. Apply the companion's working agreement.

Build the starter as real editable Session content through existing musical objects. Begin with a working drum slice if needed, then complete drums, bass and chords. Route Chord Engine through a real sound-producing instrument where required; Chord Track is only the harmonic guide.

Reuse an existing recipe/template format if present. Otherwise add only the metadata and integration required for this starter: stable ID/version, musical timing, tempo/meter/key, device/preset references, clip content and assets. Use actual device IDs and valid DSL/commands found in the code. Do not ship invented syntax or hardcoded local absolute asset paths.

Resolve/prepare all resources before committing the grouped edit. Keep decoding/loading off the audio callback. Handle missing assets, cancellation, concurrent project closure and repeated submission. A late completion from a canceled request must not create tracks or start playback. Clean up only operation-owned temporary resources.

Record the real committed action details and a matching plain-language summary. Use cleared bundled assets and track attribution/provenance. Listen for sensible gain and timing; do not hide bad gain with automatic mastering.

Verify offline creation without an account/model, correct eight-bar bounds and musical objects, audible output, editability, grouped undo/redo, missing-resource failure, save/reopen and asset portability under existing project conventions. Preserve old-project compatibility.

End at P2 with a repeatable way to create and verify the fixture. Do not build a marketplace, content subscription or full-song generation service.
~~~

## P3 prompt — first-session flow

~~~text
Implement P3 from the master plan using the real P2 starter pipeline. Apply the working agreement and inspect existing welcome, navigation, settings and audio-device initialization flows.

Deliver a concise Beat / Song / Blank entry with sensible defaults, optional choices, skip and a discoverable way to reopen the guide. Create and Play must create real content and invoke real playback after preparation. Guided new projects open in Session; existing projects and experienced users retain their established opening/layout behavior.

Show real loading, readiness and failure states. If audio output initialization fails after creating the music, preserve the music and offer device setup/retry. Retrying audio must not insert the starter again. Do not request microphone permission for a MIDI-only starter unless an actual platform requirement is established.

Make cancel/back/project-close invalidate outstanding creation callbacks. Distinguish readiness from permission to play: finishing a canceled background job must never trigger unexpected playback. Avoid duplicate creation on rapid clicks.

After success, show actual additions and one direct manual edit, such as changing a drum step. Session, Arrangement, Mix, advanced tools and real action/DSL details remain reachable. Keep missing-model and offline states usable. Do not simulate AI to fill the screen.

Check keyboard/focus behavior, normal text scaling, skip/reopen, cancellation during loading, missing output device, retry, existing-project reopening and first sound without network. Record observed startup/loading/device-setup time separately. A screenshot alone does not establish success.

Update implementation and verification records and end at P3. Song section tools belong to P5; do not present unfinished section actions as working controls.
~~~

## P4 prompt — instrument controls and editing

~~~text
Implement P4 of the master plan. Apply the companion working agreement and use the P2 starter for real editing. Split into instrument entry points, piano-roll behavior and QWERTY input slices if needed, but complete all three before marking P4 verified.

Expose Make a Beat, Add Chords and Play a Sound through existing Drum Grid, Chord Engine and Sampler/device paths. Use actual parameters, presets and routing. Preserve per-pad chains and advanced tools. Do not rebuild the sequencer, arpeggiator or Chord Track.

Adapt the existing piano roll to show the core note-editing controls. Simple and advanced presentations must share clip data. Clearly distinguish rhythmic snap and scale lock. Specify a deterministic rule for new-note pitch snapping and test it at boundaries/ties. Existing out-of-scale notes and expressive data remain unchanged when switching modes or changing scale guidance. Drum rows must not inherit pitched scale restrictions.

Expose existing QWERTY MIDI with an explicit on/off state and useful mapping/controls. Text inputs and shortcuts must not accidentally trigger notes. Track active live-input voices correctly: if the target changes before key release, route note-offs to the original target or flush those live-input notes before retargeting. Avoid blindly silencing unrelated arrangement playback or external MIDI voices.

Verify an edited beat, note add/move/resize/delete and undo, scale behavior, simple/advanced round-trip preservation, text entry while keyboard mode is active, track/device switching with a held key, focus loss and save/reopen. Record real audible checks where supported and leave unavailable ones pending.

End at P4 with actual behavior and evidence. Do not add an unrelated virtual-instrument framework or enable all advanced parameters on the beginner front door.
~~~

## P5 prompt — arrangement and playback ownership

~~~text
Implement P5 from the master plan. Apply the companion working agreement. Read transport, launch scheduling, Session/Arrangement arbitration and existing capture code before editing UI. Use Fixture B for the short song and Fixture C to distinguish playback sources.

Reuse real performance capture if present. Capture Jam must record actual launch timing; Place Scene in Arrangement may perform a deterministic insertion and must be labeled accordingly. If performance capture is missing, implement its required domain behavior as a separate slice before enabling that control. Do not label static copying as capture.

Keep all committed music editable. Handle occupied destinations without silent overwrite. Preserve supported automation, offsets, routing and timing; identify concrete unsupported cases instead of silently dropping them. Use the existing musical timing/tempo map.

Drive playback-source indicators from real engine state, including mixed-track ownership. Merely viewing Arrangement must not change what is playing. Return to Arrangement must call the actual source-switch mechanism at its correct scheduling boundary. Keep labels truthful while a switch is queued as well as after it executes.

Create thin editable intro/main/variation/ending section templates through existing scenes/clips/markers. Contextual guidance should teach launch, commit/capture, return and edit. Do not create a fourth pattern model.

Verify different launch times, empty/pending launches, transport stop, loop wrap, tempo changes, mixed sources, an occupied destination, capture undo and save/reopen. Test Arrangement sound against the distinct Session rhythm in Fixture C. State precisely what happens if capture is canceled or undo is requested while it is active.

End at P5 with real playback and timing evidence. Do not declare capture complete because a timeline screenshot contains clips.
~~~

## P6 prompt — simple mixing and a shared effect

~~~text
Implement P6 from the master plan, applying the companion working agreement. Inspect actual mixer controls, routing, parameter smoothing, layout preferences and Mix Analyze.

Give guided projects a clear basic mixer with meters, faders, pan, mute and solo. Advanced settings are a presentation choice; collapsing them must not change DSP or serialized values. Preserve experienced users' settings. Keep Analyze and the advanced view readily reachable.

Reuse measurement-grounded analysis and display real readiness/errors. Distinguish analysis from optional model interpretation. Until P7's validated mutation path exists, do not add an unsafe new shortcut that auto-applies model mix advice; use the existing safe path or leave that integration explicitly tracked for P7/P8.

Add a small set of useful effect macros through existing processors. One optional spatial effect should create/reuse an appropriate shared return. Inspect compatibility, identity and routing rather than deciding reuse from a display name alone. Respect user-created routing. Repeated use must not duplicate returns or create feedback.

Document what each macro actually controls, preserve valid parameter ranges/smoothing, and make changes undoable. If multiple tracks share the return, undoing one track's send change must not delete a return now used by others. Follow command ordering and reference ownership.

Verify audible level/pan/effect changes, mute/solo, preservation when expanding/collapsing, Analyze reachability, return reuse, routing validity, undo/redo and save/reopen. Compare representative audio CPU/responsiveness with baseline without inventing universal performance thresholds.

End at P6 with actual results and any P7/P8 integration still pending. Do not add a routing tutorial or make advanced buses mandatory for first sound.
~~~

## P7 prompt — one coach and safe project actions

~~~text
Implement P7 from the master plan and the companion working agreement. Inspect specialist routing, conversation context, DSL validation/execution, permissions and project command serialization.

Expose one coherent conversation/plan while retaining existing specialists. View changes may affect tools and selection but must not erase the request or introduce multiple simultaneous project writers. Preserve the existing authorization boundary.

Represent a request's lifecycle and identity using current infrastructure. Capture stable project and target IDs plus the relevant state/revision. A later selection change must not silently retarget the request. A response arriving after cancellation, close, project switch or target deletion must not apply.

Validate schema, real DSL syntax, permitted operation/target, parameter bounds, resource access and relevant state before mutation. Treat imported names/metadata as data. Model output never grants tool permission. Reject or replan stale proposals without reverting intervening manual edits.

Stage a proposal without mutating live music. At Apply, atomically coordinate validation and enqueue/commit through the existing writer path; avoid a check-then-write race. Do not hold a project lock during inference. Use operation identity to prevent a double Apply or retry from duplicating edits.

Record generated, rejected and applied commands distinctly, with actual deltas and grouped undo. Cancellation of an applying operation must have a defined safe result, such as completion of an already committed transaction or controlled rollback; do not claim cancellation reversed work that remains committed.

Verify valid and invalid responses, target deletion, stale edits, view changes, project switch, cancellation, retry, rapid Apply, failure rollback and undo preserving unrelated edits. Mock responses may exercise the real boundary here; they do not establish live-model success.

End at P7 with evidence and a clear route to P8. Keep expanded sound-design skills N6 deferred.
~~~

## P8 prompt — local AI coach and before/after

~~~text
Implement P8 from the master plan. Apply the companion working agreement and use P7 for every new coach mutation. Reuse the actual local model/runtime integration found in P0.

Offer a small set of context-aware beginner requests supported by real specialists. Show intended targets, a short explanation, real validated DSL/action details and Apply/Cancel. Avoid unsupported promises about song quality or automatic musical learning.

Handle model ready, absent, loading, incompatible, resource failure, canceled and failed states. Use existing setup/download infrastructure if present; no new mandatory cloud account or paid key. Offline starter/manual features remain available when AI is unavailable and must be labeled honestly.

Build real before/after audition. Prefer existing isolated preview/render infrastructure. If a scoped reversible preview is necessary, prevent it from overwriting later manual edits, corrupting autosave or consuming the normal undo stack in misleading ways. Define what happens if the user changes a target, switches projects, closes the preview or edits while it is active. Never restore the entire old project over current work just to exit A/B.

Display actual inference/validation/application progress. Late canceled responses do nothing. Keep inference off the audio callback, release resources safely and preserve manual interaction. Ground mix explanations in actual measurement data.

After an applied suggestion, route to one manual edit the user can make. A detected edit is an action acknowledgment, not evidence of learning improvement.

Verify with a real configured local model on a supported host: propose, inspect, apply, audible A/B, cancel/fail, undo and independent edit. Also verify absent-model behavior. Record model/runtime identity and actual results. If this environment cannot run a model, retain implemented work with the live-model gate pending.

End at P8; do not turn a mocked response or deterministic template into a claimed AI integration.
~~~

## P9 prompt — curated media and safe audition

~~~text
Implement P9 from the master plan. Apply the companion working agreement. Extend the current Media Library and asset-management path; preserve existing semantic/similarity search.

Expose curated starter collections plus useful type/instrument, key and tempo filters. Specify how filters combine with search and how unknown metadata behaves. Distinguish declared versus analyzed metadata and avoid claiming guessed values are known. Preserve useful empty-results and offline states.

Use supported time stretching/transposition only. Show original metadata and applied changes where they matter. Do not force unpitched drums through key matching or promise unsupported conversion ranges. Debounce/cancel expensive search work according to existing infrastructure, and ignore old results after newer filter requests.

Audition through the real preview path with correct stop/switch behavior. Import creates normal editable clips and managed asset references through the command system. Cancel/failed import must not leave partial tracks or broken references.

Track temporary audition resources separately from source and project media. Cleanup must account for current projects, undo/redo, recovery, active previews and in-progress renders. If safe ownership/reference checks are unavailable, retain the uncertain cache item and record the cleanup gap rather than risking source data. Do not sweep arbitrary user directories.

Verify known/unknown BPM/key filters, combined search, empty results, quick filter changes, audition switching, import, undo/redo, save/reopen and cleanup. Exercise the case where an imported clip is undone, its preview cache is cleaned, and then redo is requested: required media must still resolve.

End at P9 with behavior and evidence. Keep randomize/loop matching and cloud catalog subscriptions outside core scope.
~~~

## P10 prompt — export the intended song

~~~text
Implement P10 from the master plan and apply the companion working agreement. Reuse the actual renderer, encoder and destination handling. Use Fixture B for duration and Fixture C for source correctness.

Provide Export Song as a clear entry into real destination/output options with sensible supported defaults. Do not advertise codecs absent from the build. Keep advanced export available and avoid silently changing project sample rate or quality settings.

Resolve Arrangement range and playback source explicitly. With a recognizably different Session override active, the output must follow the stated export source. An empty arrangement should offer the real route to commit/capture, not silently export unrelated Session playback. Apply the renderer's established effect-tail policy and show enough detail to make the result understandable.

Use a consistent render snapshot or the existing protected rendering lifecycle. Define behavior if the project is edited/closed during export; asset cleanup must not remove resources in use. Rendering must not change the user's audible Session/Arrangement state without a clear, reversible action.

Show actual progress, allow supported cancellation, finalize safely and preserve a pre-existing destination on failure. Report success only for a completed valid file. Reveal/Open/Share must call supported host behavior. Exporting does not save the editable project or clear unrelated dirty state.

Verify the file with an independent decoder/player: expected format, channels, rate, musical duration/tails and the actual intended content. Nonzero bytes and a success toast are insufficient. Exercise cancellation, overwrite, unwritable destination, render failure, active Session override and save/reopen.

End at P10 with output evidence and remaining host-specific gates. Do not add a backend or third-party sharing service as incidental work.
~~~

## P11 prompt — exercise the whole experience

~~~text
Execute P11 from the master plan, applying the companion working agreement. Inspect the implementation and evidence, then verify the complete user journey. Do not accept prior phase summaries as proof.

With a clean test profile, create Fixture A offline without a model/account, hear it and edit a drum step/note. Check QWERTY text focus and held-note target changes. Save, close and reopen. Make Fixture B through the actual Session-to-Arrangement controls; use Fixture C to verify mixed-source labels and Return to Arrangement.

Balance the mix, use the shared effect, access Analyze and expand/collapse advanced controls. With a real configured local model, inspect/apply/preview/undo a scoped suggestion and then make an independent manual edit. Also test missing-model behavior and stale/canceled responses. Exercise controlled recovery and export a playable song using the actual UI.

Run focused adversarial cases from earlier prompts: missing assets, failed grouped edits, late callbacks, dirty-state save races, occupied capture destinations, preview restoration after manual edits, cleanup followed by redo and wrong-source exports. Check representative older projects and advanced expressive/routing data.

Verify keyboard-only access, labels/focus, supported text scaling and minimum window size. Compare relevant performance with P0; investigate concrete regressions. Use available native GUI/audio tools. If only headless checks can run, record the exact missing host procedure and leave it pending.

Fix actual defects within scope. Add focused regression coverage where necessary, preserving meaningful assertions. Do not rewrite healthy architecture or begin optional backlog work.

Document actual results and prepare a short uncoached beginner trial script. Do not claim a beginner trial occurred unless people actually completed it. End at P11 with technical status, external/manual gates and the smallest actions needed to close them.
~~~

## P12 prompt — make the result reviewable

~~~text
Complete P12 of the master plan, applying the companion working agreement. Reconcile each M1–M5 and S1–S6 requirement with actual code and verification evidence.

Update user guidance for first sound, manual editing, Session/Arrangement playback ownership, basic Mix, real local-model setup, recovery and export. Capture screenshots from the actual completed build where available. Remove stale demo text or abandoned nonfunctional controls.

Prepare a compact requirement table containing status, code locations, tests/manual evidence and remaining limitations. Distinguish implemented, verified, partial and blocked. A missing GUI/model/OS run remains a pending gate, not a silent pass.

Use existing release/feature-flag conventions. Document how to disable the new presentation or return to a previous release without deleting musical data. State actual file compatibility; do not promise an old version opens new project formats unless verified.

Prepare the review description: the beginner problem, resulting behavior, important architecture reused, focused validation and material risks. Identify any local manual checks Logan must perform with exact steps and expected results. Keep the optional backlog clearly separate.

Run only the final checks required by the repository or unresolved risk; do not repeat expensive suites without a reason. Preserve unrelated changes and leave the work ready for review. Do not merge, publish or set up external services.

End with an evidence-based readiness report. If everything has not been verified, state precisely what is complete and what remains, without describing the whole implementation as production-ready.
~~~

## Checkpoint record to keep Cursor honest

The agent can use this compact template in the repository's existing progress record. It is not a new application data schema.

~~~text
Phase / slice:
Revision or working-tree state:
User-visible outcome:
Implementation status: not started / in progress / implemented / partial / blocked
Verification status: not run / passing / failing / partly unavailable
Relevant source requirement IDs:
Code locations:
Existing systems reused:
Automated commands and actual outcomes:
Manual GUI/audio/model checks actually performed:
Evidence location, if produced:
Unresolved gate and exact next action:
Next eligible phase or remaining slice:
~~~

Implementation and verification have separate status fields so an unavailable audio device does not erase completed code or become a fabricated pass. If a dependent behavior is actually broken, repair it before building on it. If only host verification is unavailable, continue technically independent work while retaining the release gate.

## Prompt — continue sequentially with fewer handoffs

This is an alternative to pasting every phase prompt. Use it when you want the agent to keep going after it records each checkpoint. It explicitly supersedes the stop-at-phase-boundary wording in the individual prompts for this run.

~~~text
Implement the core Magda beginner experience using Magda_Cursor_Auto_Implementation_Plan.md and Magda_Cursor_Auto_Phase_Prompts.md. Use the source PDF for product scope. Apply the working agreement and all acceptance criteria.

Work sequentially through the next incomplete phases. For this run, continue after phase checkpoints instead of stopping for my confirmation at every boundary. This overrides only the phase-boundary stopping language. Keep the scope M1–M5 and S1–S6; leave Nice items deferred.

Start by inspecting the actual repository and existing progress. Complete or refresh P0 as needed, then implement in dependency order. Reuse completed behavior and evidence when still applicable. Keep one active implementation slice and one project/repository writer; do not introduce parallel agent work.

After each slice, verify the relevant behavior, update durable records, and proceed when dependencies are satisfied. Resolve ordinary choices yourself. If a host-only audio/GUI/model check is unavailable, retain it as a pending gate and continue independent work; do not falsely mark it passed. An actual broken dependency must be fixed before dependent work proceeds.

Use real controls, project data, commands, undo, persistence, audio and supported AI integration. Never replace missing implementation with fake progress or canned AI. Do not broaden architecture, weaken tests, merge, publish, purchase or configure external services.

Continue until core implementation and available verification are complete or every remaining task is genuinely blocked. Before stopping, finish other safe independent work, record exact blockers and write the next actionable handoff. If the run is interrupted, progress files must identify the unfinished slice accurately; do not claim an entire phase complete for partial work.
~~~

## Prompt — resume after a long or interrupted Cursor chat

~~~text
Resume the Magda beginner implementation from its actual repository state. Read applicable instructions, Magda_Cursor_Auto_Implementation_Plan.md, Magda_Cursor_Auto_Phase_Prompts.md and saved inventory/progress/decisions/verification records.

Inspect the current diff and relevant code. Establish which slice is actually complete, which changes are unfinished, and which tests are stale or unrun. Preserve unrelated edits. Do not recreate existing systems or restart P0 merely because the conversation is new.

Identify the smallest next slice with satisfied dependencies. Implement and verify it using the matching phase prompt, then update durable records. If records disagree with code, correct the records using evidence. Do not silently treat a partially wired screen as a completed feature.

Return the resulting behavior, actual checks, unresolved gates and exact next step. Use the current requested stopping mode: if sequential continuation was explicitly selected, keep progressing through eligible slices; otherwise finish at this phase checkpoint.
~~~

## Prompt — fix an implementation that looks done but fails in use

~~~text
Investigate the failing Magda beginner workflow against the master plan and phase companion. Preserve the approved scope and existing architecture.

First reproduce the user's exact steps on a controlled fixture and record expected versus actual behavior. Trace the path from the visible control through command validation, project/engine state, audio output, history and persistence as relevant. Inspect errors rather than hiding them. Identify whether the failure is in implementation, stale UI state, host capability or the test setup.

Make the smallest complete fix that addresses the cause. Add a focused regression check when the failure could recur and exercise neighboring affected behavior. Do not change expected results merely to match the bug, disable checks, swallow errors, or replace a real integration with a mock.

Re-run the failed workflow and relevant checks. Record what changed, why it fixes the failure, actual verification and anything still untested. Update phase status honestly. Do not continue feature expansion while a required dependency remains broken.
~~~

## What to ask Cursor to demonstrate at the end

Ask for this sequence in the actual app: create offline music → change a note yourself → obtain a real local coach suggestion when configured → inspect/apply/audition/undo it → capture or commit Session music → return to Arrangement → mix → save and reopen → export and play the result.

The original plan defines the architecture and acceptance criteria. This companion supplies the prompts and shared musical examples. Together they are the implementation handoff; repository evidence must still establish that the resulting application works.
