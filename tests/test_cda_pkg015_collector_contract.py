from pathlib import Path

workflow = Path(".github/workflows/cda-pkg015-readonly-evidence.yml").read_text(encoding="utf-8")
assert "workflow_dispatch:" in workflow, "CDA collector must be manually dispatched"
assert "\n  push:" not in workflow and "\n  pull_request:" not in workflow, "CDA collector must not run on push/PR"
assert "runs-on: [self-hosted, Windows, X64, ps4-lan]" in workflow, "collector must use the authorized Windows PS4 runner"
assert "PS4_IP: ${{ vars.PS4_IP }}" in workflow, "PS4 endpoint must come from Actions variable"
assert "IPAddress]::TryParse" in workflow and "AddressFamily]::InterNetwork" in workflow, "collector must validate IPv4 before network access"
assert "DownloadFile" in workflow and "ListDirectoryDetails" in workflow and "GetFileSize" in workflow, "only expected FTP read operations should be used"
for forbidden in ("UploadFile", "DeleteFile", "RenameFile", "MakeDirectory", "Install-Package", "GoldHEN restart"):
    assert forbidden not in workflow, f"forbidden remote mutation/action reference: {forbidden}"
assert "CopyTo($outputStream)" in workflow, "raw LIST/file streams must be copied byte-for-byte"
assert "StreamReader" not in workflow and "WriteAllText($dest" not in workflow, "raw evidence must not be decoded/re-encoded"
assert "CreateNew" in workflow, "local evidence must never overwrite existing files"
assert "Get-FileHash" in workflow and "SHA256" in workflow, "local evidence hashes are mandatory"
assert "snapshot-a" in workflow and "snapshot-b" in workflow, "mutable log.bin must have two separate snapshots"
assert "OPAQUE_UNDECODED" in workflow, "unknown binary log format must remain opaque"
assert "192.168.0.17" not in workflow, "workflow must not hardcode the PS4 address"
assert "EXPECTED_PKG_SHA256=2fcaf679766fefbed4bc60cc31beb0227b8bfefd4eb8070160f65a402be747e9" in workflow, "PKG 015 identity pin missing"
assert "USER_FEEDBACK_REFERENCE_UTF8_BASE64=" in workflow and "USER_OBSERVED_RESULT_UTF8_BASE64=" in workflow, "feedback provenance must be encoded, not interpolated into manifest lines"
print("CDA PKG 015 collector contract: PASS")
