"""Exercise the real worker's request boundary without loading model weights."""
import json
import os
import pathlib
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load_ready(path: pathlib.Path, timeout: float = 5.0) -> dict:
    """Retry until the atomic ready file has a usable port and token."""
    deadline = time.monotonic() + timeout
    last_error = None
    while time.monotonic() < deadline:
        if path.exists():
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
                if isinstance(data.get("port"), int) and isinstance(data.get("token"), str) and data["token"]:
                    return data
            except (OSError, json.JSONDecodeError, TypeError, ValueError) as err:
                last_error = err
        time.sleep(0.05)
    raise RuntimeError(f"MLX ready file incomplete: {last_error}")


class Boundary(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        base = pathlib.Path(cls.tmp.name)
        (base / "model").mkdir()
        (base / "model/config.json").write_text("{}")
        cls.ready = base / "ready.json"
        cls.proc = subprocess.Popen(
            [
                sys.executable,
                str(ROOT / "resources/sunroom/mlx_server.py"),
                "--model",
                str(base / "model"),
                "--ready-file",
                str(cls.ready),
                "--parent-pid",
                str(os.getpid()),
            ]
        )
        cls.config = load_ready(cls.ready, timeout=10.0)
        cls.url = f'http://127.0.0.1:{cls.config["port"]}'

    @classmethod
    def tearDownClass(cls):
        cls.proc.terminate()
        cls.proc.wait(timeout=5)
        cls.tmp.cleanup()

    def post(self, payload, authorized=True, origin=False):
        h = {"Content-Type": "application/json"}
        if authorized:
            h["Authorization"] = "Bearer " + self.config["token"]
        if origin:
            h["Origin"] = "https://example.invalid"
        try:
            with urlopen(
                Request(self.url + "/v1/chat/completions", json.dumps(payload).encode(), h),
                timeout=3,
            ) as r:
                return r.status, json.load(r)
        except HTTPError as e:
            return e.code, json.load(e)

    def test_private_credentials(self):
        self.assertEqual(stat.S_IMODE(self.ready.stat().st_mode), 0o600)
        self.assertGreaterEqual(len(self.config["token"]), 40)

    def test_unauthenticated_rejected(self):
        self.assertEqual(self.post({}, False)[0], 401)

    def test_browser_rejected(self):
        self.assertEqual(self.post({}, origin=True)[0], 403)

    def test_invalid_messages_rejected(self):
        for payload in [{}, {"messages": []}, {"messages": [{"role": "tool", "content": "x"}]}]:
            self.assertEqual(self.post(payload)[0], 400)

    def test_code_tools_rejected(self):
        self.assertEqual(
            self.post({"messages": [{"role": "user", "content": "x"}], "tools": [{"name": "shell"}]})[
                0
            ],
            400,
        )

    def test_large_request_rejected(self):
        self.assertEqual(self.post({"messages": "x" * 70000})[0], 413)

    def test_health_does_not_load_model(self):
        with urlopen(self.url + "/health") as r:
            self.assertEqual(json.load(r), {"ready": True, "loaded": False})


if __name__ == "__main__":
    unittest.main()
