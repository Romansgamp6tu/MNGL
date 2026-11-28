#!/usr/bin/env python3
"""Generate the test consumer's vcpkg registry configuration for the current CI ref."""
import json
import os
import sys
from pathlib import Path

if len(sys.argv) != 2:
    raise SystemExit("usage: configure_registry.py <manifest-root>")

manifest_root = Path(sys.argv[1])
template_path = Path(__file__).with_name("vcpkg-configuration.json.in")
config = json.loads(template_path.read_text(encoding="utf-8"))
registry = next(
    item
    for item in config["registries"]
    if item.get("repository") == "https://github.com/Romansgamp6tu/MNGL"
)
registry["reference"] = os.environ["GITHUB_REF"]
registry["baseline"] = os.environ["GITHUB_SHA"]
(manifest_root / "vcpkg-configuration.json").write_text(
    json.dumps(config, indent=2) + "\n",
    encoding="utf-8",
)
