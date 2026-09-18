#!/usr/bin/env python3
"""P3 beginner gate: Create and Play inserts Fixture A once; second call is idempotent."""
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
qa = ROOT / "artifacts/beginner-p3"
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


blank = saved(call("01-init", "init", qa / "Blank.mgd"))
first = saved(
    call("02-create-and-play", "exec", blank, "create-and-play", "--out", qa / "Starter.mgd")
)
doc = dump_project("02b-dump", first)
names = {t["name"] for t in doc.get("tracks") or []}
assert {"Drums", "Bass", "Chords"} <= names
assert "Added drums, bass and chords" in (qa / "02-create-and-play.log").read_text()

# Same session: create-and-play then create-and-play again must not double tracks.
double = saved(
    call(
        "03-idempotent",
        "exec",
        blank,
        "create-and-play",
        "create-and-play",
        "--out",
        qa / "Starter-once.mgd",
    )
)
double_doc = dump_project("03b-dump", double)
for name in ("Drums", "Bass", "Chords"):
    count = sum(1 for track in double_doc.get("tracks") or [] if track.get("name") == name)
    assert count == 1, f"create-and-play duplicated {name}"
assert "no duplicate insert" in (qa / "03-idempotent.log").read_text()

out = {"steps": records, "first": str(first), "idempotent": str(double), "tracks": sorted(names)}
(qa / "result.json").write_text(json.dumps(out, indent=2))
print(json.dumps(out, indent=2))
print("P3 Create and Play gate passed", flush=True)
