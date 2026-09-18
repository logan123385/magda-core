#!/usr/bin/env python3
"""P9 gate: starter catalog filters and import without deleting sources."""
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
qa = ROOT / "artifacts/beginner-p9"
qa.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env["MAGDA_DATA_DIR"] = str(qa / "profile")
env["MAGDA_CONFIG_FILE"] = str(qa / "profile/config.json")
env.setdefault("MAGDA_AUTOSAVE_RECOVER", "discard")
records = []
source = ROOT / "resources/sunroom/Sounds/Heartbeat 01.wav"
wav_count = len(list((ROOT / "resources/sunroom/Sounds").glob("*.wav")))


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


def saved_path(result):
    paths = [line[6:].strip() for line in result.stdout.splitlines() if line.startswith("Saved ")]
    assert paths, result.stdout[-400:]
    return pathlib.Path(paths[-1])


def project_json(result):
    for line in result.stdout.splitlines():
        if line.startswith("{") and '"tracks"' in line:
            return json.loads(line)
    raise AssertionError(result.stdout[-800:])


def track_names(doc):
    return {t.get("name") for t in doc.get("tracks") or []}


blank = None
init = run("01-init", "init", qa / "Blank.mgd")
for line in init.stdout.splitlines():
    if line.startswith("Saved "):
        blank = pathlib.Path(line[6:].strip())
assert blank and blank.is_file()

query = run(
    "02-filters",
    "exec",
    blank,
    "library-query",
    "kick",
    "-",
    "-",
    "-",
    "-",
    "library-query",
    "kick",
    "horizon",
    "-",
    "-",
    "-",
    "library-query",
    "-",
    "zzz",
    "-",
    "-",
    "-",
    "library-query",
    "-",
    "-",
    "2",
    "-",
    "-",
    "library-query",
    "pitched",
    "-",
    "2",
    "-",
    "-",
    "library-query",
    "-",
    "-",
    "-",
    "90",
    "110",
    "--out",
    qa / "Query.mgd",
)
text = query.stdout
assert "not guessed" in text
assert "not Media Library semantic search" in text
assert "Time-stretch and transpose are not applied" in text
blocks = text.split("Matches ")
assert len(blocks) >= 7, text[-500:]
def head(block):
    return block.splitlines()[0]

kick, empty_combo, empty_text, key_all, pitched, bpm = [head(b) for b in blocks[1:7]]
assert kick.startswith("8 "), kick
assert "unknown-bpm 0" in kick and "unknown-key 0" in kick
assert "Heartbeat 01.wav" in text
assert "pitched=declared-no" in text
assert "bpm=unknown" in text
assert "source=declared" in text
assert empty_combo.startswith("0 "), empty_combo
assert "unknown-bpm 0 unknown-key 0 unpitched-skipped 0" in empty_combo
assert empty_text.startswith("0 "), empty_text
assert "unpitched-skipped 0" in empty_text
assert key_all.startswith("0 "), key_all
assert "unpitched-skipped 40" in key_all
assert "unknown-key 8" in key_all
assert pitched.startswith("0 "), pitched
assert "unknown-key 8" in pitched
assert "unpitched-skipped 0" in pitched
assert bpm.startswith("0 "), bpm
assert "unknown-bpm 48" in bpm

missing = run(
    "03-missing",
    "exec",
    blank,
    "add-sample",
    "Missing.wav",
    "--out",
    qa / "Missing.mgd",
    expect_ok=False,
)
assert missing.returncode != 0
assert "Nothing was added" in (missing.stdout + missing.stderr)

escape = run(
    "04-escape",
    "exec",
    blank,
    "add-sample",
    "../LICENSE.txt",
    "--out",
    qa / "Escape.mgd",
    expect_ok=False,
)
assert escape.returncode != 0
assert "Nothing was added" in (escape.stdout + escape.stderr)

imported = run(
    "05-import",
    "exec",
    blank,
    "add-sample",
    "Heartbeat 01.wav",
    "dump",
    "--json",
    "--out",
    qa / "Imported.mgd",
)
assert "Added Heartbeat 01 source=" in imported.stdout
assert "stretch=not-applied" in imported.stdout
assert "Heartbeat 01" in track_names(project_json(imported))
imported_path = saved_path(imported)
assert source.is_file()

reopen = run("06-reopen", "exec", imported_path, "dump", "--json", "--out", qa / "Reopen.mgd")
assert "Heartbeat 01" in track_names(project_json(reopen))

undone = run(
    "07-undo",
    "exec",
    blank,
    "add-sample",
    "Heartbeat 01.wav",
    "undo",
    "dump",
    "--json",
    "--out",
    qa / "Undone.mgd",
)
assert "Heartbeat 01" not in track_names(project_json(undone))
assert source.is_file()

redone = run(
    "08-redo",
    "exec",
    blank,
    "add-sample",
    "Heartbeat 01.wav",
    "undo",
    "redo",
    "dump",
    "--json",
    "--out",
    qa / "Redone.mgd",
)
assert "Heartbeat 01" in track_names(project_json(redone))
assert source.is_file()
assert len(list((ROOT / "resources/sunroom/Sounds").glob("*.wav"))) == wav_count

result = {
    "phase": "P9",
    "status": "PASS",
    "records": records,
    "gates": {
        "filters": "kind and text AND together; unknown BPM/key excluded and counted, not guessed",
        "drums": "key filter skips declared-unpitched sounds; pitched bells stay unknown-key",
        "import": "add-sample creates an audio track; missing and escaped paths add nothing",
        "undo_source": "undo/redo keep the starter WAV; no cache sweep",
        "semantic_search": "unchanged Media Library; this shelf is the offline manifest",
        "audition": "not run - no separate preview player; click still imports",
        "time_stretch": "not applied",
        "gui": "not run",
    },
}
(qa / "result.json").write_text(json.dumps(result, indent=2) + "\n")
print("P9 PASS", flush=True)
