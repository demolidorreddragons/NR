#!/usr/bin/env python3
"""Safety contract for bounded read-only Windows self-hosted runner diagnostics."""
from pathlib import Path
script = Path("tools/collect_windows_runner_service_diagnostics.ps1").read_text(encoding="utf-8")
workflow = Path(".github/workflows/ps4-windows-runner-service-diagnostics.yml").read_text(encoding="utf-8")
assert "actions.runner.demolidorreddragons-NR.PS4-WINDOWS-BRIDGE" in script
assert "Get-Service -Name" in script
assert "sc.exe" in script and '"qc"' in script and '"queryex"' in script
assert '"qfailure"' in script and '"qsidtype"' in script
assert "Get-ItemProperty -LiteralPath $regPath" in script
assert "Get-EventLog" in script
assert "Get-Process -Name" in script
assert "Get-Acl -LiteralPath" in script
assert "mutations_performed=$false" in script
assert "services_started_or_stopped=$false" in script
assert "registry_modified=$false" in script
assert "runner_reconfigured=$false" in script
assert "ps4_state_modified=$false" in script
for forbidden in ("Start-Service", "Stop-Service", "Restart-Service", "Set-Service", "sc.exe config", "sc.exe failure", "Set-ItemProperty", "Remove-Item", "reg.exe add", "reg.exe delete", "Get-CimInstance", "Get-WinEvent", "Get-Content"):
    assert forbidden.lower() not in script.lower(), forbidden
assert "runs-on: [self-hosted, Windows, X64, ps4-lan]" in workflow
assert "GITHUB_TOKEN:" in workflow and "github.token" in workflow
assert "actions/checkout@" not in workflow
assert "timeout-minutes: 3" in workflow
assert "retention-days: 7" in workflow
assert "RUNNER_SERVICE_DIAGNOSTICS_READ_ONLY_COMPLETE" in script
print("Windows runner service diagnostics read-only contract: PASS")
