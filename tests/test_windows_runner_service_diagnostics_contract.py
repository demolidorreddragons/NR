#!/usr/bin/env python3
"""Safety contract for read-only Windows self-hosted runner service diagnostics."""
from pathlib import Path

script = Path("tools/collect_windows_runner_service_diagnostics.ps1").read_text(encoding="utf-8")
workflow = Path(".github/workflows/ps4-windows-runner-service-diagnostics.yml").read_text(encoding="utf-8")

assert "actions.runner.demolidorreddragons-NR.PS4-WINDOWS-BRIDGE" in script
assert "Get-CimInstance Win32_Service" in script
assert "Get-ItemProperty -LiteralPath $regPath" in script
assert "Get-WinEvent" in script
assert "Get-Acl -LiteralPath" in script
assert "RunnerService.exe" in script and "Runner.Listener.exe" in script
assert "mutations_performed=$false" in script
assert "services_started_or_stopped=$false" in script
assert "registry_modified=$false" in script
assert "runner_reconfigured=$false" in script
for forbidden in ("Start-Service", "Stop-Service", "Restart-Service", "Set-Service", "sc.exe config", "sc.exe failure", "Set-ItemProperty", "Remove-Item", "reg.exe add", "reg.exe delete"):
    assert forbidden.lower() not in script.lower(), forbidden
assert "runs-on: [self-hosted, Windows, X64, ps4-lan]" in workflow
assert "GITHUB_TOKEN: \${{ github.token }}" in workflow
assert "GITHUB_SHA: \${{ github.sha }}" in workflow
assert "contents/tools/collect_windows_runner_service_diagnostics.ps1?ref=" in workflow
assert "SHA1]::Create()" in workflow and "Git blob SHA-1 mismatch" in workflow
assert "workflow_dispatch:" in workflow and "push:" in workflow
assert "runner-service-diagnostics-\${{ github.run_id }}" in workflow
assert "retention-days: 7" in workflow
assert "RUNNER_SERVICE_DIAGNOSTICS_READ_ONLY_COMPLETE" in script
assert "actions/checkout@" not in workflow
print("Windows runner service diagnostics read-only contract: PASS")
