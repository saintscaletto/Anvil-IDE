#!/usr/bin/env python3
"""Generate a CycloneDX 1.5 SBOM for the staged Anvil package."""
import hashlib
import json
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STAGE = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build" / "deb" / "root"
OUTPUT = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "dist" / "sbom.json"


def digest(path: Path) -> dict:
    handle = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            handle.update(chunk)
    return {"alg": "SHA-256", "content": handle.hexdigest()}


def file_component(name: str, path: Path, license_id: str = "Apache-2.0") -> dict:
    component = {
        "type": "file",
        "bom-ref": f"file:{name}",
        "name": name,
        "version": "0.0.1",
        "licenses": [{"license": {"id": license_id}}],
    }
    if path.exists():
        component["hashes"] = [digest(path)]
    return component


components = [
    {
        "type": "application",
        "bom-ref": "app:anvil",
        "name": "Anvil IDE",
        "version": "0.0.1",
        "licenses": [{"license": {"id": "Apache-2.0"}}],
        "description": "C++-first IDE with a native core, WebKitGTK shell and Monaco editor.",
    },
    file_component("anvil-shell", STAGE / "usr/bin/anvil"),
    file_component("anvil-core", STAGE / "usr/lib/anvil/anvil-core"),
    file_component("anvil-ui-bundle", STAGE / "usr/share/anvil/ui/assets/index.js"),
    {
        "type": "library",
        "bom-ref": "npm:monaco-editor@0.45.0",
        "name": "monaco-editor",
        "version": "0.45.0",
        "licenses": [{"license": {"id": "MIT"}}],
    },
    {
        "type": "library",
        "bom-ref": "npm:react@18.3.1",
        "name": "react",
        "version": "18.3.1",
        "licenses": [{"license": {"id": "MIT"}}],
    },
    {
        "type": "library",
        "bom-ref": "npm:react-dom@18.3.1",
        "name": "react-dom",
        "version": "18.3.1",
        "licenses": [{"license": {"id": "MIT"}}],
    },
    {
        "type": "library",
        "bom-ref": "npm:zustand@4.5.5",
        "name": "zustand",
        "version": "4.5.5",
        "licenses": [{"license": {"id": "MIT"}}],
    },
    {
        "type": "framework",
        "bom-ref": "pkg:system/webkit2gtk@2.52.6",
        "name": "webkit2gtk-4.1",
        "version": "2.52.6",
        "licenses": [{"license": {"id": "LGPL-2.1-or-later"}}],
        "description": "System WebView runtime (not bundled).",
    },
    {
        "type": "application",
        "bom-ref": "pkg:system/bubblewrap@0.9.0",
        "name": "bubblewrap",
        "version": "0.9.0",
        "licenses": [{"license": {"id": "LGPL-2.1-or-later"}}],
        "description": "Optional system sandbox runtime; language-server isolation uses it.",
    },
    {
        "type": "application",
        "bom-ref": "pkg:system/clangd@18.1.3",
        "name": "clangd",
        "version": "18.1.3",
        "licenses": [{"expression": "Apache-2.0 WITH LLVM-exception"}],
        "description": "External C/C++ language server installed via apt (Recommends, not bundled).",
    },
]

sbom = {
    "bomFormat": "CycloneDX",
    "specVersion": "1.5",
    "serialNumber": f"urn:uuid:{uuid.uuid4()}",
    "version": 1,
    "metadata": {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "tools": [{"vendor": "Anvil", "name": "scripts/gen-sbom.py", "version": "0.0.1"}],
        "component": components[0],
    },
    "components": components[1:],
}

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_text(json.dumps(sbom, indent=2) + "\n", encoding="utf-8")
print(f"wrote {OUTPUT}")
