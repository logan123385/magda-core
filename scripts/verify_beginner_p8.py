#!/usr/bin/env python3
"""P8 gate: missing local model is labeled honestly; coach DSL stages through P7."""
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
qa = ROOT / "artifacts/beginner-p8"
qa.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env["MAGDA_DATA_DIR"] = str(qa / "profile")
env["MAGDA_CONFIG_FILE"] = str(qa / "profile/config.json")
env.setdefault("MAGDA_AUTOSAVE_RECOVER", "discard")
records = []


def run(name, *argv, expect_ok=True):
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
    assert "JUCE Assertion failure" not in log, log[-600:]
    if expect_ok:
        assert result.returncode == 0, f"{name} failed\n{log[-1200:]}"
    print(name, "passed", flush=True)
    return result


blank = None
init = run("01-init", "init", qa / "Blank.mgd")
for line in init.stdout.splitlines():
    if line.startswith("Saved "):
        blank = pathlib.Path(line[6:].strip())
assert blank and blank.is_file()

status = run("02-coach-status", "exec", blank, "coach-status", "--out", qa / "Status.mgd")
text = status.stdout + status.stderr
assert "not installed" in text.lower()
assert "not an AI answer" in text
assert "sk-" not in text

prose = run(
    "03-prose",
    "exec",
    blank,
    "coach-stage",
    "Try a slower tempo. Your music is unchanged.",
    "--out",
    qa / "Prose.mgd",
    expect_ok=False,
)
assert prose.returncode != 0
assert "no SUNROOM_DSL" in (prose.stdout + prose.stderr)

dsl = 'Please consider this. SUNROOM_DSL: filter(tracks).track.group(name="All Tracks")'
starter = run("04-fixture", "exec", blank, "fixture-a", "--out", qa / "Starter.mgd")
starter_path = [line[6:].strip() for line in starter.stdout.splitlines() if line.startswith("Saved ")][-1]
staged = run(
    "05-stage-apply",
    "exec",
    starter_path,
    "coach-stage",
    dsl,
    "apply-proposal",
    "--out",
    qa / "Applied.mgd",
)
assert "Staged " in staged.stdout
assert "Applied:" in staged.stdout

result = {
    "phase": "P8",
    "status": "PASS",
    "records": records,
    "gates": {
        "live_model": "not run - no weights loaded, no server, no API spend",
        "absent_model": "coach-status says not installed and not an AI answer",
        "apply_boundary": "SUNROOM_DSL stages then apply-proposal; prose does not mutate",
        "before_after": "code path - Hear before/after only undo or redo Apply suggestion",
        "gui_audible": "not run",
    },
}
(qa / "result.json").write_text(__import__("json").dumps(result, indent=2) + "\n")
print("P8 PASS", flush=True)
