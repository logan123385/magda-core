#!/usr/bin/env python3
"""P6 beginner gate: Shared Space return reuse + send amount; Mix entry is code path."""
import json
import os
import pathlib
import subprocess
import sys
import time
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
cli = pathlib.Path(
    sys.argv[1]
    if len(sys.argv) > 1
    else "build-sunroom/magda/daw/magda_cli_artefacts/Release/magda_cli"
).resolve()
qa = ROOT / "artifacts/beginner-p6"
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
song = saved(call("02-fixture-a", "exec", blank, "fixture-a", "--out", qa / "Starter.mgd"))
spaced = saved(
    call("03-space-return", "exec", song, "space-return", "0.4", "--out", qa / "Spaced.mgd")
)
assert "send amount" in (qa / "03-space-return.log").read_text().lower()

doc = dump_project("03b-dump", spaced)
aux = [t for t in doc.get("tracks") or [] if t.get("type") == "aux" or t.get("name") == "Shared Space"]
assert len(aux) == 1, f"expected one Shared Space aux, got {len(aux)}"
aux_id = aux[0]["id"]
devices = aux[0].get("devices") or []
assert any((d.get("pluginId") or "") == "magda_reverb" for d in devices), devices

senders = 0
for track in doc.get("tracks") or []:
    for send in track.get("sends") or []:
        if send.get("destTrackId") == aux_id:
            senders += 1
            assert abs(float(send.get("level", -1)) - 0.4) < 0.02, send
assert senders >= 3, f"expected sends from starter tracks, got {senders}"

# Second apply must reuse the same Aux (no duplicate returns)
again = saved(
    call("04-space-reuse", "exec", spaced, "space-return", "0.55", "--out", qa / "Spaced2.mgd")
)
doc2 = dump_project("04b-dump", again)
aux2 = [t for t in doc2.get("tracks") or [] if t.get("type") == "aux" or t.get("name") == "Shared Space"]
assert len(aux2) == 1, f"duplicate Shared Space returns: {len(aux2)}"
for track in doc2.get("tracks") or []:
    for send in track.get("sends") or []:
        if send.get("destTrackId") == aux2[0]["id"]:
            assert abs(float(send.get("level", -1)) - 0.55) < 0.02, send
raw = zlib.decompress(again.read_bytes()).decode("utf-8", "replace")
assert raw.count('"name": "Shared Space"') <= 2  # track + device name

# Undo in the same process as apply (history does not survive save/reopen)
undone = saved(
    call(
        "05-undo",
        "exec",
        song,
        "space-return",
        "0.35",
        "undo",
        "--out",
        qa / "Undone.mgd",
    )
)
assert "Undid" in (qa / "05-undo.log").read_text()
doc_u = dump_project("05b-dump", undone)
aux_u = [t for t in doc_u.get("tracks") or [] if t.get("name") == "Shared Space" or t.get("type") == "aux"]
assert len(aux_u) == 0, "undo should remove the created Shared Space return"

result = {
    "phase": "P6",
    "status": "PASS",
    "records": records,
    "gates": {
        "open_mix": "code path - ViewMode::Mix via onShowMix",
        "analyze": "code path - MixerToggleRail Analyze unchanged",
        "shared_space_return": "CLI space-return creates/reuses one Aux+magda_reverb",
        "send_not_wetdry": "summary states send amount",
        "gui_audible_mix": "not run - Xcode license / app relink",
    },
}
(qa / "result.json").write_text(json.dumps(result, indent=2) + "\n")
print("P6 PASS", flush=True)
