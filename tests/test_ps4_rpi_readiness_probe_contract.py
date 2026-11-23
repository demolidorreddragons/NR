#!/usr/bin/env python3
"""Safety contract for the separately gated read-only RPI readiness probe."""
from pathlib import Path
script = Path("tools/ps4-rpi-readiness-probe.ps1").read_text(encoding="utf-8")
workflow = Path(".github/workflows/ps4-rpi-readiness-probe.yml").read_text(encoding="utf-8")
assert "runs-on: [self-hosted, Windows, X64, ps4-lan]" in workflow
assert "workflow_dispatch:" in workflow
assert "github.token" in workflow and "github.sha" in workflow
assert "vars.PS4_IP" in workflow
assert r"\${{" not in workflow
assert "SHA1]::Create()" in workflow and "RPI probe Git blob SHA-1 mismatch" in workflow
assert "[System.Net.IPAddress]::TryParse" in script
assert 'Invoke-Http "/api" "GET"' in script
assert 'Invoke-Http "/api/status" "GET"' in script
assert 'Invoke-Http "/api/v2/system" "GET"' in script
assert 'Invoke-Http "/api/is_exists" "POST"' in script
assert '"PNEU00001"' in script
for forbidden in ("/api/install", "/api/v2/install", "/api/uninstall", "/api/stop_task", "/api/pause_task", "/api/resume_task", "/api/start_task", "9090", "UploadFile", "DeleteFile", "Rename"):
    assert forbidden.lower() not in script.lower(), forbidden
assert "install_requests_sent=$false" in script
assert "task_control_requests_sent=$false" in script
assert "remote_files_modified=$false" in script
assert "pkg_installed=$false" in script
assert "target_ip=\"redacted\"" in script
assert "redacted; see identity_summary" in script
assert "redacted-ip" in script
assert "existsValid" in script
assert "RPI_READINESS_COMPLETE" in script
print("PS4 RPI read-only readiness safety contract: PASS")
