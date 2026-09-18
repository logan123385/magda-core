#!/usr/bin/env python3
"""P1 beginner gate: grouped journey undo/redo via UndoManager, save/reopen, failed overwrite."""
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
qa = ROOT / "artifacts/beginner-p1"
qa.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env["MAGDA_DATA_DIR"] = str(qa / "profile")
env["MAGDA_CONFIG_FILE"] = str(qa / "profile/config.json")
env.setdefault("MAGDA_AUTOSAVE_RECOVER", "discard")
records = []


def call(name, *argv, env_override=None):
    start = time.monotonic()
    result = subprocess.run(
        [str(cli), *map(str, argv)],
        env=env_override or env,
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
    assert result.returncode == 0, f"{name} failed; see {qa / (name + '.log')}\n{log[-800:]}"
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
    # dump-json is printed before the Saved line
    payload = None
    for line in stdout.splitlines():
        line = line.strip()
        if line.startswith("{") and '"tracks"' in line:
            payload = json.loads(line)
            break
    assert payload is not None, f"{name}: missing dump-json payload\n{stdout[-500:]}"
    return payload


def music_fingerprint(doc: dict) -> dict:
    """Stable musical identity; ignore project display name (follows save path)."""
    tracks = []
    for track in doc.get("tracks", []):
        clips = []
        for clip in track.get("clips", []):
            clips.append(
                {
                    "type": clip.get("type"),
                    "view": clip.get("view"),
                    "startBeat": clip.get("startBeat"),
                    "lengthBeats": clip.get("lengthBeats"),
                    "notes": clip.get("notes") or [],
                }
            )
        tracks.append(
            {
                "name": track.get("name"),
                "volume": track.get("volume"),
                "clips": clips,
            }
        )
    return {
        "tempo": doc.get("tempo"),
        "timeSignatureNumerator": doc.get("timeSignatureNumerator"),
        "timeSignatureDenominator": doc.get("timeSignatureDenominator"),
        "keyRoot": doc.get("keyRoot"),
        "keyQuality": doc.get("keyQuality"),
        "loopEnabled": doc.get("loopEnabled"),
        "loopStartBeats": doc.get("loopStartBeats"),
        "loopEndBeats": doc.get("loopEndBeats"),
        "tracks": tracks,
    }


def clip_count(doc: dict) -> int:
    return sum(len(t.get("clips") or []) for t in doc.get("tracks") or [])


blank = saved(call("01-init", "init", qa / "Blank.mgd"))
blank_fp = music_fingerprint(dump_project("01b-blank-dump", blank))

# Journey runs through UndoManager and verifies undo/redo internally, leaving music applied.
composed = saved(
    call(
        "02-compose",
        "exec",
        blank,
        "sunroom-journey",
        0,
        2,
        8,
        84,
        "--out",
        qa / "Idea.mgd",
    )
)
composed_fp = music_fingerprint(dump_project("02b-compose-dump", composed))
assert clip_count({"tracks": composed_fp["tracks"]}) > 0, "compose produced no clips"
assert composed_fp != blank_fp, "compose did not change musical state"

# Same-session undo removes the grouped insertion before save.
undone = saved(
    call(
        "03-undo",
        "exec",
        blank,
        "sunroom-journey",
        0,
        2,
        8,
        84,
        "undo",
        "--out",
        qa / "Idea-undone.mgd",
    )
)
undone_fp = music_fingerprint(dump_project("03b-undo-dump", undone))
assert undone_fp == blank_fp, "grouped undo did not restore blank musical state"

# Same-session undo then redo restores the grouped insertion.
redone = saved(
    call(
        "04-redo",
        "exec",
        blank,
        "sunroom-journey",
        0,
        2,
        8,
        84,
        "undo",
        "redo",
        "--out",
        qa / "Idea-redone.mgd",
    )
)
redone_fp = music_fingerprint(dump_project("04b-redo-dump", redone))
assert redone_fp == composed_fp, "grouped redo did not restore composed musical state"

reopened = saved(call("05-roundtrip", "run", redone, "--out", qa / "Idea-reopened.mgd"))
reopened_fp = music_fingerprint(dump_project("05b-roundtrip-dump", reopened))
assert reopened_fp == redone_fp, "save/reopen changed musical state"

# Failed overwrite must not clobber a good file.
prior = redone.read_bytes()
redone.chmod(0o444)
fail = subprocess.run(
    [str(cli), "run", str(redone), "--out", str(redone)],
    env=env,
    capture_output=True,
    text=True,
    timeout=120,
)
redone.chmod(0o644)
(qa / "06-readonly-save.log").write_text(fail.stdout + "\n" + fail.stderr)
assert fail.returncode != 0, "read-only overwrite unexpectedly succeeded"
assert redone.read_bytes() == prior, "read-only overwrite corrupted the prior project"
assert "not writable" in (fail.stdout + fail.stderr).lower() or "permission" in (
    fail.stdout + fail.stderr
).lower(), fail.stdout + fail.stderr
records.append({"step": "06-readonly-preserve", "returncode": fail.returncode, "preserved": True})
print("06-readonly-preserve passed", flush=True)

# Headless autosave recovery via MAGDA_AUTOSAVE_RECOVER=recover.
sidecar = redone.with_name(redone.name + ".autosave")
sidecar.write_bytes(undone.read_bytes())
os.utime(sidecar, None)
env_recover = env.copy()
env_recover["MAGDA_AUTOSAVE_RECOVER"] = "recover"
recovered = saved(
    call(
        "07-autosave-recover",
        "run",
        redone,
        "--out",
        qa / "Idea-recovered.mgd",
        env_override=env_recover,
    )
)
assert recovered.is_file()
recovered_fp = music_fingerprint(dump_project("07b-recover-dump", recovered))
assert recovered_fp == undone_fp, "autosave recovery did not load the sidecar"

out = {
    "steps": records,
    "composed": str(composed),
    "undone": str(undone),
    "redone": str(redone),
    "reopened": str(reopened),
    "recovered": str(recovered),
    "clip_counts": {
        "blank": clip_count({"tracks": blank_fp["tracks"]}),
        "composed": clip_count({"tracks": composed_fp["tracks"]}),
        "undone": clip_count({"tracks": undone_fp["tracks"]}),
        "redone": clip_count({"tracks": redone_fp["tracks"]}),
        "reopened": clip_count({"tracks": reopened_fp["tracks"]}),
        "recovered": clip_count({"tracks": recovered_fp["tracks"]}),
    },
}
(qa / "result.json").write_text(json.dumps(out, indent=2))
print(json.dumps(out, indent=2))
print("P1 beginner persistence gate passed", flush=True)
