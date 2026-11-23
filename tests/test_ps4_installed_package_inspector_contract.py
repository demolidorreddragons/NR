#!/usr/bin/env python3
"""Contract for offline inspection of PS4 installed/staged PKG evidence."""
from pathlib import Path

workflow = Path(".github/workflows/ps4-inspect-installed-pkgs.yml").read_text(encoding="utf-8")
script = Path("tools/inspect_ps4_installed_packages.ps1").read_text(encoding="utf-8")
collector = Path("tools/ps4-control-plane-preflight.ps1").read_text(encoding="utf-8")

assert 'workflows: ["PS4 Control Plane Preflight"]' in workflow
assert "types: [completed]" in workflow
assert "run-id: ${{ github.event.workflow_run.id }}" in workflow
assert "name: ps4-control-plane-preflight-${{ github.event.workflow_run.id }}" in workflow
assert "3de6da849b64a8ad0d5ba00dd4e8919934b868b0" in workflow
assert "actions: read" in workflow and "contents: read" in workflow
assert "retention-days: 30" in workflow

# This inspector must only consume the preserved artifact; no console/network client is allowed.
for forbidden in ("FtpWebRequest", "HttpWebRequest", "curl.exe", "Invoke-WebRequest", "UploadFile", "DeleteFile", "RenameTo", "POST"):
    assert forbidden not in script, forbidden
assert "control-plane-preflight.json" in script
assert "Get-EvidenceEntry" in script
assert "dotnet $env:PKGTOOL validate" in script
assert "dotnet $env:PKGTOOL extract" in script
assert "dotnet $env:PKGTOOL sfo read" in script
assert "installed_matches_app_json_source" in script
assert "staged_packages" in script
assert "installed_app_prx_matches_goldhen_prx" in script
assert "not_assumed_to_be_raw_file_sha256" in script
assert 'hardware_validation="not_performed"' in script

# The preflight collector provides the exact package and PRX files used by the offline inspector.
for path in (
    "/user/app/PNEU00001/app.pkg",
    "/user/data/pkg/PS4-Neural-Engine-015.pkg",
    "/data/pkg/PS4-Neural-Engine-015.pkg",
    "/data/GoldHEN/plugins/ps4_neural_engine.prx",
):
    assert path in collector
assert "preflight_run_id=$env:GITHUB_RUN_ID" in collector
print("Installed/staged package offline inspection contract passed")
