#!/usr/bin/env python3
"""Static safety contract for the PS4 control-plane read-only collector."""
from pathlib import Path
import re

script = Path("tools/ps4-control-plane-preflight.ps1").read_text(encoding="utf-8")
workflow = Path(".github/workflows/ps4-control-plane-preflight.yml").read_text(encoding="utf-8")

# Only the three explicitly read-only FTP verbs may be assigned to requests.
ftp_methods = set(re.findall(r"\[System\.Net\.WebRequestMethods\+Ftp\]::(\w+)", script))
assert ftp_methods == {"ListDirectoryDetails", "GetFileSize", "DownloadFile"}, ftp_methods
assert not re.search(r"WebRequestMethods\+Ftp\]::(?:UploadFile|DeleteFile|Rename|MakeDirectory|RemoveDirectory)", script)
assert "runs-on: [self-hosted, Windows, X64, ps4-lan]" in workflow
assert "PS4_IP: \${{ vars.PS4_IP }}" in workflow
assert "persist-credentials: false" in workflow
assert "ref: main" in workflow, "privileged runner must execute reviewed default-branch collector, not arbitrary build-branch code"
assert "github.event.workflow_run.head_sha" in workflow, "build identity must be bound independently from collector source"

# LIST evidence must remain byte-for-byte and must not be decoded/re-encoded.
assert "ListDirectoryDetails" in script
assert "$input.CopyTo($output)" in script
assert ".raw.bin" in script
assert "StreamReader" not in script
assert "WriteAllText($destination" not in script
assert "[IO.FileMode]::CreateNew" in script
assert "$output.Flush($true)" in script
assert '".partial"' in script and 'result="partial"' in script

# GET snapshots need pre/post remote size, local size/hash, and timestamp provenance.
assert script.count("GetFileSize") >= 2
assert "remote_bytes_before=$remoteBefore" in script
assert "remote_bytes_after=$remoteAfter" in script
assert "size_stable=$stable" in script
assert "Get-FileHash" in script
assert "started_utc=$started" in script and "completed_utc=" in script

# RPI fingerprint remains GET-only; no BinLoader payload or mutating HTTP request.
assert '$request.Method="GET"' in script
assert "POST" not in script.upper()
assert "UploadFile" not in script and "DeleteFile" not in script and "Rename" not in script
assert "CONTROL_PLANE_READ_ONLY_COMPLETE" in script
print("PS4 control-plane collector safety and evidence-integrity contract passed")
