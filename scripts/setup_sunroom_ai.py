#!/usr/bin/env python3
"""Download the pinned local model and register this private MLX runtime."""
from pathlib import Path
import json
import sys
from huggingface_hub import snapshot_download

config_dir = Path.home() / "Library" / "SUNROOM"
revision = "0e7ffd5c629ef7719d4cbc04069232580bfa9d9c"
model = config_dir / "Models" / "Qwen3.5-4B-4bit"
snapshot_download("mlx-community/Qwen3.5-4B-4bit", revision=revision, local_dir=model,
                  allow_patterns=["*.json", "*.jinja", "*.safetensors"])
config_dir.mkdir(parents=True, exist_ok=True)
config = {"python": sys.executable, "model": str(model), "revision": revision,
          "model_id": "mlx-community/Qwen3.5-4B-4bit"}
(config_dir / "sunroom-ai.json").write_text(json.dumps(config, indent=2) + "\n")
(config_dir / "sunroom-ai.json").chmod(0o600)
print("SUNROOM MLX ready:", model)
