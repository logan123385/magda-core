#!/usr/bin/env python3
"""Exercise the installed MLX model against the bundled SUNROOM music guide.

This uses only local weights and a private temporary helper. It records replies
for human review; syntactically valid JSON does not prove musical accuracy.
"""
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
config = json.loads((Path.home() / "Library/SUNROOM/sunroom-ai.json").read_text())
guide = (ROOT / "resources/sunroom/knowledge/coach.md").read_text()
context = """Create-page settings: root D, mode Dorian, 84 BPM, 32 bars.
Scale notes: D E F G A B C. Quarter=714.3 ms; dotted eighth=535.7 ms.
Actual project tempo: 84 BPM. Tracks: Velvet sky, Deep current, Light droplets.
"""
questions = [
    ("theory", "In two sentences, explain why D and A work together in D Dorian, and give the dotted-eighth delay time at 84 BPM."),
    ("recipe", "Give me a short recipe with JSON for a warm, floating D Dorian psychill journey at 84 BPM, 32 bars."),
    ("melody", "Compose a sparse, two-bar D Dorian psybient melody. Give the melody JSON for the Note garden with at most six notes."),
]
records = []
output = ROOT / "artifacts/mlx-verified.json"
output.parent.mkdir(parents=True, exist_ok=True)

with tempfile.TemporaryDirectory(prefix="sunroom-ai-check-") as temp:
    ready = Path(temp) / "ready.json"
    log = (ROOT / "artifacts/mlx-verified.log").open("w")
    worker = subprocess.Popen([
        config["python"], str(ROOT / "resources/sunroom/mlx_server.py"),
        "--model", config["model"], "--ready-file", str(ready),
        "--parent-pid", str(os.getpid()),
    ], stdout=log, stderr=log)
    def load_ready(path: Path) -> dict:
        """Retry until the atomic ready file has a usable port and token."""
        last_error = None
        for _ in range(50):
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
                if isinstance(data.get("port"), int) and isinstance(data.get("token"), str) and data["token"]:
                    return data
            except (OSError, json.JSONDecodeError, TypeError, ValueError) as err:
                last_error = err
            time.sleep(0.05)
        raise RuntimeError(f"MLX ready file incomplete: {last_error}")

    try:
        # Cold starts under memory pressure can exceed 30s before the helper is ready.
        deadline = time.monotonic() + 90
        while not ready.exists():
            if worker.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError("MLX helper did not start")
            time.sleep(.1)
        endpoint = load_ready(ready)
        for label, question in questions:
            payload = json.dumps({"messages": [
                {"role": "system", "content": guide + "\n" + context},
                {"role": "user", "content": question},
            ], "max_tokens": 700, "temperature": .5}).encode()
            request = urllib.request.Request(
                f"http://127.0.0.1:{endpoint['port']}/v1/chat/completions",
                data=payload,
                headers={"Content-Type": "application/json",
                         "Authorization": "Bearer " + endpoint["token"]})
            with urllib.request.urlopen(request, timeout=300) as response:
                result = json.load(response)
            answer = result["choices"][0]["message"]["content"]
            record = {"case": label, "question": question, "answer": answer,
                      "metrics": result["sunroom_metrics"]}
            if label in ("recipe", "melody"):
                fence = re.search(r"```(?:json)?\s*(\{.*?\})\s*```", answer, re.S)
                blob = fence.group(1) if fence else answer[answer.index("{"):answer.rindex("}") + 1]
                blob = re.sub(r",\s*}", "}", blob)
                blob = re.sub(r",\s*]", "]", blob)
                proposal = json.loads(blob)
                assert proposal["mood"] == 0 and proposal["root"] == 2, proposal
                if label == "recipe":
                    assert proposal["tempo"] == 84 and proposal["bars"] == 32, proposal
                    assert all(0 <= proposal[k] <= 1 for k in ("motion", "space", "warmth"))
                else:
                    # Mirror the host: drop out-of-grid steps, keep usable cells.
                    kept = [n for n in proposal.get("melody", [])
                            if isinstance(n.get("degree"), int) and isinstance(n.get("step"), int)
                            and 0 <= n["degree"] <= 6 and 0 <= n["step"] <= 15]
                    assert 1 <= len(kept) <= 6, proposal
                    proposal["melody"] = kept
                record["validated_proposal"] = proposal
            records.append(record)
            output.write_text(json.dumps(records, indent=2))
            print(label, result["sunroom_metrics"], answer, flush=True)
    finally:
        worker.terminate()
        try:
            worker.wait(timeout=10)
        except subprocess.TimeoutExpired:
            worker.kill()
            worker.wait()
        log.close()
