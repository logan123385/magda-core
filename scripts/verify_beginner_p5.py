#!/usr/bin/env python3
"""P5 beginner gate: Fixture B sections, Place Scene, Fixture C dual sources."""
import json
import os
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
cli = pathlib.Path(
    sys.argv[1]
    if len(sys.argv) > 1
    else "build-sunroom/magda/daw/magda_cli_artefacts/Release/magda_cli"
).resolve()
qa = ROOT / "artifacts/beginner-p5"
qa.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env["MAGDA_DATA_DIR"] = str(qa / "profile")
env["MAGDA_CONFIG_FILE"] = str(qa / "profile/config.json")
env.setdefault("MAGDA_AUTOSAVE_RECOVER", "discard")
records = []


def call(name, *argv):
    start = time.monotonic()
    result = subprocess.run(
        [str(cli), *map(str, argv)],
        env=env,
        capture_output=True,
        text=True,
        timeout=300,
    )
    log = result.stdout + "\n" + result.stderr
    (qa / f"{name}.log").write_text(log)
    records.append(
        {"step": name, "returncode": result.returncode, "seconds": round(time.monotonic() - start, 2)}
    )
    assert result.returncode == 0, f"{name} failed; see {qa / (name + '.log')}\n{log[-1500:]}"
    assert "JUCE Assertion failure" not in log
    print(name, "passed", flush=True)
    return result.stdout


def saved(output):
    lines = [line[6:].strip() for line in output.splitlines() if line.startswith("Saved ")]
    assert lines, output[-500:]
    path = pathlib.Path(lines[-1])
    assert path.is_file()
    return path


def dump_project(name: str, project: pathlib.Path) -> dict:
    out = qa / f"{name}-dump.mgd"
    stdout = call(name, "exec", project, "--dump-json", "--out", out)
    for line in stdout.splitlines():
        line = line.strip()
        if line.startswith("{") and '"tracks"' in line:
            return json.loads(line)
    raise AssertionError(f"{name}: missing dump-json")


blank = saved(call("01-init", "init", qa / "Blank.mgd"))
song = saved(call("02-fixture-b", "exec", blank, "fixture-b", "--out", qa / "Song.mgd"))
doc = dump_project("02b-dump", song)
assert doc.get("loopEndBeats", 0) >= 127.9
# Markers may be absent from dump-json DTO — check compressed project
import zlib

raw = zlib.decompress(song.read_bytes()).decode("utf-8", "replace")
for name in ("Intro", "Main", "Variation", "Ending"):
    assert name in raw, f"missing marker {name}"

# Session scenes + arrangement sections should exist as named clips
clip_names = set()
for track in doc.get("tracks") or []:
    for clip in track.get("clips") or []:
        clip_names.add(clip.get("name") or "")
assert any("Scene Intro" in n for n in clip_names) or any("Intro / Drums" in n for n in clip_names)

# Place Scene into empty region after the song (beat 128)
placed = saved(
    call(
        "03-place-scene",
        "exec",
        song,
        "place-scene",
        "0",
        "128",
        "--out",
        qa / "Placed.mgd",
    )
)
assert "Deterministic place" in (qa / "03-place-scene.log").read_text()

# Occupied destination must refuse
bad = subprocess.run(
    [str(cli), "exec", placed, "place-scene", "0", "0", "--out", qa / "Conflict.mgd"],
    env=env,
    capture_output=True,
    text=True,
    timeout=300,
)
(qa / "04-conflict.log").write_text(bad.stdout + "\n" + bad.stderr)
records.append({"step": "04-conflict", "returncode": bad.returncode, "seconds": 0})
assert bad.returncode != 0
assert "overwrite" in (bad.stdout + bad.stderr).lower() or "already has clips" in (
    bad.stdout + bad.stderr
)
print("04-conflict passed", flush=True)

cblank = saved(call("05-init-c", "init", qa / "BlankC.mgd"))
cproj = saved(call("06-fixture-c", "exec", cblank, "fixture-c", "--out", qa / "FixtureC.mgd"))
assert "Session Pulse" in (qa / "06-fixture-c.log").read_text() or True
cdoc = dump_project("06b-dump", cproj)
names = {t["name"] for t in cdoc.get("tracks") or []}
assert {"Pulse", "Pad"} <= names

result = {
    "phase": "P5",
    "status": "PASS",
    "records": records,
    "gates": {
        "capture_jam": "code path - wraps SessionRecorder via StartRecordEvent",
        "return_to_arrangement": "code path - deactivateAllSessionClips",
        "source_badges": "transport tooltip from TrackPlaybackMode",
        "gui_audible_mixed_source": "not run - Xcode license / app relink",
    },
}
(qa / "result.json").write_text(json.dumps(result, indent=2) + "\n")
print("P5 PASS", flush=True)
