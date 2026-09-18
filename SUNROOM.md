# SUNROOM

Logan's personal psybient / psychill studio, built on [MAGDA](https://github.com/Conceptual-Machines/magda-core). Purple twilight, an orange sunset, real MIDI tracks and real instruments.

## Make your first track

1. Open **SUNROOM.app**. On **Create**, choose a feeling. **Floating** is a gentle starting point at 84 BPM.
2. Keep **8 bars / a loop** for a quick, fuller first listen (or pick 32/64 for a longer journey). Click **Build my journey**, then **Play**. Longer journeys open with mostly atmosphere; the pulse arrives later.
3. Open **Note garden**. Bright notes belong to your selected scale. Click a note to hear it; Shift-click a second note to hear their relationship. These are useful analogies, not rules about how everyone must feel.
4. Plant notes in the grid and click **Add melody to song**. Left to right is time; up is higher. The resulting MIDI stays editable.
5. **Sound shelf** adds instruments and WAV sounds to your song. Scroll its sound list for more. A WAV lands at the current playhead.
6. **Full studio** opens the arrangement, piano roll, mixer, recording, automation and plugin chains. Coloured blocks are your song; **SUNROOM / Guided studio** brings you back.
7. **Save project** keeps an editable `.mgd` song. **Export audio** makes a WAV or FLAC you can share. Start with WAV 24-bit, 48 kHz.

**Cmd-Z** undoes an edit, including a whole generated journey. Each new journey adds tracks, so undo an unwanted version before building another. The Create-page drawing previews the next journey; the actual song lives in Full studio. Warmth, Space and Motion shape the next generated music.

## Three AI choices

Open **AI companion** and choose a backend. This choice also updates the Full studio music, chord, command, sound-design and related agent roles.

- **This Mac / MLX:** Qwen3.5-4B, 4-bit weights, running on Apple Silicon. The app starts its private helper when asked, processes one local request at a time, and unloads the model after three idle minutes. It never falls back to a paid API. No AI is needed to create, play, edit or export music.
- **GPT-5.6 Luna / extra high:** paste your own OpenAI API key into the masked field and click **Save to Keychain**. The key is stored in macOS Keychain, never in song files or the source code. Requests use `gpt-5.6-luna`, `reasoning.effort: xhigh` and `store: false`. Your OpenAI account must have access and API billing. This is separate from a ChatGPT subscription. Model access is verified only by a real request using your key. [Official model documentation](https://developers.openai.com/api/docs/models/gpt-5.6-luna).
- **Mini PC / local server:** run an OpenAI-compatible model server on the Windows PC, then enter its LAN URL, including `/v1`. Prefer `https://` on the network; plain `http://` is only allowed for loopback. SUNROOM keeps audio on the Mac while the PC answers AI questions. A 16 GB PC's speed depends on its CPU/GPU; it does not combine its RAM or processors with the Mac's MLX runtime.

For the mini PC, [LM Studio's Developer tab](https://lmstudio.ai/docs/developer/core/server) can start a local server. Enable its local-network serving option, load a model suitable for that PC, and use the address and model ID shown by the server in AI companion. If the server requires a token, configure it in Full studio's AI settings. Keep this on your private network. The mini PC connection cannot be tested without its address and running server.

Try: **“Give me a recipe for a warm, floating psychill journey at 84 BPM.”** Review the assistant's proposed settings and click the recipe button to load them. You can also ask for a short melody: **Plant AI melody** fills the Note garden for you to hear and edit before adding it to the song. **Build my journey** turns those settings into editable music.

The companion gets the shared music guide, current settings, calculated note/timing information, device catalogue, track names and the selected clip's notes. Specialist agents also receive their existing MAGDA task instructions and validated command grammar. Models can make mistakes; the note garden and arrangement generator use deterministic music rules. The companion cannot hear the audio, and text alone does not change a song.

## Included sounds and plugins

The seven starter instruments cover slow pads, sub bass, marimba, djembe, kick, hats and bells. Presets include filtered echoes and spacious reverb. Full studio retains MAGDA's built-in synths, samplers, effects, modulation, visual meters and AU/VST3 hosting.

**48 original stereo WAVs** cover kicks, hand percussion, hats/shakers, bells, ambient air and transitions. These recordings are CC0: free to use and edit, including commercially. The generator and a manifest are included. No commercial sample-pack recordings are bundled. External third-party plugins are optional; the starter journey uses only built-in devices.

## Local setup and source

- Application data: `~/Library/SUNROOM`
- Local runtime: `~/Library/SUNROOM/runtime`
- Local model: `~/Library/SUNROOM/Models/Qwen3.5-4B-4bit`
- AI configuration: `~/Library/SUNROOM/sunroom-ai.json` (paths only)
- Core guide: `resources/sunroom/knowledge/coach.md`
- Original sound license: `resources/sunroom/Sounds/LICENSE.txt`

**Setup AI.command** reinstalls the MLX runtime/model using Homebrew Python 3.12. Initial model download is about 3 GB. The running app uses installed local files. Keep the source folder for rebuilding and development; the installed app uses its bundled resources and separate data directory.

## Developer notes

Upstream baseline: `15e9071d657bf9179432c6a0a3a62f8dd686d8a1` from Conceptual-Machines/magda-core. Local branch: `codex/sunroom`. Upstream GPLv3 licensing and third-party notices remain in place; this is a personal fork, not an official MAGDA release.

```sh
cmake -S . -B build-sunroom -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DMAGDA_FULL_VERSION=0.1.0-sunroom -DMAGDA_BUILD_TESTS=ON \
  -DMAGDA_BUILD_NATIVE_ENGINE=OFF -DMAGDA_HAVE_CLAP=ON -DMAGDA_FAUST_BACKEND=wasm
cmake --build build-sunroom --target magda_daw_app magda_cli -j 2
mkdir -p artifacts
clang++ -std=c++20 -O2 -I. tests/sunroom/theory_test.cpp -o artifacts/sunroom-theory-test
artifacts/sunroom-theory-test
python3 tests/sunroom/test_mlx_boundary.py
```

The CLI's `sunroom-journey <mood> <root> <bars> <bpm>` command calls the same composer as the GUI and verifies undo/redo counts and track identities. The inherited CLI renderer supports reproducible offline exports. See `artifacts` for the actual validation evidence from this build.

The pinned Tracktion dependency has a small, reproducible patch in `patches/tracktion-release-assertions.patch`: diagnostic prefetch state is retained when assertions are logged in a Release build. CMake applies or verifies that patch without changing the upstream submodule revision.
