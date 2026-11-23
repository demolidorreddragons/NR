from pathlib import Path

audit = Path(".github/workflows/ps4-installed-app-readonly-audit.yml").read_text(encoding="utf-8")
inspect = Path(".github/workflows/ps4-inspect-installed-pkg.yml").read_text(encoding="utf-8")

assert "shell: powershell -NoProfile -ExecutionPolicy Bypass -File {0}" in audit
assert "installed-app.pkg" in audit and "installed-app.json" in audit
assert "user-data-source-pkg015.pkg" in audit and "staged-pkg015.pkg" in audit
assert "Get-FileHash" in audit
assert "READ_ONLY=true" in audit and "NO_FILES_WRITTEN_OR_DELETED_ON_PS4=true" in audit
assert "retention-days: 30" in audit

assert 'workflows: ["PS4 installed Neural Engine read-only audit"]' in inspect
assert "run-id: ${{ github.event.workflow_run.id }}" in inspect
assert "44654196789" not in inspect
assert "OrbisPkgTool" in inspect and "3de6da849b64a8ad0d5ba00dd4e8919934b868b0" in inspect
assert 'dotnet "$tool" validate "$pkg"' in inspect
assert 'dotnet "$tool" sfo read' in inspect
print("Installed-app read-only audit and exact-run inspector contract passed")
