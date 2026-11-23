#!/usr/bin/env python3
"""Static safety contract for the PS4 control-plane read-only collector."""
from pathlib import Path
import re

script = Path("tools/ps4-control-plane-preflight.ps1").read_text(encoding="utf-8")
workflow = Path(".github/workflows/ps4-control-plane-preflight.yml").read_text(encoding="utf-8")

# The privileged workflow must use the existing Windows LAN runner and reviewed default-branch source.
assert "runs-on: [self-hosted, Windows, X64, ps4-lan]" in workflow
assert "ref: main" in workflow
assert "id: checkout-collector" in workflow
assert "steps.checkout-collector.outputs.commit" in workflow
assert "COLLECTOR_REF: main" in workflow
assert "shell: powershell -NoProfile -ExecutionPolicy Bypass -File {0}" in workflow
assert "ps4-control-plane-preflight-${{ github.run_id }}" in workflow
assert "GITHUB_RUN_ID" in script and "COLLECTOR_COMMIT" in script and "COLLECTOR_REF" in script
assert "collector_commit=$env:COLLECTOR_COMMIT" in script
assert "collector_ref=$env:COLLECTOR_REF" in script
assert "preflight_run_id=$env:GITHUB_RUN_ID" in script
assert "workflow_run:" in workflow and 'workflows: ["PS4 PRX and PKG build"]' in workflow
assert "timeout-minutes: 15" in workflow
assert r"PS4_IP: ${{ vars.PS4_IP }}" in workflow

# Network target is constrained to a literal IPv4 and all run/provenance identifiers are required.
assert "[System.Net.IPAddress]::TryParse" in script
assert "AddressFamily]::InterNetwork" in script

assert workflow.count("Collect read-only PS4 control-plane inventory") == 1
assert script.count("$root = Join-Path") == 1
assert script.rstrip().endswith('Write-Host "CONTROL_PLANE_READ_ONLY_COMPLETE"')
# Only the three explicitly read-only FTP verbs may be assigned to requests.
ftp_methods = set(re.findall(r"\[System\.Net\.WebRequestMethods\+Ftp\]::(\w+)", script))
assert ftp_methods == {"ListDirectoryDetails", "GetFileSize", "DownloadFile"}, ftp_methods
assert not re.search(r"WebRequestMethods\+Ftp\]::(?:UploadFile|DeleteFile|Rename|MakeDirectory|RemoveDirectory)", script)

# LIST evidence must remain byte-for-byte and must not be decoded/re-encoded.
assert "ListDirectoryDetails" in script
assert "$sourceStream.CopyTo($output)" in script
assert ".raw.bin" in script
listing_section = script.split("# Safe RPI fingerprint:")[0]
assert "StreamReader" not in listing_section
assert "WriteAllText($destination" not in script
assert "[IO.FileMode]::CreateNew" in script
assert "$output.Flush($true)" in script
assert '".partial"' in script and 'result="partial"' in script

# GET snapshots need pre/post remote size, local size/hash, and timestamp provenance.
assert script.count("GetFileSize") >= 2
assert "remote_bytes_before=$remoteBefore" in script
assert "remote_bytes_after=$remoteAfter" in script
assert "size_stable=$stable" in script
for remote in (
    "/data/GoldHEN/ps4_neural_engine_install.log",
    "/data/GoldHEN/plugins/ps4_neural_engine.prx.pne-release.ini",
    "/user/app/PNEU00001/app.pkg",
    "/user/app/PNEU00004/app.pkg",
    "/user/data/pkg/PS4-Neural-Engine-015.pkg",
    "/data/pkg/PS4-Neural-Engine-015.pkg",
    "/data/pkg/PS4-Neural-Engine-015-7609b5d2b536.pkg",
    "/data/pkg/PS4-Neural-Engine-015-089684400316.pkg",
    "/data/pkg/PS4-Neural-Engine-006-dfda3a974df8.pkg",
    "/data/pkg/PS4-Neural-Engine-006-eb68c632848b.pkg",
    "/data/pkg/PS4-Neural-Engine-006-a3c9eb2b185e.pkg",
    "/data/pkg/PS4-Neural-Engine-006-0f0e7060fc3d.pkg",
    "/data/pkg/PS4-Neural-Engine-006-8d95e10d45e9.pkg",
):
    assert remote in script
assert 'schema="nr.ps4.control-plane-preflight.v3"' in script
assert "16777216" in script, "large files must be bounded before GET"
assert "Get-FileHash" in script
assert "started_utc=$started" in script and "completed_utc=" in script

# RPI fingerprint remains GET-only; no BinLoader payload or mutating HTTP request.
assert '$request.Method="GET"' in script
assert "POST" not in script.upper()
assert "UploadFile" not in script and "DeleteFile" not in script and "Rename" not in script
assert "CONTROL_PLANE_READ_ONLY_COMPLETE" in script
assert not Path(".github/workflows/ps4-bridge-test.yml").exists(), "legacy auto-staging workflow must stay retired"
print("PS4 control-plane collector safety and evidence-integrity contract passed")
