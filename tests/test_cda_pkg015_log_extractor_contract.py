from pathlib import Path

workflow = Path(".github/workflows/cda-pkg015-readonly-evidence.yml").read_text(encoding="utf-8")
extractor = Path("tools/cda/Extract-Pkg015LogSignals.ps1").read_text(encoding="utf-8")

assert "Checkout reviewed log-signal extractor" in workflow
assert "uses: actions/checkout@v4" in workflow
assert "tools/cda/Extract-Pkg015LogSignals.ps1" in workflow
assert "Search preserved logs for PKG015 preflight and crash/error signals" in workflow
assert workflow.index("Search preserved logs for PKG015") < workflow.index("Preserve raw evidence artifact")
assert "workflow_dispatch:" in workflow
assert "runs-on: [self-hosted, Windows, X64, ps4-lan]" in workflow
assert "DownloadFile" in workflow and "GetFileSize" in workflow and "ListDirectoryDetails" in workflow
assert "DISCOVERED_GCN8_SMOKE_LOG_COUNT" in workflow
assert "pne_gcn8_compute_smoke_" in workflow, "workflow must discover and snapshot GCN8 smoke logs from /data listing"
for forbidden in ("UploadFile", "DeleteFile", "RenameFile", "MakeDirectory", "Install-Package"):
    assert forbidden not in workflow, f"workflow contains forbidden remote action: {forbidden}"

for signal in ("preflight", "gcn8_assets", "gnm_exports", "gnm_calls", "prx_activation", "crash_or_error"):
    assert f"Name='{signal}'" in extractor, f"missing log signal category: {signal}"
for needle in ("pne\\.log", "Loader_Logs\\.txt", "memory\\.log", "pne_gcn8_compute_smoke_[^/]+\\.log", "sce_logs/log\\.bin"):
    assert needle in extractor, f"missing log source: {needle}"
assert "FILE_SNAPSHOT remote=" in extractor, "source mapping must come from the evidence manifest"
assert "ReadAllBytes" in extractor, "extractor must read local preserved snapshots"
assert "ASCII" in extractor and "UTF16LE" in extractor, "binary string extraction must cover ASCII and UTF-16LE"
assert "log-signal-findings.json" in extractor and "log-signal-findings.txt" in extractor
assert "No proprietary log.bin decoding" in extractor, "binary extraction limitations must be explicit"
assert "WriteAllBytes" not in extractor, "extractor must not rewrite or mutate preserved raw snapshots"
print("CDA PKG 015 log-signal extractor contract: PASS")
