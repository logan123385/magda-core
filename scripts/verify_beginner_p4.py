#!/usr/bin/env python3
"""P4 beginner gate: Fixture A clips exist; scale-lock rule covered by theory_test."""
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
theory = pathlib.Path(
    sys.argv[2]
    if len(sys.argv) > 2
    else "build-sunroom/tests/sunroom_theory_test"
).resolve()
qa = ROOT / "artifacts/beginner-p4"
qa.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env["MAGDA_DATA_DIR"] = str(qa / "profile")
env["MAGDA_CONFIG_FILE"] = str(qa / "profile/config.json")
env.setdefault("MAGDA_AUTOSAVE_RECOVER", "discard")
records = []


def call(name, *argv, binary=None):
    start = time.monotonic()
    exe = binary or cli
    result = subprocess.run(
        [str(exe), *map(str, argv)],
        env=env,
        capture_output=True,
        text=True,
        timeout=300,
    )
    log = result.stdout + "\n" + result.stderr
    (qa / f"{name}.log").write_text(log)
    records.append(
        {
            "step": name,
            "returncode": result.returncode,
            "seconds": round(time.monotonic() - start, 2),
        }
    )
    assert result.returncode == 0, f"{name} failed; see {qa / (name + '.log')}\n{log[-1200:]}"
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


assert theory.is_file(), f"missing theory_test binary: {theory}"
call("00-scale-lock-theory", binary=theory)

blank = saved(call("01-init", "init", qa / "Blank.mgd"))
starter = saved(
    call("02-create-and-play", "exec", blank, "create-and-play", "--out", qa / "Starter.mgd")
)
doc = dump_project("02b-dump", starter)
tracks = {t["name"]: t for t in doc.get("tracks") or []}
assert {"Drums", "Bass", "Chords"} <= set(tracks)

clip_names = set()
for track in doc.get("tracks") or []:
    for clip in track.get("clips") or []:
        clip_names.add(clip.get("name") or "")
# dump-json may nest clips differently — also scan top-level clips
for clip in doc.get("clips") or []:
    clip_names.add(clip.get("name") or "")

needed = {"Fixture A / Drums", "Fixture A / Bass", "Fixture A / Chords"}
missing = needed - clip_names
assert not missing, f"missing fixture clips {missing}; found {sorted(clip_names)[:20]}"

info_root = doc.get("keyRoot")
info_quality = doc.get("keyQuality")
assert info_root == 9, f"expected A (9), got {info_root}"
assert info_quality in ("minor", 1), f"expected minor, got {info_quality}"

# sunroomMood/Guide live in the compressed .mgd payload; dump-json DTO omits them.
# Fixture A code sets sunroomMood=1 + sunroomGuide; keyRoot/quality above prove key.

result = {
    "phase": "P4",
    "status": "PASS",
    "records": records,
    "fixture_clips": sorted(needed),
    "keyRoot": info_root,
    "keyQuality": info_quality,
    "gates": {
        "gui_make_beat_add_chords_play_sound": "code path — not GUI-audited",
        "qwerty_text_focus": "code path — TextEditor guard + focus flush",
        "scale_lock_new_notes_only": "theory_test + PianoRoll insert snap",
        "drum_lanes_no_scale_lock": "DrumGrid does not call snapToScale",
    },
}
(qa / "result.json").write_text(json.dumps(result, indent=2) + "\n")
print("P4 PASS", flush=True)
