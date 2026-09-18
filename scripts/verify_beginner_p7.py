#!/usr/bin/env python3
"""P7 gate: staged DSL proposal. Mocks exercise the real interpreter; not a live model."""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
cli = pathlib.Path(
    sys.argv[1]
    if len(sys.argv) > 1
    else "build-sunroom/magda/daw/magda_cli_artefacts/Release/magda_cli"
).resolve()
qa = ROOT / "artifacts/beginner-p7"
qa.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env["MAGDA_DATA_DIR"] = str(qa / "profile")
env["MAGDA_CONFIG_FILE"] = str(qa / "profile/config.json")
env.setdefault("MAGDA_AUTOSAVE_RECOVER", "discard")
records = []
DSL = 'filter(tracks).track.group(name="All Tracks")'


def clear_out(path):
    path = pathlib.Path(path)
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()
    stem = path.with_suffix("")
    if stem != path and stem.is_dir():
        shutil.rmtree(stem)


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
    assert "JUCE Assertion failure" not in log, log[-800:]
    if expect_ok:
        assert result.returncode == 0, f"{name} failed\n{log[-1500:]}"
    print(name, "passed", flush=True)
    return result


def saved(output):
    lines = [line[6:].strip() for line in output.splitlines() if line.startswith("Saved ")]
    assert lines, output[-500:]
    path = pathlib.Path(lines[-1])
    assert path.is_file()
    return path


def dump_project(name: str, project: pathlib.Path) -> dict:
    out = qa / f"{name}-dump.mgd"
    result = run(name, "exec", project, "--dump-json", "--out", out)
    for line in result.stdout.splitlines():
        line = line.strip()
        if line.startswith("{") and '"tracks"' in line:
            return json.loads(line)
    raise AssertionError(f"{name}: missing dump-json")


def names(doc):
    return {t.get("name") for t in doc.get("tracks") or []}


blank = saved(run("01-init", "init", qa / "Blank.mgd").stdout)
starter = saved(run("02-fixture-a", "exec", blank, "fixture-a", "--out", qa / "Starter.mgd").stdout)
starter_bytes = starter.read_bytes()

applied = run(
    "03-apply",
    "exec",
    starter,
    "propose-dsl",
    DSL,
    "apply-proposal",
    "--out",
    qa / "Grouped.mgd",
)
assert "Proposal " in applied.stdout
assert "Applied:" in applied.stdout
gdoc = dump_project("03b-dump", saved(applied.stdout))
assert "All Tracks" in names(gdoc)
assert "Drums" in names(gdoc)

undone = run(
    "04-undo",
    "exec",
    starter,
    "propose-dsl",
    DSL,
    "apply-proposal",
    "undo",
    "--out",
    qa / "Ungrouped.mgd",
)
assert "Apply suggestion" in undone.stdout
udoc = dump_project("04b-dump", saved(undone.stdout))
assert "All Tracks" not in names(udoc)
assert "Drums" in names(udoc)

clear_out(qa / "Invalid.mgd")
bad = run(
    "05-invalid",
    "exec",
    starter,
    "propose-dsl",
    "not dsl",
    "apply-proposal",
    "--out",
    qa / "Invalid.mgd",
    expect_ok=False,
)
assert bad.returncode != 0
assert "Refused" in (bad.stdout + bad.stderr)
assert not (qa / "Invalid.mgd").exists() and not (qa / "Invalid").exists()

clear_out(qa / "Stale.mgd")
stale = run(
    "06-stale",
    "exec",
    starter,
    "propose-dsl",
    DSL,
    "bump-revision",
    "apply-proposal",
    "--out",
    qa / "Stale.mgd",
    expect_ok=False,
)
assert stale.returncode != 0
assert "project changed" in (stale.stdout + stale.stderr).lower()
assert not (qa / "Stale.mgd").exists() and not (qa / "Stale").exists()

clear_out(qa / "Selection.mgd")
sel = run(
    "07-selection",
    "exec",
    starter,
    "propose-dsl",
    DSL,
    "select-track",
    "99",
    "apply-proposal",
    "--out",
    qa / "Selection.mgd",
    expect_ok=False,
)
assert sel.returncode != 0
assert "selection changed" in (sel.stdout + sel.stderr).lower()
assert not (qa / "Selection.mgd").exists() and not (qa / "Selection").exists()

clear_out(qa / "Canceled.mgd")
cancel = run(
    "08-cancel",
    "exec",
    starter,
    "propose-dsl",
    DSL,
    "apply-proposal",
    "cancelled",
    "--out",
    qa / "Canceled.mgd",
    expect_ok=False,
)
assert cancel.returncode != 0
assert "canceled" in (cancel.stdout + cancel.stderr).lower()
assert not (qa / "Canceled.mgd").exists() and not (qa / "Canceled").exists()

clear_out(qa / "Twice.mgd")
twice = run(
    "09-double",
    "exec",
    starter,
    "propose-dsl",
    DSL,
    "apply-proposal",
    "apply-proposal",
    "--out",
    qa / "Twice.mgd",
    expect_ok=False,
)
assert twice.returncode != 0
assert "already applied" in (twice.stdout + twice.stderr).lower()
assert not (qa / "Twice.mgd").exists() and not (qa / "Twice").exists()
assert starter.read_bytes() == starter_bytes

result = {
    "phase": "P7",
    "status": "PASS",
    "records": records,
    "gates": {
        "live_model": "not run - mocks only; P8 owns a real local model",
        "console_still_applies_immediately": "unchanged; beginner CLI stages then applies",
        "mcp_grants": "untouched - model text is not a scope grant",
    },
}
(qa / "result.json").write_text(json.dumps(result, indent=2) + "\n")
print("P7 PASS", flush=True)
