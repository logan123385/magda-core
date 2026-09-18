#!/usr/bin/env python3
"""P2 beginner gate: offline Fixture A insert, undo/redo, save/reopen."""
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
qa = ROOT / "artifacts/beginner-p2"
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
    raise AssertionError(f"{name}: missing dump-json\n{stdout[-500:]}")


def named_tracks(doc: dict) -> dict:
    return {t["name"]: t for t in doc.get("tracks") or []}


blank = saved(call("01-init", "init", qa / "Blank.mgd"))
composed = saved(
    call("02-fixture-a", "exec", blank, "fixture-a", "--out", qa / "FixtureA.mgd")
)
doc = dump_project("02b-dump", composed)
tracks = named_tracks(doc)
assert doc.get("tempo") == 100.0
assert doc.get("keyRoot") == 9
assert doc.get("keyQuality") == "minor"
assert doc.get("loopEndBeats") == 32.0
for name in ("Drums", "Bass", "Chords"):
    assert name in tracks, f"missing track {name}: {list(tracks)}"
    assert tracks[name].get("clips"), f"{name} has no clips"

drums_notes = tracks["Drums"]["clips"][0].get("notes") or []
assert any(n["note"] == 24 for n in drums_notes), "kick pad note 24 missing"
assert any(n["note"] == 25 for n in drums_notes), "snare pad note 25 missing"
assert any(n["note"] == 26 for n in drums_notes), "hat pad note 26 missing"

bass_notes = tracks["Bass"]["clips"][0].get("notes") or []
assert {n["note"] for n in bass_notes} >= {45, 41, 48, 43}

chord_notes = tracks["Chords"]["clips"][0].get("notes") or []
assert {57, 60, 64}.issubset({n["note"] for n in chord_notes})

undone = saved(
    call(
        "03-undo",
        "exec",
        blank,
        "fixture-a",
        "undo",
        "--out",
        qa / "FixtureA-undone.mgd",
    )
)
undone_doc = dump_project("03b-dump", undone)
assert not ({"Drums", "Bass", "Chords"} & set(named_tracks(undone_doc))), named_tracks(undone_doc)

redone = saved(
    call(
        "04-redo",
        "exec",
        blank,
        "fixture-a",
        "undo",
        "redo",
        "--out",
        qa / "FixtureA-redone.mgd",
    )
)
redone_doc = dump_project("04b-dump", redone)
assert set(named_tracks(redone_doc)) >= {"Drums", "Bass", "Chords"}

reopened = saved(call("05-roundtrip", "run", redone, "--out", qa / "FixtureA-reopened.mgd"))
reopened_doc = dump_project("05b-dump", reopened)
assert reopened_doc.get("tempo") == 100.0
assert set(named_tracks(reopened_doc)) >= {"Drums", "Bass", "Chords"}

out = {
    "steps": records,
    "composed": str(composed),
    "undone": str(undone),
    "redone": str(redone),
    "reopened": str(reopened),
    "track_names": sorted(named_tracks(doc)),
}
(qa / "result.json").write_text(json.dumps(out, indent=2))
print(json.dumps(out, indent=2))
print("P2 Fixture A gate passed", flush=True)
