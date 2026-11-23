#!/usr/bin/env python3
"""Static safety contract for the reversible, QA-gated NR package archive."""
from pathlib import Path
import re

script = Path("tools/archive_superseded_nr_pkgs.ps1").read_text(encoding="utf-8")
workflow = Path(".github/workflows/ps4-archive-superseded-nr-pkgs.yml").read_text(encoding="utf-8")

ftp_methods = set(re.findall(r"\[System\.Net\.WebRequestMethods\+Ftp\]::(\w+)", script))
assert ftp_methods == {"ListDirectoryDetails", "GetFileSize", "DownloadFile", "Rename"}, ftp_methods
assert "UploadFile" not in script and "DeleteFile" not in script
assert "MakeDirectory" not in script and "RemoveDirectory" not in script
assert "POST" not in script.upper()
assert "Remove-Item" not in script
assert "$request.UsePassive = $true" in script
assert "$request.UsePassive = $false" not in script
assert "$request.Timeout = 60000" in script

assert "workflow_run:" in workflow
assert 'workflows: ["PS4 Installed Package Offline Inspection"]' in workflow
assert "github.event.workflow_run.conclusion == 'success'" in workflow
assert "github.event.workflow_run.head_branch == 'main'" in workflow
assert "runs-on: [self-hosted, Windows, X64, ps4-lan]" in workflow
assert "actions/download-artifact@v4" in workflow
assert r"run-id: ${{ github.event.workflow_run.id }}" in workflow
assert "ExecutionPolicy Bypass" in workflow
assert "if: always()" in workflow
assert "archive-evidence-" + r"${{ github.run_id }}-${{ github.run_attempt }}" in workflow
assert "evidence-" + r"${{ github.run_id }}-${{ github.run_attempt }}" in workflow

assert 'nr.ps4.installed-package-inspection.v1' in script
assert 'PASS_PC_ONLY' in script
assert 'installed_matches_app_json_source' in script
assert 'staged_canonical_015' in script
assert '"/data/pkg/PS4-Neural-Engine-015.pkg"' in script
assert '"/user/app/PNEU00001/app.pkg"' in script
assert '"/user/data/pkg/PS4-Neural-Engine-015.pkg"' in script
assert '"/data/pkg/old/"' in script
assert 'ARCHIVED_AND_READBACK_VERIFIED' in script
assert 'ARCHIVE_COMPLETE_READBACK_VERIFIED' in script
assert 'third_party_root_names_unchanged' in script
assert 'Compare-Object -ReferenceObject $nonNrBefore -DifferenceObject $nonNrAfter' in script
assert 'Get-FileHash' in script
assert '$before -ne $after' in script
assert '$request.RenameTo = $destinationPath' in script
assert '[IO.FileMode]::CreateNew' in script
assert 'installation_performed = $false' in script
assert 'prx_modified = $false' in script
assert 'plugins_ini_modified = $false' in script
assert 'goldhen_restarted = $false' in script

# The archive script must never address third-party game packages or mutate GoldHEN state.
assert "/data/GoldHEN" not in script
assert "plugins.ini" not in script
assert "PNEU00004" not in script
print("PS4 superseded NR PKG archive safety contract: PASS")
